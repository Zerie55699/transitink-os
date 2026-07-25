#include "providers/TflRailProvider.h"

#include <vector>

TflRailProvider::TflRailProvider(TflClient& client) : client_(client) {}

transitink::ProviderResult TflRailProvider::fetch(
    uint8_t slot,
    const transitink::WidgetConfig& config,
    int64_t nowEpoch) {
    if (config.type != transitink::WidgetType::MtrEta ||
        !transitink::isWidgetConfigValid(config)) {
        return transitink::normalizeRailSnapshot(slot, config, {}, 0, nowEpoch);
    }
    if (config.mtr.mode != transitink::RailMode::LondonRail) {
        auto incomplete = config;
        incomplete.mtr.directionId.clear();
        auto result =
            transitink::normalizeRailSnapshot(slot, incomplete, {}, 0, nowEpoch);
        result.snapshot.providerMessage = "倫敦鐵路網絡設定不正確";
        return result;
    }

    auto baseline =
        transitink::normalizeRailSnapshot(slot, config, {}, 0, nowEpoch);
    if (baseline.outcome == transitink::ProviderOutcome::ClockUnsynced) {
        return baseline;
    }

    std::vector<transitink::RailArrivalRecord> records;
    String error;
    if (!client_.fetchRailArrivals(config.mtr, nowEpoch, records, error)) {
        baseline.outcome = transitink::ProviderOutcome::Failure;
        baseline.snapshot.state = transitink::WidgetState::Error;
        baseline.snapshot.providerMessage =
            error.length() == 0 ? "未能更新倫敦鐵路到站時間" : error.c_str();
        return baseline;
    }

    return transitink::normalizeRailSnapshot(slot, config, records, nowEpoch,
                                             nowEpoch);
}
