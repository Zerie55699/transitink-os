#include "TransitJsonParsers.h"

#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int64_t kNowEpoch = 2000000000;

transitink::WidgetConfig citybusConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::BusEta;
    config.bus.operatorId = transitink::BusOperator::Citybus;
    config.bus.routeId = "11";
    config.bus.directionId = "O";
    config.bus.serviceType = "1";
    config.bus.stopId = "001145";
    config.bus.routeLabelTc = "11";
    config.bus.stopLabelTc = "中環碼頭";
    config.bus.destinationLabelTc = "渣甸山";
    return config;
}

transitink::WidgetConfig longWinConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::BusEta;
    config.bus.operatorId = transitink::BusOperator::LongWin;
    config.bus.routeId = "A31";
    config.bus.directionId = "O";
    config.bus.serviceType = "1";
    config.bus.stopId = "708870108A511AF7";
    config.bus.routeLabelTc = "A31";
    config.bus.stopLabelTc = "荃灣西站";
    config.bus.destinationLabelTc = "機場";
    return config;
}

transitink::WidgetConfig mtrConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::MtrEta;
    config.mtr.mode = transitink::RailMode::HeavyRail;
    config.mtr.lineOrRouteId = "TWL";
    config.mtr.stationId = "TSW";
    config.mtr.directionId = "UP";
    config.mtr.lineOrRouteLabelTc = "荃灣綫";
    config.mtr.stationLabelTc = "荃灣";
    config.mtr.directionLabelTc = "上行";
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
    config.mtr.directionLabelTc = "不應用作比對";
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
    config.gmb.routeLabelTc = "69";
    config.gmb.stopLabelTc = "數碼港公共運輸交匯處";
    config.gmb.directionLabelTc = "數碼港 往 鰂魚涌";
    return config;
}

std::string loadFixture(const char* path) {
    std::ifstream input(path, std::ios::binary);
    TEST_ASSERT_TRUE_MESSAGE(input.is_open(), path);
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

void assertRecord(const transitink::BusEtaRecord& record,
                  const char* route,
                  const char* direction,
                  int64_t eventEpoch,
                  const char* destination,
                  const char* remark,
                  bool cancelled) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::BusOperator::Citybus),
                          static_cast<int>(record.operatorId));
    TEST_ASSERT_EQUAL_STRING(route, record.routeId.c_str());
    TEST_ASSERT_EQUAL_STRING(direction, record.directionId.c_str());
    TEST_ASSERT_EQUAL_STRING("", record.serviceType.c_str());
    TEST_ASSERT_EQUAL_INT64(eventEpoch, record.eventEpoch);
    TEST_ASSERT_EQUAL_STRING(destination, record.destinationLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING(remark, record.remarkTc.c_str());
    TEST_ASSERT_EQUAL(cancelled, record.cancelled);
}

void test_fixture_parses_exact_records_and_normalizes_two_arrivals() {
    const std::string json = loadFixture("test_host/fixtures/citybus_eta.json");
    std::vector<transitink::BusEtaRecord> records;
    std::string error = "sentinel";

    TEST_ASSERT_TRUE(parseCitybusEtaJson(json.c_str(), citybusConfig().bus, records, error));
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL_UINT32(8, records.size());
    assertRecord(records[0], "11", "O", kNowEpoch + 300, "渣甸山", "", false);
    assertRecord(records[1], "11", "O", kNowEpoch + 300, "渣甸山", "", false);
    assertRecord(records[2], "11", "O", kNowEpoch + 600, "渣甸山", "原定班次", false);
    assertRecord(records[3], "11", "O", 0, "渣甸山", "九巴時段", false);
    assertRecord(records[4], "11", "O", 0, "渣甸山", "只供參考", false);
    assertRecord(records[5], "11", "O", 0, "渣甸山", "原定班次取消", true);
    assertRecord(records[6], "12", "O", kNowEpoch + 120, "中環", "", false);
    assertRecord(records[7], "11", "I", kNowEpoch + 180, "中環", "", false);

    const auto result = transitink::normalizeBusSnapshot(2, citybusConfig(), records,
                                                          kNowEpoch);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::ProviderOutcome::Success),
                          static_cast<int>(result.outcome));
    TEST_ASSERT_EQUAL_UINT8(2, result.snapshot.slot);
    TEST_ASSERT_EQUAL_UINT32(2, result.snapshot.valueCount);
    TEST_ASSERT_EQUAL_INT64(kNowEpoch + 300, result.snapshot.values[0].eventEpoch);
    TEST_ASSERT_EQUAL_STRING("5 分鐘", result.snapshot.values[0].text.c_str());
    TEST_ASSERT_EQUAL_INT64(kNowEpoch + 600, result.snapshot.values[1].eventEpoch);
    TEST_ASSERT_EQUAL_STRING("10 分鐘", result.snapshot.values[1].text.c_str());
}

