#include "ConfigStore.h"
#include "PortalConfigCodec.h"
#include "core/PortalRequestAuth.h"
#include "core/FirmwareUpdateCore.h"

#include <ArduinoJson.h>
#include <unity.h>

#include <cstdio>
#include <string>

namespace {

std::string repeated(char value, std::size_t size) {
    return std::string(size, value);
}

transitink::WidgetConfig validBus(transitink::BusOperator operatorId = transitink::BusOperator::Kmb) {
    transitink::WidgetConfig widget;
    widget.type = transitink::WidgetType::BusEta;
    widget.bus.operatorId = operatorId;
    widget.bus.routeId = "268B";
    widget.bus.directionId = "O";
    widget.bus.serviceType = "1";
    widget.bus.stopId = "STOP-A";
    widget.bus.routeLabelTc = "268B";
    widget.bus.stopLabelTc = "元朗廣場";
    widget.bus.destinationLabelTc = "紅磡碼頭";
    widget.bus.routeLabelEn = "268B";
    widget.bus.stopLabelEn = "Yuen Long Plaza";
    widget.bus.destinationLabelEn = "Hung Hom Ferry Pier";
    return widget;
}

transitink::WidgetConfig validMtr() {
    transitink::WidgetConfig widget;
    widget.type = transitink::WidgetType::MtrEta;
    widget.mtr.mode = transitink::RailMode::HeavyRail;
    widget.mtr.lineOrRouteId = "TML";
    widget.mtr.stationId = "YUL";
    widget.mtr.directionId = "UP";
    widget.mtr.lineOrRouteLabelTc = "屯馬綫";
    widget.mtr.stationLabelTc = "元朗";
    widget.mtr.directionLabelTc = "往烏溪沙";
    widget.mtr.lineOrRouteLabelEn = "Tuen Ma Line";
    widget.mtr.stationLabelEn = "Yuen Long";
    widget.mtr.directionLabelEn = "Wu Kai Sha bound";
    return widget;
}

transitink::WidgetConfig validGmb() {
    transitink::WidgetConfig widget;
    widget.type = transitink::WidgetType::GmbEta;
    widget.gmb.region = "HKI";
    widget.gmb.routeCode = "69";
    widget.gmb.routeId = "2000410";
    widget.gmb.routeSeq = "1";
    widget.gmb.stopId = "20003337";
    widget.gmb.stopSeq = "1";
    widget.gmb.routeLabelTc = "專線小巴 69";
    widget.gmb.stopLabelTc = "數碼港公共運輸交匯處";
    widget.gmb.directionLabelTc = "往鰂魚涌";
    widget.gmb.routeLabelEn = "Green minibus 69";
    widget.gmb.stopLabelEn = "Cyberport Public Transport Interchange";
    widget.gmb.directionLabelEn = "Quarry Bay bound";
    return widget;
}

transitink::WidgetConfig validJourney() {
    transitink::WidgetConfig widget;
    widget.type = transitink::WidgetType::JourneyTime;
    widget.journeyTime.locationId = "H1";
    widget.journeyTime.destinationId = "KOWLOON";
    widget.journeyTime.locationLabelTc = "元朗公路";
    widget.journeyTime.destinationLabelTc = "九龍";
    // The official Journey Time feed does not currently provide labels for
    // this fixture in English. Empty fields exercise source-language fallback.
    return widget;
}

DeviceConfig roundTripConfig() {
    DeviceConfig config;
    config.uiLocale = transitink::UiLocale::EnGb;
    config.displayFont = transitink::DisplayFont::Unifont;
    config.timeZone = transitink::DeviceTimeZone::UnitedKingdom;
    config.wifiSsid = "TransitInk-Test";
    config.wifiPassword = "password";
    config.weatherLocationTc = "uk:london";
    config.scheduledWakeEnabled = true;
    config.scheduledWakeStartMinutes = 8 * 60;
    config.scheduledWakeEndMinutes = 9 * 60;
    config.widgets[0] = validBus();
    config.widgets[1] = validMtr();
    config.widgets[2] = validJourney();
    config.widgets[3] = validGmb();
    return config;
}

DeviceConfig sentinelConfig() {
    DeviceConfig config = roundTripConfig();
    config.wifiSsid = "sentinel";
    return config;
}

String portalBody(const DeviceConfig& config, const String& submittedPassword) {
    StaticJsonDocument<transitink::kConfigJsonCapacity> doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, serializeDeviceConfigJson(config)));
    doc["wifi_password"] = submittedPassword;
    String body;
    serializeJson(doc, body);
    return body;
}

