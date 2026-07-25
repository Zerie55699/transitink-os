#pragma once

#include <cstdint>
#include <string>

namespace transitink {

enum class DeviceTimeZone : uint8_t {
    HongKong,
    UnitedKingdom,
};

const char* deviceTimeZoneId(DeviceTimeZone timeZone);
const char* devicePosixTimeZone(DeviceTimeZone timeZone);
bool parseDeviceTimeZoneId(const std::string& value, DeviceTimeZone& timeZone);
bool isDeviceTimeZoneSupported(DeviceTimeZone timeZone);

}  // namespace transitink
