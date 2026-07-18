#include "core/WidgetCore.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace transitink {
namespace {

constexpr const char* kInvalidConfigMessage = "設定不完整";
constexpr const char* kEmptyMessage = "暫無班次";
constexpr const char* kClockUnsyncedMessage = "時間尚未同步";
constexpr const char* kJourneyUnavailableMessage = "暫未能取得行車時間";

std::string joinNonEmpty(std::initializer_list<std::string> parts) {
    std::string result;
    for (const auto& part : parts) {
        if (part.empty()) continue;
        if (!result.empty()) result += " · ";
        result += part;
    }
    return result;
}

bool isStopCodeToken(const std::string& value) {
    if (value.size() < 3 || value.size() > 32) return false;
    bool hasDigit = false;
    for (const unsigned char character : value) {
        const bool isDigit = character >= '0' && character <= '9';
        const bool isLetter = (character >= 'A' && character <= 'Z') ||
                              (character >= 'a' && character <= 'z');
        if (!isDigit && !isLetter && character != '-' && character != '_') return false;
        hasDigit = hasDigit || isDigit;
    }
    return hasDigit;
}

std::string withoutTrailingStopCode(std::string label) {
    const std::size_t lastContent = label.find_last_not_of(" \t\r\n");
    if (lastContent == std::string::npos) return {};
    label.resize(lastContent + 1);

    std::size_t openPosition = std::string::npos;
    std::size_t openSize = 0;
    std::size_t closeSize = 0;
    if (!label.empty() && label.back() == ')') {
        openPosition = label.rfind('(');
        openSize = 1;
        closeSize = 1;
    } else if (label.size() >= 3 && label.compare(label.size() - 3, 3, "）") == 0) {
        openPosition = label.rfind("（");
        openSize = 3;
        closeSize = 3;
    }
    if (openPosition == std::string::npos) return label;

    const std::size_t tokenStart = openPosition + openSize;
    const std::string token = label.substr(tokenStart, label.size() - closeSize - tokenStart);
    if (!isStopCodeToken(token)) return label;

    label.resize(openPosition);
    const std::size_t stationEnd = label.find_last_not_of(" \t\r\n");
    if (stationEnd == std::string::npos) return {};
    label.resize(stationEnd + 1);
    return label;
}

std::string countdownText(int64_t eventEpoch, int64_t nowEpoch) {
    const int64_t seconds = eventEpoch - nowEpoch;
    const int64_t minutes = seconds / 60 + (seconds % 60 == 0 ? 0 : 1);
    return std::to_string(minutes) + " 分鐘";
}

WidgetSnapshot baseSnapshot(uint8_t slot,
                            const WidgetConfig& config,
                            int64_t fetchedAtEpoch,
                            int64_t dataAtEpoch) {
    WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = config.type;
    snapshot.fetchedAtEpoch = fetchedAtEpoch;
    snapshot.dataAtEpoch = dataAtEpoch;

    switch (config.type) {
        case WidgetType::BusEta:
            snapshot.title = joinNonEmpty(
                {config.bus.routeLabelTc, config.bus.destinationLabelTc});
            snapshot.subtitle = displayStopLabelTc(config.bus.stopLabelTc);
            break;
        case WidgetType::GmbEta:
            snapshot.title = joinNonEmpty(
                {config.gmb.routeLabelTc, config.gmb.directionLabelTc});
            snapshot.subtitle = config.gmb.stopLabelTc;
            break;
        case WidgetType::MtrEta:
            snapshot.title = joinNonEmpty(
                {config.mtr.lineOrRouteLabelTc, config.mtr.directionLabelTc});
            snapshot.subtitle = config.mtr.stationLabelTc;
            break;
        case WidgetType::JourneyTime:
            snapshot.title = config.journeyTime.locationLabelTc;
            snapshot.subtitle = config.journeyTime.destinationLabelTc;
            break;
        case WidgetType::Disabled:
            break;
    }
    return snapshot;
}

ProviderResult errorResult(uint8_t slot,
                           const WidgetConfig& config,
                           int64_t nowEpoch,
                           ProviderOutcome outcome,
                           const char* message) {
    auto snapshot = baseSnapshot(slot, config, nowEpoch, 0);
    snapshot.state = WidgetState::Error;
    snapshot.providerMessage = message;
    return {outcome, std::move(snapshot)};
}

ProviderResult emptyResult(WidgetSnapshot snapshot,
                           const std::string& message = kEmptyMessage) {
    snapshot.state = WidgetState::Empty;
    snapshot.providerMessage = message;
    return {ProviderOutcome::Empty, std::move(snapshot)};
}

bool sameValue(const WidgetValue& left, const WidgetValue& right) {
    return left.eventEpoch == right.eventEpoch && left.text == right.text &&
           left.context == right.context;
}

void appendUnique(std::vector<WidgetValue>& values, WidgetValue value) {
    const bool exists = std::any_of(values.begin(), values.end(), [&](const WidgetValue& other) {
        return sameValue(value, other);
    });
    if (!exists) values.push_back(std::move(value));
}

void storeFirstTwo(WidgetSnapshot& snapshot, std::vector<WidgetValue>& values) {
    std::stable_sort(values.begin(), values.end(), [](const WidgetValue& left,
                                                       const WidgetValue& right) {
        return left.eventEpoch < right.eventEpoch;
    });
    snapshot.valueCount = std::min(values.size(), snapshot.values.size());
    for (std::size_t index = 0; index < snapshot.valueCount; ++index) {
        snapshot.values[index] = std::move(values[index]);
    }
}

std::string journeyContext(int8_t colourId) {
    switch (colourId) {
        case 1:
            return "交通擠塞";
        case 2:
            return "行車緩慢";
        case 3:
            return "交通暢順";
        default:
            return {};
    }
}

bool busRecordMatches(const BusEtaRecord& record, const BusWidgetConfig& config) {
    return record.operatorId == config.operatorId && record.routeId == config.routeId &&
           record.directionId == config.directionId &&
           (record.serviceType.empty() || record.serviceType == config.serviceType);
}

bool railRecordMatches(const RailArrivalRecord& record, const MtrWidgetConfig& config) {
    return record.mode == config.mode && record.lineOrRouteId == config.lineOrRouteId &&
           record.stationId == config.stationId && record.directionId == config.directionId;
}

}  // namespace