void assertSentinelPreserved(const DeviceConfig& config) {
    TEST_ASSERT_EQUAL_STRING("sentinel", config.wifiSsid.c_str());
    TEST_ASSERT_EQUAL_UINT16(transitink::kConfigSchemaVersion, config.schemaVersion);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::BusEta),
                          static_cast<int>(config.widgets[0].type));
    TEST_ASSERT_EQUAL_STRING("268B", config.widgets[0].bus.routeId.c_str());
}

void assertParseFailurePreserves(const std::string& json) {
    DeviceConfig config = sentinelConfig();
    String error;
    TEST_ASSERT_FALSE(parseDeviceConfigJson(json, config, error));
    TEST_ASSERT_FALSE(error.empty());
    assertSentinelPreserved(config);
}

std::string disabledWidgets(std::size_t count) {
    std::string json = "[";
    for (std::size_t index = 0; index < count; ++index) {
        if (index > 0) {
            json += ',';
        }
        json += R"({"type":"disabled"})";
    }
    json += ']';
    return json;
}

std::string v2Json(const std::string& widgets, const std::string& wifiSsid = "wifi") {
    return R"({"schema_version":2,"wifi_ssid":")" + wifiSsid + R"(","widgets":)" + widgets + '}';
}

std::string v3Json(const std::string& widgets, const std::string& wifiSsid = "wifi") {
    return R"({"schema_version":3,"wifi_ssid":")" + wifiSsid + R"(","widgets":)" + widgets + '}';
}

void test_v2_four_slot_config_migrates_and_defaults_to_traditional_chinese() {
    DeviceConfig parsed = sentinelConfig();
    String error;
    TEST_ASSERT_TRUE(parseDeviceConfigJson(v2Json(disabledWidgets(4)), parsed, error));
    TEST_ASSERT_TRUE(error.empty());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::UiLocale::ZhHk),
                          static_cast<int>(parsed.uiLocale));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::DisplayFont::NotoSans),
                          static_cast<int>(parsed.displayFont));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(transitink::DeviceTimeZone::HongKong),
        static_cast<int>(parsed.timeZone));
    TEST_ASSERT_EQUAL_UINT16(transitink::kConfigSchemaVersion, parsed.schemaVersion);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::Disabled),
                          static_cast<int>(parsed.widgets[11].type));
}

void test_has_usable_config_requires_valid_complete_schema() {
    DeviceConfig config;
    config.wifiSsid = "wifi";
    config.wifiPassword = "";
    TEST_ASSERT_TRUE(hasUsableConfig(config));

    config.schemaVersion = 1;
    config.widgets[0] = validBus();
    TEST_ASSERT_FALSE(hasUsableConfig(config));

    config.schemaVersion = transitink::kConfigSchemaVersion;
    config.wifiSsid = "";
    TEST_ASSERT_FALSE(hasUsableConfig(config));

    config.wifiSsid = "wifi";
    config.widgets[1].type = transitink::WidgetType::JourneyTime;
    TEST_ASSERT_FALSE(hasUsableConfig(config));

    config.widgets[1] = transitink::WidgetConfig{};
    config.weatherLocationTc = repeated('W', transitink::kMaxCommonConfigTextBytes + 1);
    TEST_ASSERT_FALSE(hasUsableConfig(config));

    config.weatherLocationTc = "香港天文台";
    config.uiLocale = static_cast<transitink::UiLocale>(0xff);
    TEST_ASSERT_FALSE(hasUsableConfig(config));

    config.uiLocale = transitink::UiLocale::ZhHk;
    config.displayFont = static_cast<transitink::DisplayFont>(0xff);
    TEST_ASSERT_FALSE(hasUsableConfig(config));

    config.displayFont = transitink::DisplayFont::NotoSans;
    config.timeZone = static_cast<transitink::DeviceTimeZone>(0xff);
    TEST_ASSERT_FALSE(hasUsableConfig(config));

    config.timeZone = transitink::DeviceTimeZone::HongKong;
    config.scheduledWakeEnabled = true;
    config.scheduledWakeStartMinutes = 480;
    config.scheduledWakeEndMinutes = 480;
    TEST_ASSERT_FALSE(hasUsableConfig(config));
}