void test_mtr_fixture_parses_both_directions_and_normalizes_two_arrivals() {
    const std::string json = loadFixture("test_host/fixtures/mtr_next_train.json");
    std::vector<transitink::RailArrivalRecord> records;
    int64_t dataEpoch = 1;
    std::string error = "sentinel";

    TEST_ASSERT_TRUE(
        parseMtrNextTrainJson(json.c_str(), mtrConfig().mtr, records, dataEpoch, error));
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL_INT64(kNowEpoch, dataEpoch);
    TEST_ASSERT_EQUAL_UINT32(7, records.size());
    TEST_ASSERT_EQUAL_STRING("UP", records[0].directionId.c_str());
    TEST_ASSERT_EQUAL_STRING("荃灣", records[0].destinationLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("1 號月台", records[0].platformLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("列車服務延誤", records[0].messageTc.c_str());
    TEST_ASSERT_EQUAL_INT64(kNowEpoch + 300, records[0].eventEpoch);
    TEST_ASSERT_FALSE(records[3].valid);
    TEST_ASSERT_FALSE(records[4].valid);
    TEST_ASSERT_FALSE(records[5].valid);
    TEST_ASSERT_EQUAL_STRING("DOWN", records[6].directionId.c_str());
    TEST_ASSERT_EQUAL_STRING("中環", records[6].destinationLabelTc.c_str());

    const auto result = transitink::normalizeRailSnapshot(
        3, mtrConfig(), records, dataEpoch, kNowEpoch);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::ProviderOutcome::Success),
                          static_cast<int>(result.outcome));
    TEST_ASSERT_EQUAL_UINT32(2, result.snapshot.valueCount);
    TEST_ASSERT_EQUAL_INT64(kNowEpoch + 300, result.snapshot.values[0].eventEpoch);
    TEST_ASSERT_EQUAL_STRING("5 分鐘", result.snapshot.values[0].text.c_str());
    TEST_ASSERT_EQUAL_STRING("荃灣 · 1 號月台 · 列車服務延誤",
                             result.snapshot.values[0].context.c_str());
    TEST_ASSERT_EQUAL_INT64(kNowEpoch + 600, result.snapshot.values[1].eventEpoch);
}

void test_light_rail_fixture_uses_terminus_ids_and_ignores_special_rows() {
    const std::string json = loadFixture("test_host/fixtures/light_rail.json");
    std::vector<transitink::RailArrivalRecord> records;
    int64_t dataEpoch = 1;
    std::string error = "sentinel";

    TEST_ASSERT_TRUE(parseLightRailJson(json.c_str(), lightRailConfig().mtr,
                                        kNowEpoch, records, dataEpoch, error));
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL_INT64(kNowEpoch, dataEpoch);
    TEST_ASSERT_EQUAL_UINT32(10, records.size());
    TEST_ASSERT_EQUAL_STRING("1", records[0].directionId.c_str());
    TEST_ASSERT_EQUAL_STRING("屯門碼頭", records[0].destinationLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("1 號月台", records[0].platformLabelTc.c_str());
    TEST_ASSERT_EQUAL_INT64(kNowEpoch + 300, records[0].eventEpoch);
    TEST_ASSERT_EQUAL_STRING("600", records[3].directionId.c_str());
    TEST_ASSERT_EQUAL_INT64(0, records[4].eventEpoch);
    TEST_ASSERT_EQUAL_INT64(0, records[5].eventEpoch);
    TEST_ASSERT_FALSE(records[6].valid);
    TEST_ASSERT_EQUAL_INT64(0, records[6].eventEpoch);
    TEST_ASSERT_TRUE(records[7].cancelled);
    TEST_ASSERT_EQUAL_INT64(0, records[7].eventEpoch);
    TEST_ASSERT_EQUAL_STRING("列車服務暫停", records[7].messageTc.c_str());
    TEST_ASSERT_FALSE(records[8].valid);
    for (const auto& record : records) {
        TEST_ASSERT_NOT_EQUAL(0, record.lineOrRouteId.compare("901"));
        TEST_ASSERT_NOT_EQUAL(0, record.lineOrRouteId.compare("902"));
        TEST_ASSERT_NOT_EQUAL(0, record.lineOrRouteId.compare("610*"));
        TEST_ASSERT_NOT_EQUAL(0, record.lineOrRouteId.compare("SPR"));
    }

    const auto result = transitink::normalizeRailSnapshot(
        1, lightRailConfig(), records, dataEpoch, kNowEpoch);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::ProviderOutcome::Success),
                          static_cast<int>(result.outcome));
    TEST_ASSERT_EQUAL_UINT32(2, result.snapshot.valueCount);
    TEST_ASSERT_EQUAL_INT64(kNowEpoch + 300, result.snapshot.values[0].eventEpoch);
    TEST_ASSERT_EQUAL_INT64(kNowEpoch + 480, result.snapshot.values[1].eventEpoch);
}

