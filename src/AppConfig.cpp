#include "AppConfig.h"

#include <ArduinoJson.h>

#include <memory>
#include <new>

namespace {

String asString(JsonVariantConst value) {
    if (value.isNull()) {
        return "";
    }
    return String(value.as<const char*>());
}

std::string asStdString(JsonVariantConst value) {
    return asString(value).c_str();
}

bool isStringWithinLimit(const String& value, std::size_t limit) {
    return value.length() <= limit;
}

bool areCommonFieldsWithinLimits(const DeviceConfig& config) {
    return isStringWithinLimit(config.wifiSsid, transitink::kMaxWifiSsidBytes) &&
           isStringWithinLimit(config.wifiPassword, transitink::kMaxWifiCredentialBytes) &&
           isStringWithinLimit(config.weatherLocationTc, transitink::kMaxCommonConfigTextBytes);
}

bool isScheduledWakeValid(const DeviceConfig& config) {
    constexpr uint16_t kMinutesPerDay = 24 * 60;
    return config.scheduledWakeStartMinutes < kMinutesPerDay &&
           config.scheduledWakeEndMinutes < kMinutesPerDay &&
           (!config.scheduledWakeEnabled ||
            config.scheduledWakeStartMinutes != config.scheduledWakeEndMinutes);
}

bool areWidgetsValid(const DeviceConfig& config) {
    for (const auto& widget : config.widgets) {
        if (!transitink::isWidgetConfigValid(widget)) {
            return false;
        }
    }
    return true;
}

bool isLegacyRouteWithinLimits(const bus_eta::RouteSelection& route) {
    return route.route.size() <= transitink::kMaxStableIdBytes &&
           route.bound.size() <= transitink::kMaxStableIdBytes &&
           route.serviceType.size() <= transitink::kMaxStableIdBytes &&
           route.stopId.size() <= transitink::kMaxStableIdBytes &&
           route.destTc.size() <= transitink::kMaxConfigLabelBytes;
}

void copyRoute(JsonObjectConst item, bus_eta::RouteSelection& route) {
    route.route = asString(item["route"]).c_str();
    route.bound = asString(item["bound"]).c_str();
    route.serviceType = asString(item["service_type"]).c_str();
    route.stopId = asString(item["stop_id"]).c_str();
    route.destTc = asString(item["dest_tc"]).c_str();
}

void copyCommonFields(const JsonDocument& doc, DeviceConfig& parsed) {
    const std::string localeId = asStdString(doc["ui_locale"]);
    if (!transitink::parseUiLocaleId(localeId, parsed.uiLocale)) {
        parsed.uiLocale = static_cast<transitink::UiLocale>(0xFF);
    }
    const std::string displayFontId = asStdString(doc["display_font"]);
    if (!transitink::parseDisplayFontId(displayFontId, parsed.displayFont)) {
        parsed.displayFont = static_cast<transitink::DisplayFont>(0xFF);
    }
    const std::string timeZoneId = asStdString(doc["time_zone"]);
    if (!transitink::parseDeviceTimeZoneId(timeZoneId, parsed.timeZone)) {
        parsed.timeZone =
            static_cast<transitink::DeviceTimeZone>(0xFF);
    }
    parsed.wifiSsid = asString(doc["wifi_ssid"]);
    parsed.wifiPassword = asString(doc["wifi_password"]);
    parsed.weatherLocationTc = asString(doc["weather_location_tc"]);
    if (parsed.weatherLocationTc.length() == 0) {
        parsed.weatherLocationTc = "香港天文台";
    }
    parsed.sleepEnabled = doc["sleep_enabled"] | static_cast<bool>(SLEEP_ENABLED_DEFAULT);
    parsed.wakeDurationMinutes = doc["wake_duration_minutes"] | SLEEP_WAKE_DEFAULT_MINUTES;
    if (parsed.wakeDurationMinutes < 1) {
        parsed.wakeDurationMinutes = 1;
    }
    if (parsed.wakeDurationMinutes > 60) {
        parsed.wakeDurationMinutes = 60;
    }
    parsed.sleepMaintenanceHours = doc["sleep_maintenance_hours"] | SLEEP_MAINTENANCE_DEFAULT_HOURS;
    if (parsed.sleepMaintenanceHours > 24) {
        parsed.sleepMaintenanceHours = 24;
    }
    parsed.scheduledWakeEnabled = doc.containsKey("scheduled_wake_enabled")
                                      ? doc["scheduled_wake_enabled"].as<bool>()
                                      : static_cast<bool>(SCHEDULED_WAKE_ENABLED_DEFAULT);
    parsed.scheduledWakeStartMinutes =
        doc["scheduled_wake_start_minutes"] | SCHEDULED_WAKE_START_DEFAULT_MINUTES;
    parsed.scheduledWakeEndMinutes =
        doc["scheduled_wake_end_minutes"] | SCHEDULED_WAKE_END_DEFAULT_MINUTES;
}

bool parseWidget(JsonObjectConst item, transitink::WidgetConfig& widget) {
    if (!transitink::parseWidgetTypeId(asStdString(item["type"]), widget.type)) {
        return false;
    }

    switch (widget.type) {
        case transitink::WidgetType::Disabled:
            break;
        case transitink::WidgetType::BusEta: {
            JsonObjectConst bus = item["bus"].as<JsonObjectConst>();
            if (bus.isNull() ||
                !transitink::parseBusOperatorId(asStdString(bus["operator"]), widget.bus.operatorId)) {
                return false;
            }
            widget.bus.routeId = asStdString(bus["route_id"]);
            widget.bus.directionId = asStdString(bus["direction_id"]);
            widget.bus.serviceType = asStdString(bus["service_type"]);
            widget.bus.stopId = asStdString(bus["stop_id"]);
            widget.bus.routeLabelTc = asStdString(bus["route_label_tc"]);
            widget.bus.stopLabelTc = asStdString(bus["stop_label_tc"]);
            widget.bus.destinationLabelTc = asStdString(bus["destination_label_tc"]);
            widget.bus.routeLabelEn = asStdString(bus["route_label_en"]);
            widget.bus.stopLabelEn = asStdString(bus["stop_label_en"]);
            widget.bus.destinationLabelEn = asStdString(bus["destination_label_en"]);
            break;
        }
        case transitink::WidgetType::GmbEta: {
            JsonObjectConst gmb = item["gmb"].as<JsonObjectConst>();
            if (gmb.isNull()) {
                return false;
            }
            widget.gmb.region = asStdString(gmb["region"]);
            widget.gmb.routeCode = asStdString(gmb["route_code"]);
            widget.gmb.routeId = asStdString(gmb["route_id"]);
            widget.gmb.routeSeq = asStdString(gmb["route_seq"]);
            widget.gmb.stopId = asStdString(gmb["stop_id"]);
            widget.gmb.stopSeq = asStdString(gmb["stop_seq"]);
            widget.gmb.routeLabelTc = asStdString(gmb["route_label_tc"]);
            widget.gmb.stopLabelTc = asStdString(gmb["stop_label_tc"]);
            widget.gmb.directionLabelTc = asStdString(gmb["direction_label_tc"]);
            widget.gmb.routeLabelEn = asStdString(gmb["route_label_en"]);
            widget.gmb.stopLabelEn = asStdString(gmb["stop_label_en"]);
            widget.gmb.directionLabelEn = asStdString(gmb["direction_label_en"]);
            break;
        }
        case transitink::WidgetType::MtrEta: {
            JsonObjectConst mtr = item["mtr"].as<JsonObjectConst>();
            if (mtr.isNull() || !transitink::parseRailModeId(asStdString(mtr["mode"]), widget.mtr.mode)) {
                return false;
            }
            widget.mtr.lineOrRouteId = asStdString(mtr["line_or_route_id"]);
            widget.mtr.stationId = asStdString(mtr["station_id"]);
            widget.mtr.directionId = asStdString(mtr["direction_id"]);
            widget.mtr.lineOrRouteLabelTc = asStdString(mtr["line_or_route_label_tc"]);
            widget.mtr.stationLabelTc = asStdString(mtr["station_label_tc"]);
            widget.mtr.directionLabelTc = asStdString(mtr["direction_label_tc"]);
            widget.mtr.lineOrRouteLabelEn = asStdString(mtr["line_or_route_label_en"]);
            widget.mtr.stationLabelEn = asStdString(mtr["station_label_en"]);
            widget.mtr.directionLabelEn = asStdString(mtr["direction_label_en"]);
            break;
        }
        case transitink::WidgetType::JourneyTime: {
            JsonObjectConst journeyTime = item["journey_time"].as<JsonObjectConst>();
            if (journeyTime.isNull()) {
                return false;
            }
            widget.journeyTime.locationId = asStdString(journeyTime["location_id"]);
            widget.journeyTime.destinationId = asStdString(journeyTime["destination_id"]);
            widget.journeyTime.locationLabelTc = asStdString(journeyTime["location_label_tc"]);
            widget.journeyTime.destinationLabelTc = asStdString(journeyTime["destination_label_tc"]);
            widget.journeyTime.locationLabelEn = asStdString(journeyTime["location_label_en"]);
            widget.journeyTime.destinationLabelEn =
                asStdString(journeyTime["destination_label_en"]);
            break;
        }
    }

    return transitink::isWidgetConfigValid(widget);
}

void writeWidget(JsonObject item, const transitink::WidgetConfig& widget) {
    item["type"] = transitink::widgetTypeId(widget.type);
    switch (widget.type) {
        case transitink::WidgetType::Disabled:
            break;
        case transitink::WidgetType::BusEta: {
            JsonObject bus = item.createNestedObject("bus");
            bus["operator"] = transitink::busOperatorId(widget.bus.operatorId);
            bus["route_id"] = widget.bus.routeId.c_str();
            bus["direction_id"] = widget.bus.directionId.c_str();
            bus["service_type"] = widget.bus.serviceType.c_str();
            bus["stop_id"] = widget.bus.stopId.c_str();
            bus["route_label_tc"] = widget.bus.routeLabelTc.c_str();
            bus["stop_label_tc"] = widget.bus.stopLabelTc.c_str();
            bus["destination_label_tc"] = widget.bus.destinationLabelTc.c_str();
            bus["route_label_en"] = widget.bus.routeLabelEn.c_str();
            bus["stop_label_en"] = widget.bus.stopLabelEn.c_str();
            bus["destination_label_en"] = widget.bus.destinationLabelEn.c_str();
            break;
        }
        case transitink::WidgetType::GmbEta: {
            JsonObject gmb = item.createNestedObject("gmb");
            gmb["region"] = widget.gmb.region.c_str();
            gmb["route_code"] = widget.gmb.routeCode.c_str();
            gmb["route_id"] = widget.gmb.routeId.c_str();
            gmb["route_seq"] = widget.gmb.routeSeq.c_str();
            gmb["stop_id"] = widget.gmb.stopId.c_str();
            gmb["stop_seq"] = widget.gmb.stopSeq.c_str();
            gmb["route_label_tc"] = widget.gmb.routeLabelTc.c_str();
            gmb["stop_label_tc"] = widget.gmb.stopLabelTc.c_str();
            gmb["direction_label_tc"] = widget.gmb.directionLabelTc.c_str();
            gmb["route_label_en"] = widget.gmb.routeLabelEn.c_str();
            gmb["stop_label_en"] = widget.gmb.stopLabelEn.c_str();
            gmb["direction_label_en"] = widget.gmb.directionLabelEn.c_str();
            break;
        }
        case transitink::WidgetType::MtrEta: {
            JsonObject mtr = item.createNestedObject("mtr");
            mtr["mode"] = transitink::railModeId(widget.mtr.mode);
            mtr["line_or_route_id"] = widget.mtr.lineOrRouteId.c_str();
            mtr["station_id"] = widget.mtr.stationId.c_str();
            mtr["direction_id"] = widget.mtr.directionId.c_str();
            mtr["line_or_route_label_tc"] = widget.mtr.lineOrRouteLabelTc.c_str();
            mtr["station_label_tc"] = widget.mtr.stationLabelTc.c_str();
            mtr["direction_label_tc"] = widget.mtr.directionLabelTc.c_str();
            mtr["line_or_route_label_en"] = widget.mtr.lineOrRouteLabelEn.c_str();
            mtr["station_label_en"] = widget.mtr.stationLabelEn.c_str();
            mtr["direction_label_en"] = widget.mtr.directionLabelEn.c_str();
            break;
        }
        case transitink::WidgetType::JourneyTime: {
            JsonObject journeyTime = item.createNestedObject("journey_time");
            journeyTime["location_id"] = widget.journeyTime.locationId.c_str();
            journeyTime["destination_id"] = widget.journeyTime.destinationId.c_str();
            journeyTime["location_label_tc"] = widget.journeyTime.locationLabelTc.c_str();
            journeyTime["destination_label_tc"] = widget.journeyTime.destinationLabelTc.c_str();
            journeyTime["location_label_en"] = widget.journeyTime.locationLabelEn.c_str();
            journeyTime["destination_label_en"] =
                widget.journeyTime.destinationLabelEn.c_str();
            break;
        }
    }
}

}  // namespace

