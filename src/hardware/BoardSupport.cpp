#include "hardware/BoardSupport.h"

#include <Arduino.h>
#include <esp_sleep.h>

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "hardware/BoardProfile.h"

namespace transitink::hardware {
namespace {

void configureButtonPin(int pin) {
    if (!isPinConfigured(pin)) {
        return;
    }
    switch (kBoardProfile.buttons.bias) {
        case PinBias::PullUp:
            pinMode(pin, INPUT_PULLUP);
            break;
        case PinBias::PullDown:
            pinMode(pin, INPUT_PULLDOWN);
            break;
        case PinBias::Floating:
            pinMode(pin, INPUT);
            break;
    }
}

bool buttonPressed(int pin) {
    return isPinConfigured(pin) &&
           digitalRead(pin) == kBoardProfile.buttons.pressedLevel;
}

}  // namespace

void configureButtons() {
    const ButtonProfile& buttons = kBoardProfile.buttons;
    if (buttons.deinitHomeRtcAfterWake && isPinConfigured(buttons.homePin)) {
        rtc_gpio_deinit(static_cast<gpio_num_t>(buttons.homePin));
    }
    configureButtonPin(buttons.homePin);
    configureButtonPin(buttons.upPin);
    configureButtonPin(buttons.downPin);
    configureButtonPin(buttons.configPin);
    configureButtonPin(buttons.factoryResetUpPin);
    configureButtonPin(buttons.factoryResetDownPin);
}

bool homeButtonPressed() {
    return buttonPressed(kBoardProfile.buttons.homePin);
}

bool configButtonPressed() {
    return buttonPressed(kBoardProfile.buttons.configPin);
}

bool factoryResetUpButtonPressed() {
    return buttonPressed(kBoardProfile.buttons.factoryResetUpPin);
}

bool factoryResetDownButtonPressed() {
    return buttonPressed(kBoardProfile.buttons.factoryResetDownPin);
}

void configureHomeWakeup() {
    const ButtonProfile& buttons = kBoardProfile.buttons;
    if (!buttons.homeSupportsGpioWake || !isPinConfigured(buttons.homePin)) {
        return;
    }
    gpio_wakeup_disable(static_cast<gpio_num_t>(buttons.homePin));
    configureButtonPin(buttons.homePin);
    const gpio_int_type_t trigger =
        buttons.pressedLevel == LOW ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
    gpio_wakeup_enable(static_cast<gpio_num_t>(buttons.homePin), trigger);
    esp_sleep_enable_gpio_wakeup();
}

void disableHomeWakeup() {
    const ButtonProfile& buttons = kBoardProfile.buttons;
    if (buttons.homeSupportsGpioWake && isPinConfigured(buttons.homePin)) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(buttons.homePin));
    }
}

void configureChargeWakeup() {
    const BatteryProfile& battery = kBoardProfile.battery;
    if (!isPinConfigured(battery.chargeDetectPin)) {
        return;
    }
    const gpio_num_t pin = static_cast<gpio_num_t>(battery.chargeDetectPin);
    gpio_wakeup_disable(pin);
    pinMode(battery.chargeDetectPin, INPUT);
    const gpio_int_type_t trigger = battery.chargeDetectActiveLevel == LOW
                                        ? GPIO_INTR_LOW_LEVEL
                                        : GPIO_INTR_HIGH_LEVEL;
    gpio_wakeup_enable(pin, trigger);
    esp_sleep_enable_gpio_wakeup();
}

void disableChargeWakeup() {
    const BatteryProfile& battery = kBoardProfile.battery;
    if (isPinConfigured(battery.chargeDetectPin)) {
        gpio_wakeup_disable(static_cast<gpio_num_t>(battery.chargeDetectPin));
    }
}

}  // namespace transitink::hardware