void test_mtr_parser_rejects_wrong_mode_and_hides_provider_alert_text() {
    auto wrongMode = mtrConfig();
    wrongMode.mtr.mode = transitink::RailMode::LightRail;
    std::vector<transitink::RailArrivalRecord> records(1);
    int64_t dataEpoch = 1;
    std::string error;

    TEST_ASSERT_FALSE(parseMtrNextTrainJson("{}", wrongMode.mtr, records,
                                            dataEpoch, error));
    TEST_ASSERT_EQUAL_STRING("港鐵網絡設定不正確", error.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, records.size());
    TEST_ASSERT_EQUAL_INT64(0, dataEpoch);

    TEST_ASSERT_FALSE(parseMtrNextTrainJson(
        R"({"status":0,"message":"LOW station is suspended"})",
        mtrConfig().mtr, records, dataEpoch, error));
    TEST_ASSERT_EQUAL_STRING("港鐵服務暫未能提供", error.c_str());
    TEST_ASSERT_EQUAL_INT64(0, dataEpoch);
}

void test_light_rail_parser_rejects_wrong_mode_and_unusable_clock() {
    auto wrongMode = lightRailConfig();
    wrongMode.mtr.mode = transitink::RailMode::HeavyRail;
    std::vector<transitink::RailArrivalRecord> records(1);
    int64_t dataEpoch = 1;
    std::string error;

    TEST_ASSERT_FALSE(parseLightRailJson("{}", wrongMode.mtr, kNowEpoch,
                                         records, dataEpoch, error));
    TEST_ASSERT_EQUAL_STRING("輕鐵網絡設定不正確", error.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, records.size());
    TEST_ASSERT_EQUAL_INT64(0, dataEpoch);

    TEST_ASSERT_FALSE(parseLightRailJson(
        R"({"platform_list":[],"status":1,"system_time":"2033-05-18 11:33:20"})",
        lightRailConfig().mtr, 0, records, dataEpoch, error));
    TEST_ASSERT_EQUAL_STRING("時間尚未同步", error.c_str());
}

void test_rail_parsers_reject_directions_outside_the_selected_catalog_group() {
    std::vector<transitink::RailArrivalRecord> records(1);
    int64_t dataEpoch = 1;
    std::string error;

    auto heavy = mtrConfig();
    heavy.mtr.directionId = "SIDEWAYS";
    TEST_ASSERT_FALSE(parseMtrNextTrainJson("{}", heavy.mtr, records,
                                            dataEpoch, error));
    TEST_ASSERT_EQUAL_STRING("港鐵方向設定不正確", error.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, records.size());
    TEST_ASSERT_EQUAL_INT64(0, dataEpoch);

    auto light = lightRailConfig();
    light.mtr.directionId = "999";
    records.push_back({});
    dataEpoch = 1;
    TEST_ASSERT_FALSE(parseLightRailJson("{}", light.mtr, kNowEpoch,
                                         records, dataEpoch, error));
    TEST_ASSERT_EQUAL_STRING("輕鐵方向設定不正確", error.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, records.size());
    TEST_ASSERT_EQUAL_INT64(0, dataEpoch);
}