void test_v3_round_trip_uses_exact_active_payloads() {
    const DeviceConfig original = roundTripConfig();
    DeviceConfigSerializationMetrics metrics;
    String json;
    String error;
    TEST_ASSERT_TRUE(serializeDeviceConfigJsonChecked(original, json, metrics, error));
    TEST_ASSERT_TRUE(error.empty());

    StaticJsonDocument<transitink::kConfigJsonCapacity> doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, json));
    TEST_ASSERT_EQUAL_UINT16(transitink::kConfigSchemaVersion, doc["schema_version"].as<uint16_t>());
    TEST_ASSERT_EQUAL_STRING("en-GB", doc["ui_locale"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("unifont", doc["display_font"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Europe/London",
                             doc["time_zone"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("uk:london",
                             doc["weather_location_tc"].as<const char*>());
    JsonArrayConst widgets = doc["widgets"].as<JsonArrayConst>();
    TEST_ASSERT_EQUAL_UINT32(transitink::kWidgetSlotCount, widgets.size());

    JsonObjectConst bus = widgets[0].as<JsonObjectConst>();
    TEST_ASSERT_TRUE(bus.containsKey("bus"));
    TEST_ASSERT_FALSE(bus.containsKey("mtr"));
    TEST_ASSERT_FALSE(bus.containsKey("journey_time"));
    TEST_ASSERT_FALSE(bus.containsKey("gmb"));
    TEST_ASSERT_EQUAL_STRING("kmb", bus["bus"]["operator"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Yuen Long Plaza",
                             bus["bus"]["stop_label_en"].as<const char*>());

    JsonObjectConst mtr = widgets[1].as<JsonObjectConst>();
    TEST_ASSERT_TRUE(mtr.containsKey("mtr"));
    TEST_ASSERT_FALSE(mtr.containsKey("bus"));
    TEST_ASSERT_FALSE(mtr.containsKey("journey_time"));

    JsonObjectConst journey = widgets[2].as<JsonObjectConst>();
    TEST_ASSERT_TRUE(journey.containsKey("journey_time"));
    TEST_ASSERT_FALSE(journey.containsKey("bus"));
    TEST_ASSERT_FALSE(journey.containsKey("mtr"));

    JsonObjectConst gmb = widgets[3].as<JsonObjectConst>();
    TEST_ASSERT_TRUE(gmb.containsKey("gmb"));
    TEST_ASSERT_FALSE(gmb.containsKey("bus"));
    TEST_ASSERT_FALSE(gmb.containsKey("mtr"));
    TEST_ASSERT_FALSE(gmb.containsKey("journey_time"));
    TEST_ASSERT_EQUAL_STRING("2000410", gmb["gmb"]["route_id"].as<const char*>());

    DeviceConfig parsed;
    TEST_ASSERT_TRUE(parseDeviceConfigJson(json, parsed, error));
    TEST_ASSERT_EQUAL_STRING("268B", parsed.widgets[0].bus.routeId.c_str());
    TEST_ASSERT_EQUAL_STRING("YUL", parsed.widgets[1].mtr.stationId.c_str());
    TEST_ASSERT_EQUAL_STRING("Yuen Long",
                             parsed.widgets[1].mtr.stationLabelEn.c_str());
    TEST_ASSERT_EQUAL_STRING("KOWLOON", parsed.widgets[2].journeyTime.destinationId.c_str());
    TEST_ASSERT_EQUAL_STRING("20003337", parsed.widgets[3].gmb.stopId.c_str());
    TEST_ASSERT_EQUAL_STRING(
        "Cyberport Public Transport Interchange",
        parsed.widgets[3].gmb.stopLabelEn.c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::UiLocale::EnGb),
                          static_cast<int>(parsed.uiLocale));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::DisplayFont::Unifont),
                          static_cast<int>(parsed.displayFont));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(transitink::DeviceTimeZone::UnitedKingdom),
        static_cast<int>(parsed.timeZone));
    TEST_ASSERT_TRUE(doc["scheduled_wake_enabled"].as<bool>());
    TEST_ASSERT_EQUAL_UINT16(8 * 60, doc["scheduled_wake_start_minutes"].as<uint16_t>());
    TEST_ASSERT_EQUAL_UINT16(9 * 60, doc["scheduled_wake_end_minutes"].as<uint16_t>());
    TEST_ASSERT_TRUE(parsed.scheduledWakeEnabled);
    TEST_ASSERT_EQUAL_UINT16(8 * 60, parsed.scheduledWakeStartMinutes);
    TEST_ASSERT_EQUAL_UINT16(9 * 60, parsed.scheduledWakeEndMinutes);
}

void test_invalid_time_zone_is_rejected_without_mutation() {
    std::string invalidJson = v2Json(disabledWidgets(4));
    invalidJson.pop_back();
    invalidJson += R"(,"time_zone":"Etc/Unknown"})";
    assertParseFailurePreserves(invalidJson);

    DeviceConfig invalid = roundTripConfig();
    invalid.timeZone = static_cast<transitink::DeviceTimeZone>(0xff);
    DeviceConfigSerializationMetrics metrics;
    String json;
    String error;
    TEST_ASSERT_FALSE(
        serializeDeviceConfigJsonChecked(invalid, json, metrics, error));
    TEST_ASSERT_FALSE(error.empty());
}

void test_invalid_display_font_is_rejected_without_mutation() {
    std::string invalidJson = v3Json(disabledWidgets(transitink::kWidgetSlotCount));
    invalidJson.pop_back();
    invalidJson += R"(,"display_font":"unknown"})";
    assertParseFailurePreserves(invalidJson);

    DeviceConfig invalid = roundTripConfig();
    invalid.displayFont = static_cast<transitink::DisplayFont>(0xff);
    DeviceConfigSerializationMetrics metrics;
    String json;
    String error;
    TEST_ASSERT_FALSE(
        serializeDeviceConfigJsonChecked(invalid, json, metrics, error));
    TEST_ASSERT_FALSE(error.empty());
}

void test_scheduled_wake_rejects_invalid_windows_without_mutation() {
    DeviceConfig parsed = sentinelConfig();
    String error;
    std::string invalidJson = v2Json(disabledWidgets(4));
    invalidJson.pop_back();
    invalidJson += R"(,"scheduled_wake_enabled":true,"scheduled_wake_start_minutes":480,"scheduled_wake_end_minutes":480})";
    TEST_ASSERT_FALSE(parseDeviceConfigJson(invalidJson.c_str(), parsed, error));
    TEST_ASSERT_FALSE(error.empty());
    assertSentinelPreserved(parsed);

    DeviceConfig invalid = roundTripConfig();
    invalid.scheduledWakeEndMinutes = invalid.scheduledWakeStartMinutes;
    DeviceConfigSerializationMetrics metrics;
    String json;
    TEST_ASSERT_FALSE(serializeDeviceConfigJsonChecked(invalid, json, metrics, error));
    TEST_ASSERT_TRUE(json.empty());
}

void test_parse_failures_preserve_the_original_config() {
    assertParseFailurePreserves("{");
    assertParseFailurePreserves(v3Json(disabledWidgets(4)));
    assertParseFailurePreserves(
        R"({"schema_version":4,"wifi_ssid":"wifi","widgets":[]})");
    assertParseFailurePreserves(v2Json(disabledWidgets(3)));
    assertParseFailurePreserves(v2Json(R"([{"type":"bus_eta","bus":{"operator":"bogus","route_id":"1","direction_id":"O","service_type":"1","stop_id":"S"}},{"type":"disabled"},{"type":"disabled"},{"type":"disabled"}])"));
    assertParseFailurePreserves(v2Json(R"([{"type":"gmb_eta","gmb":{"region":"HKI","route_code":"69","route_id":"not-numeric","route_seq":"1","stop_id":"20003337","stop_seq":"1"}},{"type":"disabled"},{"type":"disabled"},{"type":"disabled"}])"));
    assertParseFailurePreserves(v2Json(disabledWidgets(4), repeated('S', transitink::kMaxWifiSsidBytes + 1)));
    assertParseFailurePreserves(v2Json(disabledWidgets(4), repeated('X', transitink::kConfigJsonCapacity + 1)));
    assertParseFailurePreserves(
        R"({"schema_version":2,"ui_locale":"fr-FR","wifi_ssid":"wifi","widgets":[{"type":"disabled"},{"type":"disabled"},{"type":"disabled"},{"type":"disabled"}]})");
}

void test_legacy_decode_migrates_directly_to_widget_slots() {
    const String legacy = R"({"wifi_ssid":"legacy","wifi_password":"secret","stop_name_tc":"元朗廣場","refresh_seconds":75,"routes":[{"route":"268B","bound":"O","service_type":"1","stop_id":"STOP-A","dest_tc":"紅磡碼頭"},{"route":"968","bound":"I","service_type":"1","stop_id":"STOP-B","dest_tc":"元朗西"}]})";
    DeviceConfig parsed;
    String error;
    TEST_ASSERT_TRUE(parseDeviceConfigJson(legacy, parsed, error));
    TEST_ASSERT_EQUAL_UINT16(transitink::kConfigSchemaVersion, parsed.schemaVersion);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::UiLocale::ZhHk),
                          static_cast<int>(parsed.uiLocale));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::BusEta),
                          static_cast<int>(parsed.widgets[0].type));
    TEST_ASSERT_EQUAL_STRING("268B", parsed.widgets[0].bus.routeId.c_str());
    TEST_ASSERT_EQUAL_STRING("元朗廣場", parsed.widgets[0].bus.stopLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("968", parsed.widgets[1].bus.routeId.c_str());
    TEST_ASSERT_EQUAL_STRING("元朗廣場", parsed.widgets[1].bus.stopLabelTc.c_str());

    const String v2 = serializeDeviceConfigJson(parsed);
    StaticJsonDocument<transitink::kConfigJsonCapacity> doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, v2));
    TEST_ASSERT_FALSE(doc.containsKey("routes"));
    TEST_ASSERT_FALSE(doc.containsKey("stop_name_tc"));
    TEST_ASSERT_FALSE(doc.containsKey("refresh_seconds"));
    TEST_ASSERT_EQUAL_STRING("zh-HK", doc["ui_locale"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("noto_sans",
                             doc["display_font"].as<const char*>());
}

void test_maximum_valid_config_stays_within_capacity_headroom() {
    DeviceConfig config;
    config.wifiSsid = repeated('S', transitink::kMaxWifiSsidBytes);
    config.wifiPassword = repeated('P', transitink::kMaxWifiCredentialBytes);
    config.weatherLocationTc = repeated('W', transitink::kMaxCommonConfigTextBytes);
    for (auto& widget : config.widgets) {
        widget = validBus();
        widget.bus.routeId = repeated('R', transitink::kMaxStableIdBytes);
        widget.bus.directionId = repeated('D', transitink::kMaxStableIdBytes);
        widget.bus.serviceType = repeated('T', transitink::kMaxStableIdBytes);
        widget.bus.stopId = repeated('S', transitink::kMaxStableIdBytes);
        widget.bus.routeLabelTc = repeated('A', transitink::kMaxConfigLabelBytes);
        widget.bus.stopLabelTc = repeated('B', transitink::kMaxConfigLabelBytes);
        widget.bus.destinationLabelTc = repeated('C', transitink::kMaxConfigLabelBytes);
        widget.bus.routeLabelEn = repeated('D', transitink::kMaxConfigLabelBytes);
        widget.bus.stopLabelEn = repeated('E', transitink::kMaxConfigLabelBytes);
        widget.bus.destinationLabelEn =
            repeated('F', transitink::kMaxConfigLabelBytes);
    }

    DeviceConfigSerializationMetrics metrics;
    String json;
    String error;
    TEST_ASSERT_TRUE(serializeDeviceConfigJsonChecked(config, json, metrics, error));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(transitink::kConfigJsonSafeBytes, metrics.documentBytes);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(transitink::kConfigBlobSafeBytes,
                                     metrics.jsonBytes);
    TEST_ASSERT_EQUAL_UINT32(metrics.jsonBytes, json.size());
    std::printf("CONFIG_CAPACITY document=%zu json=%zu safe=%zu capacity=%zu\n",
                metrics.documentBytes,
                metrics.jsonBytes,
                transitink::kConfigJsonSafeBytes,
                transitink::kConfigJsonCapacity);
}

void test_checked_serializer_rejects_over_limit_input() {
    DeviceConfig config = roundTripConfig();
    config.widgets[0].bus.stopLabelTc = repeated('L', transitink::kMaxConfigLabelBytes + 1);
    DeviceConfigSerializationMetrics metrics;
    String json = "sentinel";
    String error;
    TEST_ASSERT_FALSE(serializeDeviceConfigJsonChecked(config, json, metrics, error));
    TEST_ASSERT_TRUE(json.empty());
    TEST_ASSERT_FALSE(error.empty());
    TEST_ASSERT_TRUE(serializeDeviceConfigJson(config).empty());
}

void test_invalid_config_save_preserves_last_valid_json() {
    ConfigStore store;
    TEST_ASSERT_TRUE(store.begin());
    const String validJson = serializeDeviceConfigJson(roundTripConfig());
    TEST_ASSERT_FALSE(validJson.empty());
    Preferences::seedTestValue(validJson);

    DeviceConfig invalid = roundTripConfig();
    invalid.widgets[0].bus.stopLabelTc = repeated('L', transitink::kMaxConfigLabelBytes + 1);
    TEST_ASSERT_FALSE(store.save(invalid));
    TEST_ASSERT_EQUAL_STRING(validJson.c_str(), Preferences::testValue().c_str());
    TEST_ASSERT_EQUAL_UINT32(0, Preferences::testWriteCount());
}

void test_sleep_resume_marker_is_persistent_and_idempotent() {
    ConfigStore store;
    TEST_ASSERT_TRUE(store.begin());
    Preferences::seedTestBool(false);
    TEST_ASSERT_FALSE(store.sleepResumePending());
    TEST_ASSERT_TRUE(store.setSleepResumePending(true));
    TEST_ASSERT_TRUE(store.sleepResumePending());
    TEST_ASSERT_EQUAL_UINT32(1, Preferences::testBoolWriteCount());
    TEST_ASSERT_TRUE(store.setSleepResumePending(true));
    TEST_ASSERT_EQUAL_UINT32(1, Preferences::testBoolWriteCount());
    TEST_ASSERT_TRUE(store.setSleepResumePending(false));
    TEST_ASSERT_FALSE(store.sleepResumePending());
    TEST_ASSERT_EQUAL_UINT32(2, Preferences::testBoolWriteCount());
}

void test_portal_get_redacts_password_and_round_trips_twelve_slots() {
    DeviceConfig config = roundTripConfig();
    config.widgets[3] = validMtr();
    config.wifiPassword = "never-return-this";
    bus_eta::BatterySnapshot battery;
    battery.valid = true;
    battery.percent = 73;
    String json;
    String error;

    TEST_ASSERT_TRUE(encodePortalConfig(config, battery, "2.0.0",
                                        "0123456789abcdef0123456789abcdef",
                                        json, error));
    StaticJsonDocument<transitink::kConfigJsonCapacity> doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, json));
    TEST_ASSERT_FALSE(doc.containsKey("wifi_password"));
    TEST_ASSERT_TRUE(doc["wifi_password_set"].as<bool>());
    TEST_ASSERT_EQUAL_UINT32(transitink::kWidgetSlotCount, doc["widgets"].size());
    TEST_ASSERT_EQUAL_STRING("2.0.0", doc["firmware_version"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef0123456789abcdef",
                             doc["csrf_token"].as<const char*>());
    TEST_ASSERT_EQUAL_UINT8(73, doc["battery"]["percent"].as<uint8_t>());
    TEST_ASSERT_TRUE(doc["scheduled_wake_enabled"].as<bool>());
    TEST_ASSERT_EQUAL_UINT16(8 * 60, doc["scheduled_wake_start_minutes"].as<uint16_t>());
    TEST_ASSERT_EQUAL_UINT16(9 * 60, doc["scheduled_wake_end_minutes"].as<uint16_t>());
    TEST_ASSERT_EQUAL_STRING("en-GB", doc["ui_locale"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("unifont", doc["display_font"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Europe/London",
                             doc["time_zone"].as<const char*>());
    TEST_ASSERT_EQUAL(std::string::npos, json.find("never-return-this"));
}

void test_portal_post_preserves_empty_password_and_replaces_non_empty_password() {
    DeviceConfig current = roundTripConfig();
    current.wifiPassword = "stored-secret";
    DeviceConfig parsed;
    String error;

    TEST_ASSERT_TRUE(decodePortalSave(portalBody(current, ""), current, parsed, error));
    TEST_ASSERT_EQUAL_STRING("stored-secret", parsed.wifiPassword.c_str());
    TEST_ASSERT_EQUAL_STRING("268B", parsed.widgets[0].bus.routeId.c_str());

    TEST_ASSERT_TRUE(decodePortalSave(portalBody(current, "replacement"), current, parsed, error));
    TEST_ASSERT_EQUAL_STRING("replacement", parsed.wifiPassword.c_str());
}

void test_portal_post_rejects_malformed_oversized_and_wrong_slot_count() {
    DeviceConfig current = roundTripConfig();
    DeviceConfig parsed = sentinelConfig();
    String error;
    TEST_ASSERT_FALSE(decodePortalSave("{", current, parsed, error));
    TEST_ASSERT_FALSE(error.empty());
    TEST_ASSERT_EQUAL_STRING("sentinel", parsed.wifiSsid.c_str());

    const String oversized(transitink::kConfigJsonCapacity + 1, 'x');
    TEST_ASSERT_FALSE(decodePortalSave(oversized, current, parsed, error));
    TEST_ASSERT_FALSE(error.empty());

    TEST_ASSERT_FALSE(decodePortalSave(
        R"({"schema_version":2,"wifi_ssid":"wifi","wifi_password":"","widgets":[{"type":"disabled"}]})",
        current, parsed, error));
    TEST_ASSERT_FALSE(error.empty());
}

void test_portal_failed_save_does_not_mutate_live_config() {
    DeviceConfig live = roundTripConfig();
    live.wifiSsid = "live-before-save";
    DeviceConfig submitted = live;
    submitted.wifiSsid = "candidate";
    ConfigStore store;
    TEST_ASSERT_TRUE(store.begin());
    Preferences::setTestWriteFailure(true);
    String error;
    TEST_ASSERT_FALSE(savePortalConfig(portalBody(submitted, ""), live, store, error));
    Preferences::setTestWriteFailure(false);
    TEST_ASSERT_EQUAL_STRING("live-before-save", live.wifiSsid.c_str());
    TEST_ASSERT_FALSE(error.empty());
}

void test_portal_successful_multi_page_save_survives_reboot_load() {
    DeviceConfig live;
    live.wifiSsid = "TransitInk-Test";
    live.wifiPassword = "stored-secret";
    live.widgets[0] = validBus();
    live.widgets[1] = validBus(transitink::BusOperator::LongWin);

    DeviceConfig submitted = live;
    submitted.widgets[2] = validMtr();
    submitted.widgets[3] = validJourney();
    submitted.widgets[4] = validBus(transitink::BusOperator::Citybus);
    submitted.uiLocale = transitink::UiLocale::EnGb;

    ConfigStore store;
    TEST_ASSERT_TRUE(store.begin());
    Preferences::seedTestValue(serializeDeviceConfigJson(live));
    String error;
    TEST_ASSERT_TRUE(savePortalConfig(portalBody(submitted, ""), live, store, error));
    TEST_ASSERT_TRUE(error.empty());

    ConfigStore rebootedStore;
    DeviceConfig rebooted;
    TEST_ASSERT_TRUE(rebootedStore.begin());
    TEST_ASSERT_TRUE(rebootedStore.load(rebooted));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::BusEta),
                          static_cast<int>(rebooted.widgets[0].type));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::BusEta),
                          static_cast<int>(rebooted.widgets[1].type));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::MtrEta),
                          static_cast<int>(rebooted.widgets[2].type));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::JourneyTime),
                          static_cast<int>(rebooted.widgets[3].type));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::WidgetType::BusEta),
                          static_cast<int>(rebooted.widgets[4].type));
    TEST_ASSERT_EQUAL_STRING("YUL", rebooted.widgets[2].mtr.stationId.c_str());
    TEST_ASSERT_EQUAL_STRING("KOWLOON", rebooted.widgets[3].journeyTime.destinationId.c_str());
    TEST_ASSERT_EQUAL_STRING("stored-secret", rebooted.wifiPassword.c_str());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::UiLocale::EnGb),
                          static_cast<int>(rebooted.uiLocale));
}

