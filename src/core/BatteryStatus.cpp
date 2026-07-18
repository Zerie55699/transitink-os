#include "core/BatteryStatus.h"

namespace bus_eta {

int batteryPercentFromMillivolts(int voltageMv) {
    if (voltageMv <= 0) {
        return 0;
    }
    int percent = (-1 * voltageMv * voltageMv + 9016 * voltageMv - 19189000) / 10000;
    if (percent < 0) {
        return 0;
    }
    if (percent > 100) {
        return 100;
    }
    return percent;
}

BatterySnapshot batterySnapshotFromSignals(int voltageMv,
                                           bool chargingDetected,
                                           bool fullDetected) {
    BatterySnapshot snapshot;
    snapshot.valid = voltageMv > 0;
    snapshot.voltageMv = static_cast<uint16_t>(voltageMv > 0 ? voltageMv : 0);

    // CHRG_L is authoritative while LOW. STDBY_H is open-drain and can float
    // HIGH during charging, so it must never override an active charge signal.
    snapshot.charging = chargingDetected;
    snapshot.full = !chargingDetected && fullDetected;
    snapshot.powerPresent = snapshot.charging || snapshot.full;

    if (snapshot.valid) {
        snapshot.percent =
            static_cast<uint8_t>(batteryPercentFromMillivolts(voltageMv));
    }
    if (snapshot.charging && snapshot.percent >= 100) {
        snapshot.percent = 99;
    } else if (snapshot.full) {
        snapshot.percent = 100;
    }
    return snapshot;
}

bool chargeIndicatorOn(const BatterySnapshot& snapshot, uint32_t nowMs) {
    constexpr uint32_t kBlinkHalfPeriodMs = 500;
    return snapshot.full ||
           (snapshot.charging &&
            ((nowMs / kBlinkHalfPeriodMs) % 2U == 0U));
}

}  // namespace bus_eta
