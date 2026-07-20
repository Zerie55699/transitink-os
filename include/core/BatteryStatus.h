#pragma once

#include <cstdint>

namespace bus_eta {

struct BatterySnapshot {
    bool valid = false;
    uint8_t percent = 0;
    uint16_t voltageMv = 0;
    bool powerPresent = false;
    bool charging = false;
    bool full = false;
};

int batteryPercentFromMillivolts(int voltageMv);
BatterySnapshot batterySnapshotFromSignals(int voltageMv,
                                           bool chargingDetected,
                                           bool fullDetected);
bool chargeIndicatorOn(const BatterySnapshot& snapshot, uint32_t nowMs);

}  // namespace bus_eta