void test_portal_save_auth_rejects_cross_site_and_invalid_tokens() {
    const std::string expected = "0123456789abcdef0123456789abcdef";
    TEST_ASSERT_TRUE(transitink::isPortalSaveAuthorized(
        "application/json", expected, expected));
    TEST_ASSERT_TRUE(transitink::isPortalSaveAuthorized(
        "application/json; charset=utf-8", expected, expected));
    TEST_ASSERT_FALSE(transitink::isPortalSaveAuthorized(
        "text/plain", expected, expected));
    TEST_ASSERT_FALSE(transitink::isPortalSaveAuthorized(
        "application/x-www-form-urlencoded", expected, expected));
    TEST_ASSERT_FALSE(transitink::isPortalSaveAuthorized(
        "application/json", "", expected));
    TEST_ASSERT_FALSE(transitink::isPortalSaveAuthorized(
        "application/json", "wrong", expected));
    TEST_ASSERT_FALSE(transitink::isPortalSaveAuthorized(
        "application/json", expected, ""));
}

void test_portal_ap_password_and_request_source_are_restricted() {
    const std::string password = transitink::generatePortalApPassword(
        0x01234567U, 0x89abcdefU, 0xfedcba98U);
    TEST_ASSERT_EQUAL_UINT32(12, password.size());
    TEST_ASSERT_TRUE(
        password.find_first_not_of("23456789ABCDEFGHJKLMNPQRSTUVWXYZ") ==
        std::string::npos);
    TEST_ASSERT_TRUE(password != transitink::generatePortalApPassword(1U, 2U, 3U));

    TEST_ASSERT_TRUE(transitink::isPortalRequestSourceAllowed(
        "192.168.4.1", "", "192.168.4.1", false));
    TEST_ASSERT_TRUE(transitink::isPortalRequestSourceAllowed(
        "192.168.4.1:80", "http://192.168.4.1", "192.168.4.1", true));
    TEST_ASSERT_FALSE(transitink::isPortalRequestSourceAllowed(
        "192.168.4.1", "", "192.168.4.1", true));
    TEST_ASSERT_FALSE(transitink::isPortalRequestSourceAllowed(
        "attacker.example", "", "192.168.4.1", false));
    TEST_ASSERT_FALSE(transitink::isPortalRequestSourceAllowed(
        "192.168.4.1", "http://attacker.example", "192.168.4.1", true));
    TEST_ASSERT_FALSE(transitink::isPortalRequestSourceAllowed(
        "192.168.4.1:invalid", "", "192.168.4.1", false));
    TEST_ASSERT_TRUE(transitink::isPortalAccessTokenAuthorized(
        "SESSIONTOKEN", "SESSIONTOKEN"));
    TEST_ASSERT_FALSE(transitink::isPortalAccessTokenAuthorized(
        "WRONGTOKEN", "SESSIONTOKEN"));
    TEST_ASSERT_FALSE(transitink::isPortalAccessTokenAuthorized(
        "", "SESSIONTOKEN"));
}