bool parseDeviceConfigJson(const String& json, DeviceConfig& config, String& error) {
    if (json.length() > transitink::kConfigJsonCapacity) {
        error = "設定內容過大";
        return false;
    }

    DynamicJsonDocument doc(transitink::kConfigJsonCapacity);
    DeserializationError jsonError = deserializeJson(doc, json);
    if (jsonError || doc.overflowed()) {
        error = String("設定格式錯誤: ") + jsonError.c_str();
        return false;
    }

    std::unique_ptr<DeviceConfig> parsed(new (std::nothrow) DeviceConfig());
    if (!parsed) {
        error = "設定記憶體不足";
        return false;
    }
    copyCommonFields(doc, *parsed);
    if (!transitink::isUiLocaleSupported(parsed->uiLocale)) {
        error = "介面語言設定不正確";
        return false;
    }
    if (!transitink::isDisplayFontSupported(parsed->displayFont)) {
        error = "顯示字型設定不正確";
        return false;
    }
    if (!transitink::isDeviceTimeZoneSupported(parsed->timeZone)) {
        error = "時區設定不正確";
        return false;
    }
    if (!areCommonFieldsWithinLimits(*parsed)) {
        error = "共用設定欄位過長";
        return false;
    }
    if (!isScheduledWakeValid(*parsed)) {
        error = "每日定時喚醒時段無效";
        return false;
    }

    if (doc.containsKey("schema_version")) {
        const uint16_t schemaVersion = doc["schema_version"].as<uint16_t>();
        if (schemaVersion != transitink::kConfigSchemaVersion &&
            schemaVersion != transitink::kPreviousConfigSchemaVersion) {
            error = "不支援的設定版本";
            return false;
        }
        JsonArrayConst widgets = doc["widgets"].as<JsonArrayConst>();
        const std::size_t expectedWidgetCount =
            schemaVersion == transitink::kPreviousConfigSchemaVersion
                ? transitink::kWidgetsPerPage
                : transitink::kWidgetSlotCount;
        if (widgets.isNull() || widgets.size() != expectedWidgetCount) {
            error = "小工具數量不正確";
            return false;
        }
        std::size_t index = 0;
        for (JsonObjectConst item : widgets) {
            if (item.isNull() || !parseWidget(item, parsed->widgets[index])) {
                error = "小工具設定無效";
                return false;
            }
            ++index;
        }
    } else {
        if (!doc.containsKey("routes")) {
            error = "缺少設定版本";
            return false;
        }
        JsonArrayConst routes = doc["routes"].as<JsonArrayConst>();
        if (routes.isNull()) {
            error = "舊版路線格式錯誤";
            return false;
        }
        String legacyStopNameTc = asString(doc["stop_name_tc"]);
        if (!isStringWithinLimit(legacyStopNameTc, transitink::kMaxCommonConfigTextBytes)) {
            error = "舊版站名過長";
            return false;
        }
        const uint16_t legacyRefreshSeconds =
            doc["refresh_seconds"] | ETA_REFRESH_DEFAULT_SECONDS;
        (void)legacyRefreshSeconds;
        std::vector<bus_eta::RouteSelection> legacyRoutes;
        for (JsonObjectConst item : routes) {
            if (legacyRoutes.size() >= transitink::kWidgetsPerPage) {
                break;
            }
            bus_eta::RouteSelection route;
            copyRoute(item, route);
            if (!isLegacyRouteWithinLimits(route)) {
                error = "舊版路線欄位過長";
                return false;
            }
            if (!route.route.empty() && !route.bound.empty() && !route.serviceType.empty() &&
                !route.stopId.empty()) {
                legacyRoutes.push_back(route);
            }
        }
        parsed->widgets =
            transitink::migrateLegacyRoutes(legacyRoutes, legacyStopNameTc.c_str());
    }

    parsed->schemaVersion = transitink::kConfigSchemaVersion;
    config = *parsed;
    error = "";
    return true;
}