void test_empty_data_is_a_successful_empty_snapshot() {
    const char* json = R"({"type":"ETA","version":"2.0","data":[]})";
    std::vector<transitink::BusEtaRecord> records = {
        {transitink::BusOperator::Citybus, "sentinel", "O", "", 1, "", "", false}};
    std::string error = "sentinel";

    TEST_ASSERT_TRUE(parseCitybusEtaJson(json, citybusConfig().bus, records, error));
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, records.size());
    const auto result = transitink::normalizeBusSnapshot(1, citybusConfig(), records,
                                                          kNowEpoch);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::ProviderOutcome::Empty),
                          static_cast<int>(result.outcome));
    TEST_ASSERT_EQUAL_UINT8(1, result.snapshot.slot);
    TEST_ASSERT_EQUAL_STRING("暫無班次", result.snapshot.providerMessage.c_str());
}

void test_malformed_and_wrong_shape_json_return_formal_errors() {
    std::vector<transitink::BusEtaRecord> records = {
        {transitink::BusOperator::Citybus, "sentinel", "O", "", 1, "", "", false}};
    std::string error;

    TEST_ASSERT_FALSE(parseCitybusEtaJson("{\"data\":[", citybusConfig().bus, records,
                                          error));
    TEST_ASSERT_EQUAL_UINT32(0, records.size());
    TEST_ASSERT_EQUAL_STRING("城巴到站時間資料無法解析", error.c_str());

    records.push_back({transitink::BusOperator::Citybus, "sentinel", "O", "", 1,
                       "", "", false});
    TEST_ASSERT_FALSE(parseCitybusEtaJson("{\"data\":{}}", citybusConfig().bus,
                                          records, error));
    TEST_ASSERT_EQUAL_UINT32(0, records.size());
    TEST_ASSERT_EQUAL_STRING("城巴到站時間資料格式錯誤", error.c_str());
}

void test_long_win_uses_official_kmb_company_code_and_keeps_selected_operator() {
    const char* json = R"({
        "type":"ETA",
        "version":"1.0",
        "data":[
          {"co":"KMB","route":"A31","dir":"O","service_type":1,
           "eta":"2033-05-18T11:38:20+08:00","dest_tc":"機場","rmk_tc":""},
          {"co":"LWB","route":"A31","dir":"O","service_type":1,
           "eta":"2033-05-18T11:43:20+08:00","dest_tc":"機場","rmk_tc":""}
        ]
      })";
    std::vector<transitink::BusEtaRecord> records;
    std::string error;

    TEST_ASSERT_TRUE(parseKmbEtaJson(json, longWinConfig().bus, records, error));
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL_UINT32(1, records.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::BusOperator::LongWin),
                          static_cast<int>(records[0].operatorId));
    TEST_ASSERT_EQUAL_STRING("A31", records[0].routeId.c_str());
    TEST_ASSERT_EQUAL_STRING("O", records[0].directionId.c_str());
    TEST_ASSERT_EQUAL_STRING("1", records[0].serviceType.c_str());
    TEST_ASSERT_EQUAL_INT64(kNowEpoch + 300, records[0].eventEpoch);

    auto kmb = longWinConfig();
    kmb.bus.operatorId = transitink::BusOperator::Kmb;
    TEST_ASSERT_TRUE(parseKmbEtaJson(json, kmb.bus, records, error));
    TEST_ASSERT_EQUAL_UINT32(1, records.size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::BusOperator::Kmb),
                          static_cast<int>(records[0].operatorId));
}

void test_invalid_iso_timestamps_remain_non_live() {
    const char* json = R"({"data":[
      {"co":"CTB","route":"11","dir":"O","eta":"2033-02-31T11:38:20+08:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2023-02-29T11:38:20+08:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T24:00:00+08:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T11:60:00+08:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T11:38:60+08:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T11:38:20+15:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T11:38:20+08:60","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T11:38:20+08:00tail","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T11:38:20+0800","dest_tc":"渣甸山","rmk_tc":""}
    ]})";
    std::vector<transitink::BusEtaRecord> records;
    std::string error;

    TEST_ASSERT_TRUE(parseCitybusEtaJson(json, citybusConfig().bus, records, error));
    TEST_ASSERT_EQUAL_UINT32(9, records.size());
    for (const auto& record : records) {
        TEST_ASSERT_EQUAL_INT64(0, record.eventEpoch);
    }
    const auto result = transitink::normalizeBusSnapshot(0, citybusConfig(), records,
                                                          kNowEpoch);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::ProviderOutcome::Empty),
                          static_cast<int>(result.outcome));
}

