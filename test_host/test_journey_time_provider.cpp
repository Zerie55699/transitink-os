#include <cassert>
#include <cstdint>
#include <string>

#include "JourneyTimeClient.h"
#include "providers/JourneyTimeProvider.h"

namespace {

int gFetchCalls = 0;
JourneyTimeFetchOutcome gFetchOutcome = JourneyTimeFetchOutcome::Failure;
transitink::JourneyTimeRecord gRecord;

transitink::WidgetConfig journeyConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::JourneyTime;
    config.journeyTime.locationId = "K07";
    config.journeyTime.destinationId = "ATSCA";
    config.journeyTime.locationLabelTc = "西九龍公路西行近港鐵南昌站";
    config.journeyTime.destinationLabelTc = "機場經八號幹線";
    return config;
}

}  // namespace

JourneyTimeFetchOutcome JourneyTimeClient::fetchJourneyTime(
    const transitink::JourneyTimeWidgetConfig&,
    transitink::JourneyTimeRecord& record,
    String& error) {
    ++gFetchCalls;
    record = gRecord;
    error = "不可顯示的後端訊息";
    return gFetchOutcome;
}

int main() {
    JourneyTimeClient client;
    JourneyTimeProvider provider(client);

    auto incomplete = journeyConfig();
    incomplete.journeyTime.locationId.clear();
    auto result = provider.fetch(0, incomplete, 0);
    assert(result.outcome == transitink::ProviderOutcome::InvalidConfig);
    assert(result.snapshot.providerMessage == "設定不完整");
    assert(gFetchCalls == 0);

    auto invalidPair = journeyConfig();
    invalidPair.journeyTime.destinationId = "CH";
    invalidPair.journeyTime.destinationLabelTc = "紅磡海底隧道";
    result = provider.fetch(0, invalidPair, 2'000'000'000);
    assert(result.outcome == transitink::ProviderOutcome::InvalidConfig);
    assert(result.snapshot.providerMessage == "行車時間地點或目的地設定不正確");
    assert(gFetchCalls == 0);

    gRecord = {"K07", "ATSCA", 7, 2, 1'709'222'399, true};
    gFetchOutcome = JourneyTimeFetchOutcome::Matched;
    result = provider.fetch(1, journeyConfig(), 0);
    assert(gFetchCalls == 1);
    assert(result.outcome == transitink::ProviderOutcome::Success);
    assert(result.snapshot.values[0].text == "7 分鐘");
    assert(result.snapshot.values[0].context == "行車緩慢");
    assert(result.snapshot.dataAtEpoch == 1'709'222'399);

    gFetchOutcome = JourneyTimeFetchOutcome::Empty;
    result = provider.fetch(2, journeyConfig(), 0);
    assert(result.outcome == transitink::ProviderOutcome::Empty);
    assert(result.snapshot.providerMessage == "暫未能取得行車時間");

    gFetchOutcome = JourneyTimeFetchOutcome::Failure;
    result = provider.fetch(3, journeyConfig(), 0);
    assert(result.outcome == transitink::ProviderOutcome::Failure);
    assert(result.snapshot.state == transitink::WidgetState::Error);
    assert(result.snapshot.providerMessage == "未能更新行車時間");
    assert(result.snapshot.providerMessage != "不可顯示的後端訊息");
    return 0;
}