String serializeDeviceConfigJson(const DeviceConfig& config) {
    DeviceConfigSerializationMetrics metrics;
    String json;
    String error;
    if (!serializeDeviceConfigJsonChecked(config, json, metrics, error)) {
        return "";
    }
    return json;
}

bool serializeDeviceConfigJsonChecked(const DeviceConfig& config,
                                      String& json,
                                      DeviceConfigSerializationMetrics& metrics,
                                      String& error) {
    json = "";
    metrics = DeviceConfigSerializationMetrics{};
    error = "";
    if (config.schemaVersion != transitink::kConfigSchemaVersion) {
        error = "不支援的設定版本";
        return false;
    }
    if (!transitink::isUiLocaleSupported(config.uiLocale)) {
        error = "介面語言設定不正確";
        return false;
    }
    if (!transitink::isDisplayFontSupported(config.displayFont)) {
        error = "顯示字型設定不正確";
        return false;
    }
    if (!transitink::isDeviceTimeZoneSupported(config.timeZone)) {
        error = "時區設定不正確";
        return false;
    }
    if (!areCommonFieldsWithinLimits(config)) {
        error = "共用設定欄位過長";
        return false;
    }
    if (!isScheduledWakeValid(config)) {
        error = "每日定時喚醒時段無效";
        return false;
    }
    if (!areWidgetsValid(config)) {
        error = "小工具設定無效";
        return false;
    }

    DynamicJsonDocument doc(transitink::kConfigJsonCapacity);
    doc["schema_version"] = transitink::kConfigSchemaVersion;
    doc["ui_locale"] = transitink::uiLocaleId(config.uiLocale);
    doc["display_font"] = transitink::displayFontId(config.displayFont);
    doc["time_zone"] = transitink::deviceTimeZoneId(config.timeZone);
    doc["wifi_ssid"] = config.wifiSsid;
    doc["wifi_password"] = config.wifiPassword;
    doc["weather_location_tc"] = config.weatherLocationTc;
    doc["sleep_enabled"] = config.sleepEnabled;
    doc["wake_duration_minutes"] = config.wakeDurationMinutes;
    doc["sleep_maintenance_hours"] = config.sleepMaintenanceHours;
    doc["scheduled_wake_enabled"] = config.scheduledWakeEnabled;
    doc["scheduled_wake_start_minutes"] = config.scheduledWakeStartMinutes;
    doc["scheduled_wake_end_minutes"] = config.scheduledWakeEndMinutes;
    JsonArray widgets = doc.createNestedArray("widgets");
    for (const auto& widget : config.widgets) {
        JsonObject item = widgets.createNestedObject();
        writeWidget(item, widget);
    }
    if (doc.overflowed()) {
        error = "設定內容超出 JSON 容量";
        return false;
    }

    metrics.documentBytes = doc.memoryUsage();
    metrics.jsonBytes = measureJson(doc);
    if (metrics.documentBytes > transitink::kConfigJsonSafeBytes ||
        metrics.jsonBytes > transitink::kConfigBlobSafeBytes) {
        error = "設定內容超出 JSON 安全容量";
        return false;
    }
    const std::size_t written = serializeJson(doc, json);
    if (written != metrics.jsonBytes || json.length() != metrics.jsonBytes) {
        json = "";
        error = "設定序列化不完整";
        return false;
    }
    return true;
}

bool hasUsableConfig(const DeviceConfig& config) {
    if (config.schemaVersion != transitink::kConfigSchemaVersion || config.wifiSsid.length() == 0 ||
        !transitink::isUiLocaleSupported(config.uiLocale) ||
        !transitink::isDisplayFontSupported(config.displayFont) ||
        !transitink::isDeviceTimeZoneSupported(config.timeZone) ||
        !areCommonFieldsWithinLimits(config) || !isScheduledWakeValid(config)) {
        return false;
    }
    return areWidgetsValid(config);
}