void test_valid_iso_timezones_and_leap_day_are_accepted() {
    const char* json = R"({"data":[
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T03:33:20Z","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-17T22:33:20-05:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2032-02-29T00:00:00Z","dest_tc":"渣甸山","rmk_tc":""}
    ]})";
    std::vector<transitink::BusEtaRecord> records;
    std::string error;

    TEST_ASSERT_TRUE(parseCitybusEtaJson(json, citybusConfig().bus, records, error));
    TEST_ASSERT_EQUAL_UINT32(3, records.size());
    TEST_ASSERT_EQUAL_INT64(kNowEpoch, records[0].eventEpoch);
    TEST_ASSERT_EQUAL_INT64(kNowEpoch, records[1].eventEpoch);
    TEST_ASSERT_GREATER_THAN_INT64(0, records[2].eventEpoch);
}

void test_schema_string_fields_reject_numeric_and_missing_required_values() {
    const char* json = R"({"data":[
      {"co":123,"route":"11","dir":"O","eta":"2033-05-18T11:38:20+08:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":11,"dir":"O","eta":"2033-05-18T11:38:20+08:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":0,"eta":"2033-05-18T11:38:20+08:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":2000000300,"dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T11:38:20+08:00","dest_tc":123,"rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T11:38:20+08:00","dest_tc":"渣甸山","rmk_tc":123},
      {"co":"CTB","dir":"O","eta":"2033-05-18T11:38:20+08:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","eta":"2033-05-18T11:38:20+08:00","dest_tc":"渣甸山","rmk_tc":""},
      {"co":"CTB","route":"11","dir":"O","eta":"2033-05-18T11:38:20+08:00","dest_tc":"渣甸山","rmk_tc":"有效"}
    ]})";
    std::vector<transitink::BusEtaRecord> records;
    std::string error;

    TEST_ASSERT_TRUE(parseCitybusEtaJson(json, citybusConfig().bus, records, error));
    TEST_ASSERT_EQUAL_UINT32(1, records.size());
    assertRecord(records[0], "11", "O", kNowEpoch + 300, "渣甸山", "有效", false);
    const auto result = transitink::normalizeBusSnapshot(0, citybusConfig(), records,
                                                          kNowEpoch);
    TEST_ASSERT_EQUAL_UINT32(1, result.snapshot.valueCount);
    TEST_ASSERT_EQUAL_STRING("渣甸山 · 有效",
                             result.snapshot.values[0].context.c_str());
}

void test_kmb_service_type_accepts_integer_or_numeric_string_only() {
    const char* json = R"({"data":[
      {"co":"KMB","route":"A31","dir":"O","service_type":1,"eta":"2033-05-18T11:38:20+08:00","dest_tc":"機場","rmk_tc":""},
      {"co":"KMB","route":"A31","dir":"O","service_type":"002","eta":"2033-05-18T11:43:20+08:00","dest_tc":"機場","rmk_tc":""},
      {"co":"KMB","route":"A31","dir":"O","service_type":"2A","eta":"2033-05-18T11:48:20+08:00","dest_tc":"機場","rmk_tc":""},
      {"co":"KMB","route":"A31","dir":"O","service_type":-1,"eta":"2033-05-18T11:53:20+08:00","dest_tc":"機場","rmk_tc":""},
      {"co":"KMB","route":"A31","dir":"O","service_type":1.5,"eta":"2033-05-18T11:58:20+08:00","dest_tc":"機場","rmk_tc":""},
      {"co":"KMB","route":"A31","dir":"O","eta":"2033-05-18T12:03:20+08:00","dest_tc":"機場","rmk_tc":""}
    ]})";
    std::vector<transitink::BusEtaRecord> records;
    std::string error;

    TEST_ASSERT_TRUE(parseKmbEtaJson(json, longWinConfig().bus, records, error));
    TEST_ASSERT_EQUAL_UINT32(2, records.size());
    TEST_ASSERT_EQUAL_STRING("1", records[0].serviceType.c_str());
    TEST_ASSERT_EQUAL_STRING("2", records[1].serviceType.c_str());
}

