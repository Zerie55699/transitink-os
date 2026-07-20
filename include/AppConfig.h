#pragma once

#include <Arduino.h>
#include "ProductConfig.h"
#include "core/WidgetConfigCore.h"

namespace transitink {

constexpr std::size_t kConfigJsonCapacity = 8192;
constexpr std::size_t kConfigJsonSafeBytes = kConfigJsonCapacity * 4 / 5;
constexpr std::size_t kMaxWifiSsidBytes = 32;
constexpr std::size_t kMaxWifiCredentialBytes = 64;
constexpr std::size_t kMaxCommonConfigTextBytes = 96;

}  // namespace transitink

struct DeviceConfig {
    uint16_t schemaVersion = transitink::kConfigSchemaVersion;
    String wifiSsid;
    String wifiPassword;
    String weatherLocationTc = "香港天文台";
    bool sleepEnabled = SLEEP_ENABLED_DEFAULT;
    uint16_t wakeDurationMinutes = SLEEP_WAKE_DEFAULT_MINUTES;
    uint16_t sleepMaintenanceHours = SLEEP_MAINTENANCE_DEFAULT_HOURS;
    transitink::WidgetSlots widgets{};
};

struct DeviceConfigSerializationMetrics {
    std::size_t documentBytes = 0;
    std::size_t jsonBytes = 0;
};

bool parseDeviceConfigJson(const String& json, DeviceConfig& config, String& error);
String serializeDeviceConfigJson(const DeviceConfig& config);
bool serializeDeviceConfigJsonChecked(const DeviceConfig& config,
                                      String& json,
                                      DeviceConfigSerializationMetrics& metrics,
                                      String& error);
bool hasUsableConfig(const DeviceConfig& config);
