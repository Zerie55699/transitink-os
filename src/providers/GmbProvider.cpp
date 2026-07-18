#include "providers/GmbProvider.h"

GmbProvider::GmbProvider(GmbClient& client) : client_(client) {}

transitink::ProviderResult GmbProvider::fetch(
    uint8_t slot,
    const transitink::WidgetConfig& config,
    int64_t nowEpoch) {
    auto baseline = transitink::normalizeGmbSnapshot(slot, config, {}, nowEpoch);
    if (baseline.outcome == transitink::ProviderOutcome::InvalidConfig ||
        baseline.outcome == transitink::ProviderOutcome::ClockUnsynced) {
        return baseline;
    }

    transitink::GmbEtaPayload payload;
    String error;
    if (!client_.fetchEta(config.gmb, payload, error)) {
        baseline.outcome = transitink::ProviderOutcome::Failure;
        baseline.snapshot.state = transitink::WidgetState::Error;
        baseline.snapshot.providerMessage =
            error.length() == 0 ? "未能更新專線小巴到站時間" : error.c_str();
        return baseline;
    }
    return transitink::normalizeGmbSnapshot(slot, config, payload, nowEpoch);
}
