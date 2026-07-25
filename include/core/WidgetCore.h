#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/WidgetConfigCore.h"

namespace transitink {

enum class WidgetState : uint8_t { Ready, Empty, Error };
enum class Freshness : uint8_t { Fresh, Stale };
enum class ProviderOutcome : uint8_t { Success, Empty, InvalidConfig, ClockUnsynced, Failure };

struct WidgetValue {
    std::string text, context;
    int64_t eventEpoch = 0;
};

struct WidgetSnapshot {
    uint8_t slot = 0;
    WidgetType type = WidgetType::Disabled;
    std::string title, subtitle;
    std::array<WidgetValue, 2> values{};
    std::size_t valueCount = 0;
    WidgetState state = WidgetState::Empty;
    std::string providerMessage;
    int64_t fetchedAtEpoch = 0, dataAtEpoch = 0;
    Freshness freshness = Freshness::Fresh;
    uint8_t consecutiveFailures = 0;
};

using WidgetSnapshotSet = std::array<WidgetSnapshot, kWidgetSlotCount>;
using WidgetPageSnapshotSet = std::array<WidgetSnapshot, kWidgetsPerPage>;

WidgetSnapshot configuredWidgetSnapshot(uint8_t slot, const WidgetConfig& config);
WidgetPageSnapshotSet snapshotsForWidgetPage(const WidgetSnapshotSet& snapshots,
                                             std::size_t page);

struct ProviderResult {
    ProviderOutcome outcome;
    WidgetSnapshot snapshot;
};

struct BusEtaRecord {
    BusOperator operatorId = BusOperator::Kmb;
    std::string routeId, directionId, serviceType;
    int64_t eventEpoch = 0;
    std::string destinationLabelTc, remarkTc;
    bool cancelled = false;
    std::string destinationLabelEn{}, remarkEn{};
};

struct GmbEtaRecord {
    int32_t diffMinutes = -1;
    std::string remarkTc;
    std::string remarkEn{};
};

struct GmbEtaPayload {
    bool enabled = true;
    std::string descriptionTc;
    std::string descriptionEn{};
    std::vector<GmbEtaRecord> records;
};

struct RailArrivalRecord {
    RailMode mode = RailMode::HeavyRail;
    std::string lineOrRouteId, stationId, directionId;
    int64_t eventEpoch = 0;
    std::string destinationLabelTc, platformLabelTc, messageTc;
    bool cancelled = false;
    bool valid = true;
    std::string destinationLabelEn{}, platformLabelEn{}, messageEn{};
};

enum class JourneyTimeValueKind : uint8_t { Minutes, Status, Unavailable };

struct JourneyTimeRecord {
    std::string locationId, destinationId;
    uint16_t minutes = 0;
    int8_t colourId = -1;
    int64_t dataEpoch = 0;
    bool valid = true;
    JourneyTimeValueKind valueKind = JourneyTimeValueKind::Minutes;
    int16_t statusCode = 0;
};

uint32_t refreshIntervalMs(WidgetType type);
uint32_t refreshIntervalMs(const WidgetConfig& config);
uint32_t staleWindowSeconds(WidgetType type);
uint32_t staleWindowSeconds(const WidgetConfig& config);
bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs);
void removeExpiredValues(WidgetSnapshot& snapshot, int64_t nowEpoch);
std::string displayStopLabel(std::string label);
std::string displayStopLabelTc(std::string label);

ProviderResult normalizeBusSnapshot(uint8_t slot,
                                    const WidgetConfig& config,
                                    const std::vector<BusEtaRecord>& records,
                                    int64_t nowEpoch);
ProviderResult normalizeGmbSnapshot(uint8_t slot,
                                    const WidgetConfig& config,
                                    const GmbEtaPayload& payload,
                                    int64_t nowEpoch);
ProviderResult normalizeRailSnapshot(uint8_t slot,
                                     const WidgetConfig& config,
                                     const std::vector<RailArrivalRecord>& records,
                                     int64_t dataEpoch,
                                     int64_t nowEpoch);
ProviderResult normalizeJourneyTimeSnapshot(uint8_t slot,
                                            const WidgetConfig& config,
                                            const JourneyTimeRecord& record,
                                            int64_t nowEpoch);

}  // namespace transitink
