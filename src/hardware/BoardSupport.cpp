#include "hardware/BoardSupport.h"

#include <Arduino.h>
#include <esp_sleep.h>

#include <atomic>

#include "core/BusEtaCore.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

std::atomic<bool> configClickPending{false};
std::atomic<bool> widgetPageClickPending{false};
std::atomic<bool> factoryResetHoldPending{false};
std::atomic<bool> homePressPending{false};
TaskHandle_t buttonMonitorTaskHandle = nullptr;
bus_eta::DualButtonHoldDetector factoryResetDetector(
    kBoardProfile.buttons.factoryResetHoldMs);
bus_eta::SingleButtonClickDetector configButtonDetector(
    kBoardProfile.buttons.configDebounceMs,
    kBoardProfile.buttons.configMaxClickMs);
bus_eta::SingleButtonClickDetector widgetPageButtonDetector(
    kBoardProfile.buttons.configDebounceMs,
    kBoardProfile.buttons.configMaxClickMs);
bus_eta::DebouncedButtonPressDetector homeButtonDetector(
    kBoardProfile.buttons.configDebounceMs);

void monitorButtons(void*) {
    while (true) {
        const unsigned long nowMs = millis();
        const bool upPressed = buttonPressed(kBoardProfile.buttons.factoryResetUpPin);
        const bool downPressed = buttonPressed(kBoardProfile.buttons.factoryResetDownPin);
        const bool configPressed = buttonPressed(kBoardProfile.buttons.configPin);
        if (factoryResetDetector.update(upPressed, downPressed, nowMs)) {
            factoryResetHoldPending.store(true, std::memory_order_relaxed);
        }
        if (configButtonDetector.update(configPressed, downPressed, nowMs)) {
            configClickPending.store(true, std::memory_order_relaxed);
        }
        if (widgetPageButtonDetector.update(downPressed, upPressed, nowMs)) {
            widgetPageClickPending.store(true, std::memory_order_relaxed);
        }
        if (homeButtonDetector.update(
                buttonPressed(kBoardProfile.buttons.homePin), nowMs)) {
            homePressPending.store(true, std::memory_order_relaxed);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
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

bool startButtonMonitoring() {
    if (buttonMonitorTaskHandle != nullptr) {
        return true;
    }
    return xTaskCreate(
               monitorButtons,
               "buttons",
               2048,
               nullptr,
               2,
               &buttonMonitorTaskHandle) == pdPASS;
}

bool homeButtonPressed() {
    return buttonPressed(kBoardProfile.buttons.homePin);
}

bool factoryResetUpButtonPressed() {
    return buttonPressed(kBoardProfile.buttons.factoryResetUpPin);
}

bool factoryResetDownButtonPressed() {
    return buttonPressed(kBoardProfile.buttons.factoryResetDownPin);
}

bool takeConfigClick() {
    return configClickPending.exchange(false, std::memory_order_relaxed);
}

void clearPendingConfigClick() {
    configClickPending.store(false, std::memory_order_relaxed);
}

bool takeWidgetPageClick() {
    return widgetPageClickPending.exchange(false, std::memory_order_relaxed);
}

void clearPendingWidgetPageClick() {
    widgetPageClickPending.store(false, std::memory_order_relaxed);
}

bool takeFactoryResetHold() {
    return factoryResetHoldPending.exchange(false, std::memory_order_relaxed);
}

bool takeHomePress() {
    return homePressPending.exchange(false, std::memory_order_relaxed);
}

void clearPendingHomePress() {
    homePressPending.store(false, std::memory_order_relaxed);
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

}  // namespace transitink::hardware
