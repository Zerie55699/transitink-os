#include "PortalConfigCodec.h"

#include <ArduinoJson.h>

#include <memory>
#include <new>

namespace {

void writeWidget(JsonObject item, const transitink::WidgetConfig& widget) {
    item["type"] = transitink::widgetTypeId(widget.type);
    if (widget.type == transitink::WidgetType::BusEta) {
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
    } else if (widget.type == transitink::WidgetType::GmbEta) {
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
    } else if (widget.type == transitink::WidgetType::MtrEta) {
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
    } else if (widget.type == transitink::WidgetType::JourneyTime) {
        JsonObject journey = item.createNestedObject("journey_time");
        journey["location_id"] = widget.journeyTime.locationId.c_str();
        journey["destination_id"] = widget.journeyTime.destinationId.c_str();
        journey["location_label_tc"] = widget.journeyTime.locationLabelTc.c_str();
        journey["destination_label_tc"] = widget.journeyTime.destinationLabelTc.c_str();
        journey["location_label_en"] = widget.journeyTime.locationLabelEn.c_str();
        journey["destination_label_en"] = widget.journeyTime.destinationLabelEn.c_str();
    }
}

}  // namespace

bool encodePortalConfig(const DeviceConfig& config,
                        const bus_eta::BatterySnapshot& battery,
                        const String& firmwareVersion,
                        const String& csrfToken,
                        String& outJson,
                        String& error) {
    outJson = "";
    DynamicJsonDocument doc(transitink::kConfigJsonCapacity);
    doc["schema_version"] = transitink::kConfigSchemaVersion;
    doc["ui_locale"] = transitink::uiLocaleId(config.uiLocale);
    doc["display_font"] = transitink::displayFontId(config.displayFont);
    doc["time_zone"] = transitink::deviceTimeZoneId(config.timeZone);
    doc["wifi_ssid"] = config.wifiSsid;
    doc["wifi_password_set"] = config.wifiPassword.length() > 0;
    doc["weather_location_tc"] = config.weatherLocationTc;
    doc["sleep_enabled"] = config.sleepEnabled;
    doc["wake_duration_minutes"] = config.wakeDurationMinutes;
    doc["sleep_maintenance_hours"] = config.sleepMaintenanceHours;
    doc["scheduled_wake_enabled"] = config.scheduledWakeEnabled;
    doc["scheduled_wake_start_minutes"] = config.scheduledWakeStartMinutes;
    doc["scheduled_wake_end_minutes"] = config.scheduledWakeEndMinutes;
    doc["firmware_version"] = firmwareVersion;
    doc["csrf_token"] = csrfToken;

    JsonArray widgets = doc.createNestedArray("widgets");
    for (const auto& widget : config.widgets) {
        writeWidget(widgets.createNestedObject(), widget);
    }

    JsonObject batteryJson = doc.createNestedObject("battery");
    batteryJson["valid"] = battery.valid;
    batteryJson["percent"] = battery.percent;
    batteryJson["voltage_mv"] = battery.voltageMv;
    batteryJson["charging"] = battery.charging;
    batteryJson["full"] = battery.full;
    batteryJson["power_present"] = battery.powerPresent;

    if (doc.overflowed() || measureJson(doc) > transitink::kConfigJsonSafeBytes) {
        error = "設定內容過大";
        return false;
    }
    if (serializeJson(doc, outJson) == 0) {
        error = "設定 JSON 產生失敗";
        return false;
    }
    error = "";
    return true;
}

bool decodePortalSave(const String& body,
                      const DeviceConfig& current,
                      DeviceConfig& outConfig,
                      String& error) {
    if (body.length() > transitink::kConfigJsonCapacity) {
        error = "設定內容過大";
        return false;
    }

    DynamicJsonDocument doc(transitink::kConfigJsonCapacity);
    const DeserializationError jsonError = deserializeJson(doc, body);
    if (jsonError || doc.overflowed() || !doc.is<JsonObject>()) {
        error = "設定格式錯誤";
        return false;
    }

    JsonVariant password = doc["wifi_password"];
    if (!password.isNull() && !password.is<const char*>()) {
        error = "Wi-Fi 密碼格式錯誤";
        return false;
    }
    const char* submittedPassword = password | "";
    if (submittedPassword[0] == '\0') {
        doc["wifi_password"] = current.wifiPassword;
    }

    String merged;
    if (serializeJson(doc, merged) == 0 || merged.length() > transitink::kConfigJsonCapacity) {
        error = "設定內容過大";
        return false;
    }
    return parseDeviceConfigJson(merged, outConfig, error);
}

bool savePortalConfig(const String& body,
                      DeviceConfig& liveConfig,
                      ConfigStore& store,
                      String& error) {
    std::unique_ptr<DeviceConfig> parsed(new (std::nothrow) DeviceConfig());
    if (!parsed) {
        error = "設定記憶體不足";
        return false;
    }
    if (!decodePortalSave(body, liveConfig, *parsed, error)) {
        return false;
    }
    if (!store.save(*parsed)) {
        error = "儲存失敗";
        return false;
    }
    liveConfig = *parsed;
    error = "";
    return true;
}