void test_firmware_update_metadata_is_strictly_validated() {
    int comparison = 0;
    TEST_ASSERT_TRUE(transitink::isSemanticFirmwareVersion("1.2.3"));
    TEST_ASSERT_FALSE(transitink::isSemanticFirmwareVersion("1.2"));
    TEST_ASSERT_FALSE(transitink::isSemanticFirmwareVersion("v1.2.3"));
    TEST_ASSERT_FALSE(transitink::isSemanticFirmwareVersion("1.2.3.4"));
    TEST_ASSERT_TRUE(transitink::compareSemanticFirmwareVersions(
        "1.2.4", "1.2.3", comparison));
    TEST_ASSERT_EQUAL_INT(1, comparison);
    TEST_ASSERT_TRUE(transitink::compareSemanticFirmwareVersions(
        "1.2.3", "1.2.3", comparison));
    TEST_ASSERT_EQUAL_INT(0, comparison);
    TEST_ASSERT_FALSE(transitink::compareSemanticFirmwareVersions(
        "invalid", "1.2.3", comparison));

    TEST_ASSERT_TRUE(transitink::isSafeFirmwareAssetPath(
        "firmware/transitink-zectrix-note4-ota-v1.2.3.bin"));
    TEST_ASSERT_FALSE(transitink::isSafeFirmwareAssetPath(
        "firmware/../secrets.bin"));
    TEST_ASSERT_FALSE(transitink::isSafeFirmwareAssetPath(
        "https://attacker.example/update.bin"));
    TEST_ASSERT_FALSE(transitink::isSafeFirmwareAssetPath(
        "firmware/update.bin?download=1"));

    TEST_ASSERT_TRUE(transitink::isSha256Digest(std::string(64, 'a')));
    TEST_ASSERT_FALSE(transitink::isSha256Digest(std::string(63, 'a')));
    TEST_ASSERT_FALSE(transitink::isSha256Digest(std::string(64, 'A')));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_has_usable_config_requires_valid_complete_schema);
    RUN_TEST(test_v3_round_trip_uses_exact_active_payloads);
    RUN_TEST(test_v2_four_slot_config_migrates_and_defaults_to_traditional_chinese);
    RUN_TEST(test_invalid_time_zone_is_rejected_without_mutation);
    RUN_TEST(test_invalid_display_font_is_rejected_without_mutation);
    RUN_TEST(test_scheduled_wake_rejects_invalid_windows_without_mutation);
    RUN_TEST(test_parse_failures_preserve_the_original_config);
    RUN_TEST(test_legacy_decode_migrates_directly_to_widget_slots);
    RUN_TEST(test_maximum_valid_config_stays_within_capacity_headroom);
    RUN_TEST(test_checked_serializer_rejects_over_limit_input);
    RUN_TEST(test_invalid_config_save_preserves_last_valid_json);
    RUN_TEST(test_sleep_resume_marker_is_persistent_and_idempotent);
    RUN_TEST(test_portal_get_redacts_password_and_round_trips_twelve_slots);
    RUN_TEST(test_portal_post_preserves_empty_password_and_replaces_non_empty_password);
    RUN_TEST(test_portal_post_rejects_malformed_oversized_and_wrong_slot_count);
    RUN_TEST(test_portal_failed_save_does_not_mutate_live_config);
    RUN_TEST(test_portal_successful_multi_page_save_survives_reboot_load);
    RUN_TEST(test_portal_save_auth_rejects_cross_site_and_invalid_tokens);
    RUN_TEST(test_portal_ap_password_and_request_source_are_restricted);
    RUN_TEST(test_firmware_update_metadata_is_strictly_validated);
    return UNITY_END();
}
