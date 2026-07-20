#include "BatteryMonitor.h"

#include "hardware/BoardProfile.h"

namespace {

adc_attenuation_t adcAttenuation(transitink::hardware::AdcAttenuation attenuation) {
    switch (attenuation) {
        case transitink::hardware::AdcAttenuation::Db0:
            return ADC_0db;
        case transitink::hardware::AdcAttenuation::Db2_5:
            return ADC_2_5db;
        case transitink::hardware::AdcAttenuation::Db6:
            return ADC_6db;
        case transitink::hardware::AdcAttenuation::Db11:
            return ADC_11db;
    }
    return ADC_11db;
}

int inactiveLevel(int activeLevel) {
    return activeLevel == LOW ? HIGH : LOW;
}

}  // namespace

void BatteryMonitor::begin() {
    if (initialized_) {
        return;
    }
    const transitink::hardware::BatteryProfile& battery =
        transitink::hardware::kBoardProfile.battery;
    if (!battery.available) {
        initialized_ = true;
        return;
    }
    if (transitink::hardware::isPinConfigured(battery.sensePowerPin)) {
        pinMode(battery.sensePowerPin, OUTPUT);
        digitalWrite(battery.sensePowerPin, battery.sensePowerActiveLevel);
    }
    if (transitink::hardware::isPinConfigured(battery.chargeDetectPin)) {
        pinMode(battery.chargeDetectPin, INPUT);
    }
    if (transitink::hardware::isPinConfigured(battery.chargeFullPin)) {
        pinMode(battery.chargeFullPin, INPUT);
    }
    if (transitink::hardware::isPinConfigured(battery.chargeIndicatorPin)) {
        const int offLevel = inactiveLevel(battery.chargeIndicatorActiveLevel);
        digitalWrite(battery.chargeIndicatorPin, offLevel);
        pinMode(battery.chargeIndicatorPin, OUTPUT);
        digitalWrite(battery.chargeIndicatorPin, offLevel);
    }
    if (transitink::hardware::isPinConfigured(battery.adcPin)) {
        analogSetPinAttenuation(battery.adcPin, adcAttenuation(battery.attenuation));
    }
    initialized_ = true;
}

bus_eta::BatterySnapshot BatteryMonitor::readChargeState() {
    if (!initialized_) {
        begin();
    }

    const transitink::hardware::BatteryProfile& battery =
        transitink::hardware::kBoardProfile.battery;
    if (!battery.available) {
        return {};
    }

    const bool chargingDetected =
        transitink::hardware::isPinConfigured(battery.chargeDetectPin) &&
        digitalRead(battery.chargeDetectPin) == battery.chargeDetectActiveLevel;
    const bool fullDetected =
        transitink::hardware::isPinConfigured(battery.chargeFullPin) &&
        digitalRead(battery.chargeFullPin) == battery.chargeFullActiveLevel;
    bus_eta::BatterySnapshot snapshot =
        bus_eta::batterySnapshotFromSignals(0, chargingDetected, fullDetected);
    updateChargeIndicator(snapshot);
    return snapshot;
}

bus_eta::BatterySnapshot BatteryMonitor::read() {
    bus_eta::BatterySnapshot chargeState = readChargeState();
    const transitink::hardware::BatteryProfile& battery =
        transitink::hardware::kBoardProfile.battery;
    if (!battery.available ||
        !transitink::hardware::isPinConfigured(battery.adcPin)) {
        return chargeState;
    }

    uint32_t voltageSum = 0;
    for (uint8_t i = 0; i < battery.samples; ++i) {
        voltageSum += analogReadMilliVolts(battery.adcPin) * battery.voltageMultiplier;
        delay(1);
    }

    const uint32_t voltageMv =
        battery.samples == 0 ? 0 : voltageSum / battery.samples;
    bus_eta::BatterySnapshot snapshot = bus_eta::batterySnapshotFromSignals(
        static_cast<int>(voltageMv), chargeState.charging, chargeState.full);
    updateChargeIndicator(snapshot);
    return snapshot;
}

void BatteryMonitor::updateChargeIndicator(
    const bus_eta::BatterySnapshot& snapshot) {
    const transitink::hardware::BatteryProfile& battery =
        transitink::hardware::kBoardProfile.battery;
    if (!transitink::hardware::isPinConfigured(battery.chargeIndicatorPin)) {
        return;
    }
    const bool indicatorOn = bus_eta::chargeIndicatorOn(snapshot, millis());
    const int level = indicatorOn
                          ? battery.chargeIndicatorActiveLevel
                          : inactiveLevel(battery.chargeIndicatorActiveLevel);
    digitalWrite(battery.chargeIndicatorPin, level);
}

void BatteryMonitor::shutdown() {
    const transitink::hardware::BatteryProfile& battery =
        transitink::hardware::kBoardProfile.battery;
    if (transitink::hardware::isPinConfigured(battery.sensePowerPin)) {
        pinMode(battery.sensePowerPin, OUTPUT);
        digitalWrite(battery.sensePowerPin,
                     inactiveLevel(battery.sensePowerActiveLevel));
    }
    if (transitink::hardware::isPinConfigured(battery.chargeIndicatorPin)) {
        digitalWrite(battery.chargeIndicatorPin,
                     inactiveLevel(battery.chargeIndicatorActiveLevel));
    }
    initialized_ = false;
}
