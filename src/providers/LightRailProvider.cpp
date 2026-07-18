#include "providers/LightRailProvider.h"

#include "TransitCatalog.h"

#include <vector>

LightRailProvider::LightRailProvider(LightRailClient& client) : client_(client) {}

transitink::ProviderResult LightRailProvider::fetch(
    uint8_t slot,
    const transitink::WidgetConfig& config,
    int64_t nowEpoch) {
    if (config.type != transitink::WidgetType::MtrEta ||
        !transitink::isWidgetConfigValid(config)) {
        return transitink::normalizeRailSnapshot(slot, config, {}, 0, nowEpoch);
    }
    const auto invalidConfig = [&](const char* message) {
        auto incomplete = config;
        incomplete.mtr.directionId.clear();
        auto result =
            transitink::normalizeRailSnapshot(slot, incomplete, {}, 0, nowEpoch);
        result.snapshot.providerMessage = message;
        return result;
    };
    if (config.mtr.mode != transitink::RailMode::LightRail) {
        return invalidConfig("輕鐵網絡設定不正確");
    }
    if (transitink::findTransitCatalogStation(
            transitink::RailMode::LightRail, config.mtr.lineOrRouteId,
            config.mtr.stationId) == nullptr) {
        return invalidConfig("輕鐵路綫或車站設定不正確");
    }
    if (transitink::findTransitCatalogDirection(
            transitink::RailMode::LightRail, config.mtr.lineOrRouteId,
            config.mtr.directionId) == nullptr) {
        return invalidConfig("輕鐵方向設定不正確");
    }

    auto baseline =
        transitink::normalizeRailSnapshot(slot, config, {}, 0, nowEpoch);
    if (baseline.outcome == transitink::ProviderOutcome::ClockUnsynced) return baseline;

    std::vector<transitink::RailArrivalRecord> records;
    int64_t dataEpoch = 0;
    String error;
    if (!client_.fetchArrivals(config.mtr, nowEpoch, records, dataEpoch, error)) {
        baseline.outcome = transitink::ProviderOutcome::Failure;
        baseline.snapshot.state = transitink::WidgetState::Error;
        baseline.snapshot.providerMessage =
            error.length() == 0 ? "未能更新輕鐵到站時間" : error.c_str();
        return baseline;
    }

    return transitink::normalizeRailSnapshot(slot, config, records, dataEpoch,
                                             nowEpoch);
}
