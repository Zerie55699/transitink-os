#include "core/TimeZoneCore.h"

namespace transitink {

const char* deviceTimeZoneId(DeviceTimeZone timeZone) {
    return timeZone == DeviceTimeZone::UnitedKingdom
               ? "Europe/London"
               : "Asia/Hong_Kong";
}

const char* devicePosixTimeZone(DeviceTimeZone timeZone) {
    return timeZone == DeviceTimeZone::UnitedKingdom
               ? "GMT0BST,M3.5.0/1,M10.5.0/2"
               : "HKT-8";
}

bool parseDeviceTimeZoneId(const std::string& value,
                           DeviceTimeZone& timeZone) {
    if (value.empty() || value == "Asia/Hong_Kong") {
        timeZone = DeviceTimeZone::HongKong;
        return true;
    }
    if (value == "Europe/London") {
        timeZone = DeviceTimeZone::UnitedKingdom;
        return true;
    }
    return false;
}

bool isDeviceTimeZoneSupported(DeviceTimeZone timeZone) {
    return timeZone == DeviceTimeZone::HongKong ||
           timeZone == DeviceTimeZone::UnitedKingdom;
}

}  // namespace transitink
