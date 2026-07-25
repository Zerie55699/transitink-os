#include "providers/JourneyTimeProvider.h"

#include "TransitCatalog.h"
#include "core/UiText.h"

JourneyTimeProvider::JourneyTimeProvider(JourneyTimeClient& client) : client_(client) {}

transitink::ProviderResult JourneyTimeProvider::fetch(
    uint8_t slot,
    const transitink::WidgetConfig& config,
    int64_t nowEpoch) {
    transitink::JourneyTimeRecord noRecord;
    noRecord.valid = false;
    if (config.type != transitink::WidgetType::JourneyTime ||
        !transitink::isWidgetConfigValid(config)) {
        return transitink::normalizeJourneyTimeSnapshot(slot, config, noRecord,
                                                        nowEpoch);
    }
    if (!transitink::isJourneyTimePairValid(
            config.journeyTime.locationId, config.journeyTime.destinationId)) {
        auto invalid = config;
        invalid.journeyTime.destinationId.clear();
        auto result = transitink::normalizeJourneyTimeSnapshot(
            slot, invalid, noRecord, nowEpoch);
        result.snapshot.providerMessage = "行車時間地點或目的地設定不正確";
        return result;
    }

    transitink::JourneyTimeRecord record;
    String error;
    const auto outcome =
        client_.fetchJourneyTime(config.journeyTime, record, error);
    if (outcome == JourneyTimeFetchOutcome::Matched) {
        return transitink::normalizeJourneyTimeSnapshot(slot, config, record,
                                                        nowEpoch);
    }

    auto result = transitink::normalizeJourneyTimeSnapshot(slot, config, noRecord,
                                                           nowEpoch);
    if (outcome == JourneyTimeFetchOutcome::Empty) {
        result.snapshot.providerMessage =
            transitink::uiText(transitink::UiTextId::JourneyUnavailable);
        return result;
    }
    result.outcome = transitink::ProviderOutcome::Failure;
    result.snapshot.state = transitink::WidgetState::Error;
    result.snapshot.providerMessage =
        transitink::uiText(transitink::UiTextId::JourneyUpdateFailed);
    return result;
}