void test_citybus_paths_allow_only_official_identifiers_and_directions() {
    TEST_ASSERT_TRUE(isOfficialBusIdentifier("11"));
    TEST_ASSERT_TRUE(isOfficialBusIdentifier("A31"));
    TEST_ASSERT_TRUE(isOfficialBusIdentifier("001145"));
    for (const char* invalid : {"", "../11", "11/12", "11 12", "11-12",
                                "11%2F12", "路線11"}) {
        TEST_ASSERT_FALSE(isOfficialBusIdentifier(invalid));
    }

    std::string path = "sentinel";
    TEST_ASSERT_TRUE(mapCitybusDirectionPath("I", path));
    TEST_ASSERT_EQUAL_STRING("inbound", path.c_str());
    TEST_ASSERT_TRUE(mapCitybusDirectionPath("O", path));
    TEST_ASSERT_EQUAL_STRING("outbound", path.c_str());
    for (const char* invalid : {"", "i", "o", "inbound", "outbound", "I/O"}) {
        path = "sentinel";
        TEST_ASSERT_FALSE(mapCitybusDirectionPath(invalid, path));
        TEST_ASSERT_EQUAL_STRING("", path.c_str());
    }
}

void test_gmb_catalog_parsers_return_route_directions_and_stops() {
    const char* routeJson = R"({"data":{"routes":["1","10P",11,"../12"]}})";
    std::vector<std::string> routes;
    std::string error = "sentinel";
    TEST_ASSERT_TRUE(parseGmbRouteCodesJson(routeJson, "HKI", routes, error));
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL_UINT32(2, routes.size());
    TEST_ASSERT_EQUAL_STRING("1", routes[0].c_str());
    TEST_ASSERT_EQUAL_STRING("10P", routes[1].c_str());

    const char* directionJson = R"({"data":[
      {"route_id":2000410,"region":"HKI","route_code":"69",
       "description_tc":"正常班次","directions":[
         {"route_seq":1,"orig_tc":"數碼港","dest_tc":"鰂魚涌"},
         {"route_seq":2,"orig_tc":"鰂魚涌","dest_tc":"數碼港"}]},
      {"route_id":2000999,"region":"KLN","route_code":"69",
       "description_tc":"不應採用","directions":[
         {"route_seq":1,"orig_tc":"甲","dest_tc":"乙"}]}
    ]})";
    std::vector<transitink::GmbCatalogDirection> directions;
    TEST_ASSERT_TRUE(parseGmbDirectionsJson(directionJson, "HKI", "69",
                                             directions, error));
    TEST_ASSERT_EQUAL_UINT32(2, directions.size());
    TEST_ASSERT_EQUAL_STRING("HKI", directions[0].region.c_str());
    TEST_ASSERT_EQUAL_STRING("2000410", directions[0].routeId.c_str());
    TEST_ASSERT_EQUAL_STRING("1", directions[0].routeSeq.c_str());
    TEST_ASSERT_EQUAL_STRING("數碼港", directions[0].originLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("鰂魚涌",
                             directions[0].destinationLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("正常班次",
                             directions[0].descriptionTc.c_str());

    const char* stopJson = R"({"data":{"route_stops":[
      {"stop_seq":1,"stop_id":20003337,"name_tc":"數碼港公共運輸交匯處"},
      {"stop_seq":"2","stop_id":"20007719","name_tc":"資訊道"},
      {"stop_seq":3,"stop_id":20002491,"name_tc":123}
    ]}})";
    std::vector<transitink::GmbCatalogStop> stops;
    TEST_ASSERT_TRUE(parseGmbStopsJson(stopJson, "2000410", "1", stops,
                                       error));
    TEST_ASSERT_EQUAL_UINT32(2, stops.size());
    TEST_ASSERT_EQUAL_STRING("20003337", stops[0].stopId.c_str());
    TEST_ASSERT_EQUAL_STRING("1", stops[0].stopSeq.c_str());
    TEST_ASSERT_EQUAL_STRING("數碼港公共運輸交匯處",
                             stops[0].labelTc.c_str());
}

