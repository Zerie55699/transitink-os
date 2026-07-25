#include "core/WidgetCore.h"
#include "core/UiText.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace {

transitink::WidgetConfig busConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::BusEta;
    config.bus.operatorId = transitink::BusOperator::Citybus;
    config.bus.routeId = "11";
    config.bus.directionId = "I";
    config.bus.serviceType = "1";
    config.bus.stopId = "STOP-A";
    config.bus.routeLabelTc = "11";
    config.bus.stopLabelTc = "中環碼頭（TW515）";
    config.bus.destinationLabelTc = "中環";
    config.bus.routeLabelEn = "11";
    config.bus.stopLabelEn = "Central Pier (TW515)";
    config.bus.destinationLabelEn = "Central";
    return config;
}

transitink::WidgetConfig railConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::MtrEta;
    config.mtr.mode = transitink::RailMode::HeavyRail;
    config.mtr.lineOrRouteId = "TML";
    config.mtr.stationId = "YUL";
    config.mtr.directionId = "UP";
    config.mtr.lineOrRouteLabelTc = "屯馬綫";
    config.mtr.stationLabelTc = "元朗";
    config.mtr.directionLabelTc = "往屯門";
    config.mtr.lineOrRouteLabelEn = "Tuen Ma Line";
    config.mtr.stationLabelEn = "Yuen Long";
    config.mtr.directionLabelEn = "Tuen Mun bound";
    return config;
}

transitink::WidgetConfig gmbConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::GmbEta;
    config.gmb.region = "HKI";
    config.gmb.routeCode = "69";
    config.gmb.routeId = "2000410";
    config.gmb.routeSeq = "1";
    config.gmb.stopId = "20003337";
    config.gmb.stopSeq = "1";
    config.gmb.routeLabelTc = "專線小巴 69";
    config.gmb.stopLabelTc = "數碼港公共運輸交匯處";
    config.gmb.directionLabelTc = "往鰂魚涌";
    return config;
}

transitink::WidgetConfig journeyConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::JourneyTime;
    config.journeyTime.locationId = "H1";
    config.journeyTime.destinationId = "K1";
    config.journeyTime.locationLabelTc = "紅磡";
    config.journeyTime.destinationLabelTc = "九龍灣";
    return config;
}

}  // namespace