std::string displayStopLabelTc(std::string label) {
    return withoutTrailingStopCode(std::move(label));
}

uint32_t refreshIntervalMs(WidgetType type) {
    switch (type) {
        case WidgetType::BusEta:
            return 60000;
        case WidgetType::GmbEta:
            return 60000;
        case WidgetType::MtrEta:
            return 30000;
        case WidgetType::JourneyTime:
            return 120000;
        case WidgetType::Disabled:
            return 0;
    }
    return 0;
}

uint32_t staleWindowSeconds(WidgetType type) {
    switch (type) {
        case WidgetType::BusEta:
            return 180;
        case WidgetType::GmbEta:
            return 180;
        case WidgetType::MtrEta:
            return 90;
        case WidgetType::JourneyTime:
            return 360;
        case WidgetType::Disabled:
            return 0;
    }
    return 0;
}

bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
    return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

void removeExpiredValues(WidgetSnapshot& snapshot, int64_t nowEpoch) {
    std::size_t writeIndex = 0;
    for (std::size_t readIndex = 0; readIndex < snapshot.valueCount; ++readIndex) {
        const auto& value = snapshot.values[readIndex];
        if (value.eventEpoch > 0 && value.eventEpoch <= nowEpoch) continue;
        if (writeIndex != readIndex) snapshot.values[writeIndex] = value;
        ++writeIndex;
    }
    for (std::size_t index = writeIndex; index < snapshot.values.size(); ++index) {
        snapshot.values[index] = {};
    }
    snapshot.valueCount = writeIndex;
}

ProviderResult normalizeBusSnapshot(uint8_t slot,
                                    const WidgetConfig& config,
                                    const std::vector<BusEtaRecord>& records,
                                    int64_t nowEpoch) {
    if (config.type != WidgetType::BusEta || !isWidgetConfigValid(config)) {
        return errorResult(slot, config, nowEpoch, ProviderOutcome::InvalidConfig,
                           kInvalidConfigMessage);
    }
    if (nowEpoch <= 0) {
        return errorResult(slot, config, nowEpoch, ProviderOutcome::ClockUnsynced,
                           kClockUnsyncedMessage);
    }

    auto snapshot = baseSnapshot(slot, config, nowEpoch, nowEpoch);
    std::vector<WidgetValue> values;
    values.reserve(records.size());
    for (const auto& record : records) {
        if (record.cancelled || record.eventEpoch <= nowEpoch ||
            !busRecordMatches(record, config.bus)) {
            continue;
        }
        const std::string destination = record.destinationLabelTc.empty()
                                            ? config.bus.destinationLabelTc
                                            : record.destinationLabelTc;
        appendUnique(values,
                     {countdownText(record.eventEpoch, nowEpoch),
                      joinNonEmpty({destination, record.remarkTc}), record.eventEpoch});
    }
    storeFirstTwo(snapshot, values);
    if (snapshot.valueCount == 0) return emptyResult(std::move(snapshot));
    snapshot.state = WidgetState::Ready;
    return {ProviderOutcome::Success, std::move(snapshot)};
}