void test_gmb_eta_fixture_parses_and_normalizes_arrivals() {
    const std::string json = loadFixture("test_host/fixtures/gmb_eta.json");
    transitink::GmbEtaPayload payload;
    std::string error = "sentinel";
    TEST_ASSERT_TRUE(parseGmbEtaJson(json.c_str(), gmbConfig().gmb, payload,
                                     error));
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_TRUE(payload.enabled);
    TEST_ASSERT_EQUAL_UINT32(2, payload.records.size());
    TEST_ASSERT_EQUAL_INT(0, payload.records[0].diffMinutes);
    TEST_ASSERT_EQUAL_STRING("", payload.records[0].remarkTc.c_str());
    TEST_ASSERT_EQUAL_INT(7, payload.records[1].diffMinutes);
    TEST_ASSERT_EQUAL_STRING("未開出", payload.records[1].remarkTc.c_str());

    const auto result = transitink::normalizeGmbSnapshot(
        1, gmbConfig(), payload, kNowEpoch);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::ProviderOutcome::Success),
                          static_cast<int>(result.outcome));
    TEST_ASSERT_EQUAL_UINT32(2, result.snapshot.valueCount);
    TEST_ASSERT_EQUAL_STRING("即將到站", result.snapshot.values[0].text.c_str());
    TEST_ASSERT_EQUAL_INT64(kNowEpoch + 60,
                            result.snapshot.values[0].eventEpoch);
    TEST_ASSERT_EQUAL_STRING("7 分鐘", result.snapshot.values[1].text.c_str());
    TEST_ASSERT_EQUAL_STRING("未開出", result.snapshot.values[1].context.c_str());
}

void test_gmb_eta_disabled_and_invalid_shapes_are_explicit() {
    const char* disabled = R"({"data":{"stop_id":20003337,"enabled":false,
      "description_tc":"班次暫停","eta":[]}})";
    transitink::GmbEtaPayload payload;
    std::string error;
    TEST_ASSERT_TRUE(parseGmbEtaJson(disabled, gmbConfig().gmb, payload, error));
    TEST_ASSERT_FALSE(payload.enabled);
    TEST_ASSERT_EQUAL_STRING("班次暫停", payload.descriptionTc.c_str());

    TEST_ASSERT_FALSE(parseGmbEtaJson("{\"data\":[", gmbConfig().gmb,
                                      payload, error));
    TEST_ASSERT_EQUAL_STRING("專線小巴到站時間資料無法解析",
                             error.c_str());
    TEST_ASSERT_FALSE(parseGmbEtaJson(
        "{\"data\":{\"stop_id\":999,\"enabled\":true,\"eta\":[]}}",
        gmbConfig().gmb, payload, error));
    TEST_ASSERT_EQUAL_STRING("專線小巴到站時間資料格式錯誤",
                             error.c_str());

    std::vector<std::string> routes;
    TEST_ASSERT_FALSE(parseGmbRouteCodesJson("{}", "INVALID", routes, error));
    TEST_ASSERT_EQUAL_STRING("專線小巴地區設定不正確", error.c_str());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fixture_parses_exact_records_and_normalizes_two_arrivals);
    RUN_TEST(test_mtr_fixture_parses_both_directions_and_normalizes_two_arrivals);
    RUN_TEST(test_light_rail_fixture_uses_terminus_ids_and_ignores_special_rows);
    RUN_TEST(test_mtr_parser_rejects_wrong_mode_and_hides_provider_alert_text);
    RUN_TEST(test_light_rail_parser_rejects_wrong_mode_and_unusable_clock);
    RUN_TEST(test_rail_parsers_reject_directions_outside_the_selected_catalog_group);
    RUN_TEST(test_empty_data_is_a_successful_empty_snapshot);
    RUN_TEST(test_malformed_and_wrong_shape_json_return_formal_errors);
    RUN_TEST(test_long_win_uses_official_kmb_company_code_and_keeps_selected_operator);
    RUN_TEST(test_invalid_iso_timestamps_remain_non_live);
    RUN_TEST(test_valid_iso_timezones_and_leap_day_are_accepted);
    RUN_TEST(test_schema_string_fields_reject_numeric_and_missing_required_values);
    RUN_TEST(test_kmb_service_type_accepts_integer_or_numeric_string_only);
    RUN_TEST(test_citybus_paths_allow_only_official_identifiers_and_directions);
    RUN_TEST(test_gmb_catalog_parsers_return_route_directions_and_stops);
    RUN_TEST(test_gmb_eta_fixture_parses_and_normalizes_arrivals);
    RUN_TEST(test_gmb_eta_disabled_and_invalid_shapes_are_explicit);
    return UNITY_END();
}
