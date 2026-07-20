#include "providers/GmbProvider.h"

#include <cassert>
#include <cstdint>

namespace {

int gFetchCalls = 0;
bool gFetchResult = true;
String gFetchError;
transitink::GmbEtaPayload gPayload;

transitink::WidgetConfig validConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::GmbEta;
    config.gmb.region = "HKI";
    config.gmb.routeCode = "69";
    config.gmb.routeId = "2000410";
    config.gmb.routeSeq = "1";
    config.gmb.stopId = "20003337";
    config.gmb.stopSeq = "1";
    config.gmb.routeLabelTc = "69";
    config.gmb.stopLabelTc = "數碼港";
    config.gmb.directionLabelTc = "數碼港 往 鰂魚涌";
    return config;
}

}  // namespace

bool GmbClient::fetchEta(const transitink::GmbWidgetConfig&,
                         transitink::GmbEtaPayload& payload,
                         String& error) {
    ++gFetchCalls;
    payload = gPayload;
    error = gFetchError;
    return gFetchResult;
}

int main() {
    GmbClient client;
    GmbProvider provider(client);
    constexpr int64_t now = 2000000000;

    auto invalid = validConfig();
    invalid.gmb.stopId.clear();
    const auto invalidResult = provider.fetch(0, invalid, now);
    assert(invalidResult.outcome == transitink::ProviderOutcome::InvalidConfig);
    assert(gFetchCalls == 0);

    const auto clockResult = provider.fetch(0, validConfig(), 0);
    assert(clockResult.outcome == transitink::ProviderOutcome::ClockUnsynced);
    assert(gFetchCalls == 0);

    gPayload = {};
    gPayload.records = {{0, ""}, {6, "未開出"}};
    const auto success = provider.fetch(2, validConfig(), now);
    assert(success.outcome == transitink::ProviderOutcome::Success);
    assert(success.snapshot.valueCount == 2);
    assert(success.snapshot.values[0].text == "即將到站");
    assert(success.snapshot.values[1].text == "6 分鐘");
    assert(gFetchCalls == 1);

    gFetchResult = false;
    gFetchError = "測試連線失敗";
    const auto failure = provider.fetch(2, validConfig(), now);
    assert(failure.outcome == transitink::ProviderOutcome::Failure);
    assert(failure.snapshot.state == transitink::WidgetState::Error);
    assert(failure.snapshot.providerMessage == "測試連線失敗");
    assert(gFetchCalls == 2);

    gFetchResult = true;
    gFetchError = "";
    gPayload = {};
    gPayload.enabled = false;
    gPayload.descriptionTc = "服務暫停";
    const auto disabled = provider.fetch(2, validConfig(), now);
    assert(disabled.outcome == transitink::ProviderOutcome::Empty);
    assert(disabled.snapshot.providerMessage == "服務暫停");
    assert(gFetchCalls == 3);
    return 0;
}
