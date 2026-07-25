#include "providers/BusProvider.h"

#include <string>
#include <vector>

BusProvider::BusProvider(KmbClient& kmb, CitybusClient& citybus,
                         TflClient& tfl)
    : kmb_(kmb), citybus_(citybus), tfl_(tfl) {}

transitink::ProviderResult BusProvider::fetch(
    uint8_t slot,
    const transitink::WidgetConfig& config,
    int64_t nowEpoch) {
    auto baseline = transitink::normalizeBusSnapshot(slot, config, {}, nowEpoch);
    if (baseline.outcome == transitink::ProviderOutcome::InvalidConfig ||
        baseline.outcome == transitink::ProviderOutcome::ClockUnsynced) {
        return baseline;
    }

    std::vector<transitink::BusEtaRecord> records;
    String error;
    bool fetched = false;
    switch (config.bus.operatorId) {
        case transitink::BusOperator::Kmb:
            fetched = kmb_.fetchEtaRecords(config.bus, records, error);
            break;
        case transitink::BusOperator::LongWin:
            fetched = kmb_.fetchEtaRecords(config.bus, records, error);
            break;
        case transitink::BusOperator::Citybus:
            fetched = citybus_.fetchEtaRecords(config.bus, records, error);
            break;
        case transitink::BusOperator::Tfl:
            fetched =
                tfl_.fetchEtaRecords(config.bus, nowEpoch, records, error);
            break;
        default:
            error = "巴士營辦商設定不正確";
            break;
    }

    if (!fetched) {
        baseline.outcome = transitink::ProviderOutcome::Failure;
        baseline.snapshot.state = transitink::WidgetState::Error;
        baseline.snapshot.providerMessage =
            error.length() == 0 ? "未能更新巴士到站時間" : error.c_str();
        return baseline;
    }
    return transitink::normalizeBusSnapshot(slot, config, records, nowEpoch);
}