ProviderResult normalizeGmbSnapshot(uint8_t slot,
                                    const WidgetConfig& config,
                                    const GmbEtaPayload& payload,
                                    int64_t nowEpoch) {
    if (config.type != WidgetType::GmbEta || !isWidgetConfigValid(config)) {
        return errorResult(slot, config, nowEpoch, ProviderOutcome::InvalidConfig,
                           kInvalidConfigMessage);
    }
    if (nowEpoch <= 0) {
        return errorResult(slot, config, nowEpoch, ProviderOutcome::ClockUnsynced,
                           kClockUnsyncedMessage);
    }

    auto snapshot = baseSnapshot(slot, config, nowEpoch, nowEpoch);
    if (!payload.enabled) {
        return emptyResult(std::move(snapshot),
                           payload.descriptionTc.empty() ? "到站預報暫停"
                                                         : payload.descriptionTc);
    }

    std::vector<WidgetValue> values;
    values.reserve(payload.records.size());
    for (const auto& record : payload.records) {
        if (record.diffMinutes < 0) continue;
        const int64_t visibleMinutes = record.diffMinutes == 0 ? 1 : record.diffMinutes;
        const int64_t eventEpoch = nowEpoch + visibleMinutes * 60;
        appendUnique(values,
                     {record.diffMinutes == 0
                          ? "即將到站"
                          : std::to_string(record.diffMinutes) + " 分鐘",
                      record.remarkTc, eventEpoch});
    }
    storeFirstTwo(snapshot, values);
    if (snapshot.valueCount == 0) return emptyResult(std::move(snapshot));
    snapshot.state = WidgetState::Ready;
    return {ProviderOutcome::Success, std::move(snapshot)};
}

ProviderResult normalizeRailSnapshot(uint8_t slot,
                                     const WidgetConfig& config,
                                     const std::vector<RailArrivalRecord>& records,
                                     int64_t dataEpoch,
                                     int64_t nowEpoch) {
    if (config.type != WidgetType::MtrEta || !isWidgetConfigValid(config)) {
        return errorResult(slot, config, nowEpoch, ProviderOutcome::InvalidConfig,
                           kInvalidConfigMessage);
    }
    if (nowEpoch <= 0) {
        return errorResult(slot, config, nowEpoch, ProviderOutcome::ClockUnsynced,
                           kClockUnsyncedMessage);
    }

    auto snapshot = baseSnapshot(slot, config, nowEpoch, dataEpoch);
    std::vector<WidgetValue> values;
    values.reserve(records.size());
    for (const auto& record : records) {
        if (!record.valid || record.cancelled || record.eventEpoch <= nowEpoch ||
            !railRecordMatches(record, config.mtr)) {
            continue;
        }
        const std::string destination = record.destinationLabelTc.empty()
                                            ? config.mtr.directionLabelTc
                                            : record.destinationLabelTc;
        appendUnique(values,
                     {countdownText(record.eventEpoch, nowEpoch),
                      joinNonEmpty(
                          {destination, record.platformLabelTc, record.messageTc}),
                      record.eventEpoch});
    }
    storeFirstTwo(snapshot, values);
    if (snapshot.valueCount == 0) return emptyResult(std::move(snapshot));
    snapshot.state = WidgetState::Ready;
    return {ProviderOutcome::Success, std::move(snapshot)};
}

ProviderResult normalizeJourneyTimeSnapshot(uint8_t slot,
                                            const WidgetConfig& config,
                                            const JourneyTimeRecord& record,
                                            int64_t nowEpoch) {
    if (config.type != WidgetType::JourneyTime || !isWidgetConfigValid(config)) {
        return errorResult(slot, config, nowEpoch, ProviderOutcome::InvalidConfig,
                           kInvalidConfigMessage);
    }

    auto snapshot = baseSnapshot(slot, config, nowEpoch, record.dataEpoch);
    if (!record.valid || record.locationId != config.journeyTime.locationId ||
        record.destinationId != config.journeyTime.destinationId || record.dataEpoch <= 0) {
        return emptyResult(std::move(snapshot));
    }
    if (record.valueKind == JourneyTimeValueKind::Unavailable) {
        snapshot.state = WidgetState::Empty;
        snapshot.providerMessage = kJourneyUnavailableMessage;
        return {ProviderOutcome::Empty, std::move(snapshot)};
    }
    std::string valueText;
    if (record.valueKind == JourneyTimeValueKind::Minutes) {
        valueText = std::to_string(record.minutes) + " 分鐘";
    } else if (record.statusCode == 1) {
        valueText = "交通擠塞";
    } else if (record.statusCode == 3) {
        valueText = "隧道封閉";
    } else {
        snapshot.state = WidgetState::Empty;
        snapshot.providerMessage = kJourneyUnavailableMessage;
        return {ProviderOutcome::Empty, std::move(snapshot)};
    }
    snapshot.values[0] = {std::move(valueText), journeyContext(record.colourId), 0};
    snapshot.valueCount = 1;
    snapshot.state = WidgetState::Ready;
    return {ProviderOutcome::Success, std::move(snapshot)};
}

}  // namespace transitink