int main() {
    using namespace transitink;

    static_assert(std::is_same_v<decltype(JourneyTimeRecord{}.minutes), uint16_t>);
    static_assert(std::is_same_v<decltype(JourneyTimeRecord{}.colourId), int8_t>);
    static_assert(std::is_same_v<decltype(JourneyTimeRecord{}.statusCode), int16_t>);

    const int64_t now = 2'000'000'000;
    const BusEtaRecord busContract{BusOperator::Citybus, "11", "I", "", now + 60,
                                   "中環", "原定班次", false};
    const RailArrivalRecord railContract{RailMode::LightRail, "610", "YUL", "UP",
                                         now + 60, "屯門碼頭", "2 號月台", "", false,
                                         true};
    const JourneyTimeRecord journeyContract{"H1", "K1", 24, -1, now - 10, true};
    assert(busContract.eventEpoch == now + 60);
    assert(railContract.platformLabelTc == "2 號月台");
    assert(journeyContract.dataEpoch == now - 10);
    assert(journeyContract.colourId == -1);
    assert(journeyContract.valueKind == JourneyTimeValueKind::Minutes);

    assert(refreshIntervalMs(WidgetType::Disabled) == 0);
    assert(refreshIntervalMs(WidgetType::BusEta) == 60000);
    assert(refreshIntervalMs(WidgetType::GmbEta) == 60000);
    assert(refreshIntervalMs(WidgetType::MtrEta) == 30000);
    assert(refreshIntervalMs(WidgetType::JourneyTime) == 120000);
    assert(staleWindowSeconds(WidgetType::Disabled) == 0);
    assert(staleWindowSeconds(WidgetType::BusEta) == 180);
    assert(staleWindowSeconds(WidgetType::GmbEta) == 180);
    assert(staleWindowSeconds(WidgetType::MtrEta) == 90);
    assert(staleWindowSeconds(WidgetType::JourneyTime) == 360);
    auto tfl = busConfig();
    tfl.bus.operatorId = BusOperator::Tfl;
    assert(refreshIntervalMs(tfl) == 30000);
    assert(staleWindowSeconds(tfl) == 30);
    assert(refreshIntervalMs(busConfig()) == 60000);
    assert(staleWindowSeconds(busConfig()) == 180);

    assert(deadlineReached(100, 100));
    assert(deadlineReached(101, 100));
    assert(!deadlineReached(99, 100));
    assert(deadlineReached(0x00000010U, 0xfffffff0U));
    assert(!deadlineReached(0xfffffff0U, 0x00000010U));

    WidgetSnapshot expiring;
    expiring.valueCount = 2;
    expiring.values[0] = {"到期", "", now};
    expiring.values[1] = {"仍有效", "", now + 1};
    removeExpiredValues(expiring, now);
    assert(expiring.valueCount == 1);
    assert(expiring.values[0].text == "仍有效");

    WidgetSnapshot duration;
    duration.valueCount = 1;
    duration.values[0] = {"24 分鐘", "", 0};
    removeExpiredValues(duration, now);
    assert(duration.valueCount == 1);

    std::vector<BusEtaRecord> records = {
        {BusOperator::Citybus, "11", "I", "", now + 300, "中環", "", false},
        {BusOperator::Citybus, "11", "I", "", now + 300, "中環", "", false},
        {BusOperator::Citybus, "11", "I", "", now - 30, "中環", "", false},
        {BusOperator::Citybus, "11", "I", "", now + 600, "中環", "取消", true},
    };
    const auto bus = normalizeBusSnapshot(0, busConfig(), records, now);
    assert(bus.outcome == ProviderOutcome::Success);
    assert(bus.snapshot.state == WidgetState::Ready);
    assert(bus.snapshot.valueCount == 1);
    assert(bus.snapshot.title == "11 · 中環");
    assert(bus.snapshot.subtitle == "中環碼頭");
    assert(bus.snapshot.subtitle.find("TW515") == std::string::npos);
    assert(bus.snapshot.values[0].eventEpoch == now + 300);
    assert(bus.snapshot.fetchedAtEpoch == now);
    assert(bus.snapshot.dataAtEpoch == now);

    auto asciiStopConfig = busConfig();
    asciiStopConfig.bus.stopLabelTc = "海壩村 (TW515)";
    const auto asciiStop = normalizeBusSnapshot(0, asciiStopConfig, records, now);
    assert(asciiStop.snapshot.subtitle == "海壩村");
    assert(displayStopLabelTc("海壩村 (TW515)") == "海壩村");
    assert(displayStopLabelTc("中環碼頭（TW515）") == "中環碼頭");
    assert(displayStopLabelTc("荃灣(如心廣場)") == "荃灣(如心廣場)");

    std::vector<BusEtaRecord> sortableRecords = {
        {BusOperator::Citybus, "12", "I", "", now + 120, "中環", "", false},
        {BusOperator::Citybus, "11", "I", "", now + 900, "中環", "", false},
        {BusOperator::Citybus, "11", "I", "", now + 600, "中環", "", false},
        {BusOperator::Citybus, "11", "I", "", now + 300, "中環", "", false},
        {BusOperator::Citybus, "11", "I", "", now + 300, "中環", "", false},
    };
    const auto sortedBus = normalizeBusSnapshot(0, busConfig(), sortableRecords, now);
    assert(sortedBus.outcome == ProviderOutcome::Success);
    assert(sortedBus.snapshot.valueCount == 2);
    assert(sortedBus.snapshot.values[0].eventEpoch == now + 300);
    assert(sortedBus.snapshot.values[1].eventEpoch == now + 600);
    assert(sortedBus.snapshot.values[0].text == "5 分鐘");
    assert(sortedBus.snapshot.values[1].text == "10 分鐘");

    const auto emptyBus = normalizeBusSnapshot(0, busConfig(), {}, now);
    assert(emptyBus.outcome == ProviderOutcome::Empty);
    assert(emptyBus.snapshot.state == WidgetState::Empty);
    assert(emptyBus.snapshot.providerMessage == "暫無班次");

    WidgetConfig invalidBus = busConfig();
    invalidBus.bus.stopId.clear();
    const auto invalid = normalizeBusSnapshot(0, invalidBus, records, now);
    assert(invalid.outcome == ProviderOutcome::InvalidConfig);
    assert(invalid.snapshot.state == WidgetState::Error);
    assert(invalid.snapshot.valueCount == 0);
    assert(invalid.snapshot.providerMessage == "設定不完整");

    const auto unsynced = normalizeBusSnapshot(0, busConfig(), records, 0);
    assert(unsynced.outcome == ProviderOutcome::ClockUnsynced);
    assert(unsynced.snapshot.state == WidgetState::Error);
    assert(unsynced.snapshot.providerMessage == "時間尚未同步");

    GmbEtaPayload gmbPayload;
    gmbPayload.records = {{7, "未開出"}, {0, ""}, {3, ""}};
    const auto gmb = normalizeGmbSnapshot(1, gmbConfig(), gmbPayload, now);
    assert(gmb.outcome == ProviderOutcome::Success);
    assert(gmb.snapshot.state == WidgetState::Ready);
    assert(gmb.snapshot.title == "專線小巴 69 · 往鰂魚涌");
    assert(gmb.snapshot.subtitle == "數碼港公共運輸交匯處");
    assert(gmb.snapshot.valueCount == 2);
    assert(gmb.snapshot.values[0].text == "即將到站");
    assert(gmb.snapshot.values[0].eventEpoch == now + 60);
    assert(gmb.snapshot.values[1].text == "3 分鐘");
    assert(gmb.snapshot.values[1].eventEpoch == now + 180);

    GmbEtaPayload disabledGmb;
    disabledGmb.enabled = false;
    disabledGmb.descriptionTc = "到站預報暫停服務";
    const auto disabled = normalizeGmbSnapshot(1, gmbConfig(), disabledGmb, now);
    assert(disabled.outcome == ProviderOutcome::Empty);
    assert(disabled.snapshot.providerMessage == "到站預報暫停服務");

    WidgetConfig invalidGmb = gmbConfig();
    invalidGmb.gmb.routeId = "not-numeric";
    assert(normalizeGmbSnapshot(1, invalidGmb, gmbPayload, now).outcome ==
           ProviderOutcome::InvalidConfig);
    assert(normalizeGmbSnapshot(1, gmbConfig(), gmbPayload, 0).outcome ==
           ProviderOutcome::ClockUnsynced);

    std::vector<RailArrivalRecord> arrivals = {
        {RailMode::HeavyRail, "TML", "YUL", "UP", now + 600, "屯門", "2 號月台", "",
         false, true},
        {RailMode::HeavyRail, "TML", "YUL", "UP", now + 300, "屯門", "1 號月台", "",
         false, true},
        {RailMode::HeavyRail, "TML", "YUL", "UP", now + 300, "屯門", "1 號月台", "",
         false, true},
        {RailMode::HeavyRail, "TML", "YUL", "UP", now - 1, "屯門", "", "", false,
         true},
        {RailMode::HeavyRail, "TML", "YUL", "UP", now + 100, "屯門", "", "", true,
         true},
        {RailMode::HeavyRail, "TML", "YUL", "UP", now + 200, "屯門", "", "", false,
         false},
        {RailMode::HeavyRail, "TML", "MKK", "UP", now + 100, "屯門", "", "", false,
         true},
        {RailMode::HeavyRail, "TML", "YUL", "UP", now + 900, "屯門", "", "", false,
         true},
    };
    const auto rail = normalizeRailSnapshot(1, railConfig(), arrivals, now - 5, now);
    assert(rail.outcome == ProviderOutcome::Success);
    assert(rail.snapshot.valueCount == 2);
    assert(rail.snapshot.title == "屯馬綫 · 往屯門");
    assert(rail.snapshot.subtitle == "元朗");
    assert(rail.snapshot.values[0].eventEpoch == now + 300);
    assert(rail.snapshot.values[1].eventEpoch == now + 600);
    assert(rail.snapshot.values[0].text == "5 分鐘");
    assert(rail.snapshot.dataAtEpoch == now - 5);

    const auto journey = normalizeJourneyTimeSnapshot(2, journeyConfig(), journeyContract, now);
    assert(journey.outcome == ProviderOutcome::Success);
    assert(journey.snapshot.valueCount == 1);
    assert(journey.snapshot.values[0].eventEpoch == 0);
    assert(journey.snapshot.values[0].text == "24 分鐘");
    assert(journey.snapshot.values[0].context.empty());
    assert(journey.snapshot.dataAtEpoch == now - 10);

    const auto journeyWithoutDeviceClock =
        normalizeJourneyTimeSnapshot(2, journeyConfig(), journeyContract, 0);
    assert(journeyWithoutDeviceClock.outcome == ProviderOutcome::Success);
    assert(journeyWithoutDeviceClock.snapshot.values[0].text == "24 分鐘");

    JourneyTimeRecord congested = journeyContract;
    congested.colourId = 1;
    assert(normalizeJourneyTimeSnapshot(2, journeyConfig(), congested, now)
               .snapshot.values[0]
               .context == "交通擠塞");
    congested.colourId = 2;
    assert(normalizeJourneyTimeSnapshot(2, journeyConfig(), congested, now)
               .snapshot.values[0]
               .context == "行車緩慢");
    congested.colourId = 3;
    assert(normalizeJourneyTimeSnapshot(2, journeyConfig(), congested, now)
               .snapshot.values[0]
               .context == "交通暢順");

    JourneyTimeRecord status = journeyContract;
    status.minutes = 0;
    status.valueKind = JourneyTimeValueKind::Status;
    status.statusCode = 1;
    auto normalizedStatus =
        normalizeJourneyTimeSnapshot(2, journeyConfig(), status, now);
    assert(normalizedStatus.outcome == ProviderOutcome::Success);
    assert(normalizedStatus.snapshot.values[0].text == "交通擠塞");
    assert(normalizedStatus.snapshot.values[0].text.find("分鐘") == std::string::npos);
    status.statusCode = 3;
    normalizedStatus = normalizeJourneyTimeSnapshot(2, journeyConfig(), status, now);
    assert(normalizedStatus.outcome == ProviderOutcome::Success);
    assert(normalizedStatus.snapshot.values[0].text == "隧道封閉");

    status.valueKind = JourneyTimeValueKind::Unavailable;
    for (const int16_t code : {int16_t{4}, int16_t{-1}}) {
        status.statusCode = code;
        const auto unavailable =
            normalizeJourneyTimeSnapshot(2, journeyConfig(), status, now);
        assert(unavailable.outcome == ProviderOutcome::Empty);
        assert(unavailable.snapshot.valueCount == 0);
        assert(unavailable.snapshot.providerMessage == "暫未能取得行車時間");
    }

    JourneyTimeRecord invalidJourney = journeyContract;
    invalidJourney.valid = false;
    const auto emptyJourney =
        normalizeJourneyTimeSnapshot(2, journeyConfig(), invalidJourney, now);
    assert(emptyJourney.outcome == ProviderOutcome::Empty);
    assert(emptyJourney.snapshot.state == WidgetState::Empty);
    assert(emptyJourney.snapshot.providerMessage == "暫無班次");

    setUiLocale(UiLocale::EnGb);
    assert(englishDisplayLabel("LEI MUK SHUE (CIRCULAR)") ==
           "Lei Muk Shue (Circular)");
    assert(englishDisplayLabel("TAI WO HAU BBI-TAI WO HAU STATION") ==
           "Tai Wo Hau BBI-Tai Wo Hau Station");
    assert(englishDisplayLabel("MTR KMB LWB DLR BBI HK NT PHASE III") ==
           "MTR KMB LWB DLR BBI HK NT Phase III");
    assert(englishDisplayLabel("QUEEN'S ROAD CENTRAL") ==
           "Queen's Road Central");
    assert(englishDisplayLabel("Tsuen Wan (Hoi Pa Street)") ==
           "Tsuen Wan (Hoi Pa Street)");
    assert(englishDisplayLabel("香港天文台") == "香港天文台");

    auto uppercaseBusConfig = busConfig();
    uppercaseBusConfig.bus.operatorId = BusOperator::Kmb;
    uppercaseBusConfig.bus.destinationLabelEn = "LEI MUK SHUE (CIRCULAR)";
    uppercaseBusConfig.bus.stopLabelEn =
        "TAI WO HAU BBI-TAI WO HAU STATION";
    const auto uppercaseBus =
        configuredWidgetSnapshot(0, uppercaseBusConfig);
    assert(uppercaseBus.title == "11 · Lei Muk Shue (Circular)");
    assert(uppercaseBus.subtitle ==
           "Tai Wo Hau BBI-Tai Wo Hau Station");

    const auto englishBus = normalizeBusSnapshot(0, busConfig(), sortableRecords, now);
    assert(englishBus.snapshot.title == "11 · Central");
    assert(englishBus.snapshot.subtitle == "Central Pier");
    assert(englishBus.snapshot.values[0].context == "Central");
    assert(englishBus.snapshot.values[0].text == "5 min");
    assert(configuredWidgetSnapshot(1, railConfig()).title ==
           "Tuen Ma Line · Tuen Mun bound");
    assert(configuredWidgetSnapshot(2, journeyConfig()).title == "紅磡");
    assert(normalizeBusSnapshot(0, busConfig(), {}, now).snapshot.providerMessage ==
           "No arrivals");
    assert(normalizeJourneyTimeSnapshot(2, journeyConfig(), journeyContract, now)
               .snapshot.values[0]
               .text == "24 min");
    assert(std::string(weekdayText(4)) == "Thu");
    setUiLocale(UiLocale::ZhHk);

    return 0;
}
