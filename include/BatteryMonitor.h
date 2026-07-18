#pragma once

#include <Arduino.h>

#include "core/BatteryStatus.h"

class BatteryMonitor {
public:
    void begin();
    bus_eta::BatterySnapshot read();
    bus_eta::BatterySnapshot readChargeState();
    void shutdown();

private:
    void updateChargeIndicator(const bus_eta::BatterySnapshot& snapshot);

    bool initialized_ = false;
};
