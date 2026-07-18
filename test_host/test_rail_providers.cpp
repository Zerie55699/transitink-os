#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "LightRailClient.h"
#include "MtrClient.h"
#include "providers/LightRailProvider.h"
#include "providers/MtrProvider.h"

namespace {

int gMtrFetchCalls = 0;
int gLightRailFetchCalls = 0;

transitink::WidgetConfig heavyConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::MtrEta;
    config.mtr.mode = transitink::RailMode::HeavyRail;
    config.mtr.lineOrRouteId = "TWL";
    config.mtr.stationId = "TSW";
    config.mtr.directionId = "UP";
    config.mtr.lineOrRouteLabelTc = "荃灣綫";
    config.mtr.stationLabelTc = "荃灣";
    config.mtr.directionLabelTc = "荃灣方向";
    return config;
}

transitink::WidgetConfig lightRailConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::MtrEta;
    config.mtr.mode = transitink::RailMode::LightRail;
    config.mtr.lineOrRouteId = "610";
    config.mtr.stationId = "600";
    config.mtr.directionId = "1";
    config.mtr.lineOrRouteLabelTc = "610";
    config.mtr.stationLabelTc = "元朗";
    config.mtr.directionLabelTc = "屯門碼頭方向";
    return config;
}

void assertInvalidConfig(const transitink::ProviderResult& result,
                         const char* expectedMessage) {
    assert(result.outcome == transitink::ProviderOutcome::InvalidConfig);
    assert(result.snapshot.state == transitink::WidgetState::Error);
    assert(result.snapshot.providerMessage == expectedMessage);
}

void testMtrProviderRejectsInvalidSelectionsBeforeClockAndClient() {
    MtrClient client;
    MtrProvider provider(client);

    for (int64_t nowEpoch : {int64_t{0}, int64_t{2000000000}}) {
        auto wrongMode = heavyConfig();
        wrongMode.mtr.mode = transitink::RailMode::LightRail;
        assertInvalidConfig(provider.fetch(0, wrongMode, nowEpoch),
                            "港鐵網絡設定不正確");

        auto invalidStation = heavyConfig();
        invalidStation.mtr.stationId = "BAD";
        assertInvalidConfig(provider.fetch(0, invalidStation, nowEpoch),
                            "港鐵路綫或車站設定不正確");

        auto invalidDirection = heavyConfig();
        invalidDirection.mtr.directionId = "SIDEWAYS";
        assertInvalidConfig(provider.fetch(0, invalidDirection, nowEpoch),
                            "港鐵方向設定不正確");
    }
    assert(gMtrFetchCalls == 0);
}

void testLightRailProviderRejectsInvalidSelectionsBeforeClockAndClient() {
    LightRailClient client;
    LightRailProvider provider(client);

    for (int64_t nowEpoch : {int64_t{0}, int64_t{2000000000}}) {
        auto wrongMode = lightRailConfig();
        wrongMode.mtr.mode = transitink::RailMode::HeavyRail;
        assertInvalidConfig(provider.fetch(1, wrongMode, nowEpoch),
                            "輕鐵網絡設定不正確");

        auto invalidStation = lightRailConfig();
        invalidStation.mtr.stationId = "999";
        assertInvalidConfig(provider.fetch(1, invalidStation, nowEpoch),
                            "輕鐵路綫或車站設定不正確");

        auto invalidDirection = lightRailConfig();
        invalidDirection.mtr.directionId = "999";
        assertInvalidConfig(provider.fetch(1, invalidDirection, nowEpoch),
                            "輕鐵方向設定不正確");
    }
    assert(gLightRailFetchCalls == 0);
}

}  // namespace

bool MtrClient::fetchArrivals(
    const transitink::MtrWidgetConfig&,
    std::vector<transitink::RailArrivalRecord>&,
    int64_t&,
    String&) {
    ++gMtrFetchCalls;
    return false;
}

bool LightRailClient::fetchArrivals(
    const transitink::MtrWidgetConfig&,
    int64_t,
    std::vector<transitink::RailArrivalRecord>&,
    int64_t&,
    String&) {
    ++gLightRailFetchCalls;
    return false;
}

int main() {
    testMtrProviderRejectsInvalidSelectionsBeforeClockAndClient();
    testLightRailProviderRejectsInvalidSelectionsBeforeClockAndClient();
    return 0;
}
