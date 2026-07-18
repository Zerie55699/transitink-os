#pragma once

#include <cstdint>

namespace transitink::hardware {

constexpr int kUnusedPin = -1;

enum class DisplayDriverKind : uint8_t {
    Ssd1683,
};

enum class PinBias : uint8_t {
    Floating,
    PullUp,
    PullDown,
};

enum class AdcAttenuation : uint8_t {
    Db0,
    Db2_5,
    Db6,
    Db11,
};

struct DisplayProfile {
    DisplayDriverKind driver;
    int width;
    int height;
    int dcPin;
    int chipSelectPin;
    int clockPin;
    int mosiPin;
    int resetPin;
    int busyPin;
    int powerPin;
    int powerActiveLevel;
    int busyActiveLevel;
    uint32_t spiHz;
};

struct BatteryProfile {
    bool available;
    int adcPin;
    uint8_t voltageMultiplier;
    AdcAttenuation attenuation;
    int sensePowerPin;
    int sensePowerActiveLevel;
    int chargeDetectPin;
    int chargeDetectActiveLevel;
    int chargeFullPin;
    int chargeFullActiveLevel;
    int chargeIndicatorPin;
    int chargeIndicatorActiveLevel;
    uint8_t samples;
};

struct ButtonProfile {
    int homePin;
    int upPin;
    int downPin;
    int configPin;
    int factoryResetUpPin;
    int factoryResetDownPin;
    PinBias bias;
    int pressedLevel;
    bool homeSupportsGpioWake;
    bool deinitHomeRtcAfterWake;
    uint32_t factoryResetHoldMs;
    uint32_t configDebounceMs;
    uint32_t configMaxClickMs;
};

struct BoardProfile {
    const char* id;
    const char* displayName;
    DisplayProfile display;
    BatteryProfile battery;
    ButtonProfile buttons;
};

constexpr bool isPinConfigured(int pin) {
    return pin >= 0;
}

}  // namespace transitink::hardware
