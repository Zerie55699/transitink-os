#include "hardware/displays/Ssd1683DisplayDriver.h"

#include <Arduino.h>
#include <SPI.h>
#include <driver/gpio.h>

#include <algorithm>

#include "hardware/BoardProfile.h"

namespace transitink::hardware {
namespace {

constexpr int kPanelWidth = kBoardProfile.display.width;
constexpr int kPanelHeight = kBoardProfile.display.height;
constexpr size_t kBytesPerRow = static_cast<size_t>(kPanelWidth / 8);

int inactiveLevel(int activeLevel) {
    return activeLevel == LOW ? HIGH : LOW;
}

}  // namespace

void Ssd1683DisplayDriver::begin() {
    const DisplayProfile& display = kBoardProfile.display;
    if (isPinConfigured(display.powerPin)) {
        pinMode(display.powerPin, OUTPUT);
    }
    pinMode(display.chipSelectPin, OUTPUT);
    pinMode(display.dcPin, OUTPUT);
    pinMode(display.resetPin, OUTPUT);
    pinMode(display.busyPin, INPUT);
    digitalWrite(display.chipSelectPin, HIGH);
    digitalWrite(display.dcPin, HIGH);
    digitalWrite(display.resetPin, HIGH);
    powerOn();
    SPI.begin(display.clockPin, -1, display.mosiPin, display.chipSelectPin);
}

void Ssd1683DisplayDriver::show(const uint8_t* buffer) {
    initPanel();
    displayFrame(buffer);
}

void Ssd1683DisplayDriver::showPartialRegion(const uint8_t* current,
                                             const uint8_t* previous,
                                             int x,
                                             int y,
                                             int width,
                                             int height) {
    initPanel();
    displayPartialFrameRegion(current, previous, x, y, width, height);
}

void Ssd1683DisplayDriver::powerOn() {
    const DisplayProfile& display = kBoardProfile.display;
    if (isPinConfigured(display.powerPin)) {
        const gpio_num_t powerPin =
            static_cast<gpio_num_t>(display.powerPin);
        gpio_hold_dis(powerPin);
        digitalWrite(display.powerPin, display.powerActiveLevel);
        gpio_hold_en(powerPin);
        delay(10);
    }
}

void Ssd1683DisplayDriver::powerOff() {
    const DisplayProfile& display = kBoardProfile.display;
    if (isPinConfigured(display.powerPin)) {
        const gpio_num_t powerPin =
            static_cast<gpio_num_t>(display.powerPin);
        gpio_hold_dis(powerPin);
        digitalWrite(display.powerPin, inactiveLevel(display.powerActiveLevel));
        gpio_hold_en(powerPin);
    }
}

void Ssd1683DisplayDriver::reset() {
    const int resetPin = kBoardProfile.display.resetPin;
    digitalWrite(resetPin, HIGH);
    delay(10);
    digitalWrite(resetPin, LOW);
    delay(20);
    digitalWrite(resetPin, HIGH);
    delay(10);
}

bool Ssd1683DisplayDriver::waitBusy(const char* label, uint32_t timeoutMs) {
    const DisplayProfile& display = kBoardProfile.display;
    unsigned long start = millis();
    while (digitalRead(display.busyPin) == display.busyActiveLevel) {
        delay(5);
        if (millis() - start > timeoutMs) {
            Serial.print("EPD busy timeout: ");
            Serial.println(label);
            return false;
        }
    }
    return true;
}

void Ssd1683DisplayDriver::sendCommand(uint8_t command) {
    const DisplayProfile& display = kBoardProfile.display;
    SPI.beginTransaction(SPISettings(display.spiHz, MSBFIRST, SPI_MODE0));
    digitalWrite(display.dcPin, LOW);
    digitalWrite(display.chipSelectPin, LOW);
    SPI.transfer(command);
    digitalWrite(display.chipSelectPin, HIGH);
    digitalWrite(display.dcPin, HIGH);
    SPI.endTransaction();
}

void Ssd1683DisplayDriver::sendData(uint8_t data) {
    const DisplayProfile& display = kBoardProfile.display;
    SPI.beginTransaction(SPISettings(display.spiHz, MSBFIRST, SPI_MODE0));
    digitalWrite(display.dcPin, HIGH);
    digitalWrite(display.chipSelectPin, LOW);
    SPI.transfer(data);
    digitalWrite(display.chipSelectPin, HIGH);
    SPI.endTransaction();
}

void Ssd1683DisplayDriver::writeBytes(const uint8_t* data, size_t length) {
    const DisplayProfile& display = kBoardProfile.display;
    SPI.beginTransaction(SPISettings(display.spiHz, MSBFIRST, SPI_MODE0));
    digitalWrite(display.dcPin, HIGH);
    digitalWrite(display.chipSelectPin, LOW);
    for (size_t index = 0; index < length; ++index) {
        SPI.transfer(data[index]);
    }
    digitalWrite(display.chipSelectPin, HIGH);
    SPI.endTransaction();
}

void Ssd1683DisplayDriver::initPanel() {
    powerOn();
    reset();
    waitBusy("reset");
    sendCommand(0x00);
    sendData(0x2F);
    sendData(0x2E);
    sendCommand(0xE9);
    sendData(0x01);
    waitBusy("e9");
}

void Ssd1683DisplayDriver::pack1bpp(uint8_t input,
                                    uint8_t& output0,
                                    uint8_t& output1) {
    uint8_t first = 0;
    uint8_t second = 0;
    for (uint8_t index = 0; index < 8; ++index) {
        const uint8_t bit = (input >> (7 - index)) & 0x01;
        if (index < 4) {
            first |= bit << (8 - 2 * (index + 1));
        } else {
            second |= bit << (14 - 2 * index);
        }
    }
    output0 = first;
    output1 = second;
}

void Ssd1683DisplayDriver::packPartialTransition(uint8_t previousByte,
                                                 uint8_t currentByte,
                                                 uint8_t& output0,
                                                 uint8_t& output1) {
    uint16_t packed = 0;
    for (uint8_t index = 0; index < 8; ++index) {
        const int sourceBit = 7 - index;
        const int currentBit = 2 * sourceBit;
        const int previousBit = currentBit + 1;
        packed |= static_cast<uint16_t>(
            ((previousByte >> sourceBit) & 0x01) << previousBit);
        packed |= static_cast<uint16_t>(
            ((currentByte >> sourceBit) & 0x01) << currentBit);
    }
    output0 = static_cast<uint8_t>(packed >> 8);
    output1 = static_cast<uint8_t>(packed & 0xFF);
}

void Ssd1683DisplayDriver::displayFrame(const uint8_t* buffer) {
    uint8_t line[kBytesPerRow * 2];

    sendCommand(0xE0);
    sendData(0x02);
    sendCommand(0xE6);
    sendData(238);
    sendCommand(0xA5);
    waitBusy("temperature");
    delay(10);

    sendCommand(0x10);
    for (int y = 0; y < kPanelHeight; ++y) {
        const uint8_t* source =
            buffer + static_cast<size_t>(y) * kBytesPerRow;
        for (size_t byte = 0; byte < kBytesPerRow; ++byte) {
            pack1bpp(source[byte], line[byte * 2], line[byte * 2 + 1]);
        }
        writeBytes(line, sizeof(line));
    }

    turnOnDisplay();
}

void Ssd1683DisplayDriver::displayPartialFrameRegion(
    const uint8_t* currentFrame,
    const uint8_t* previousFrame,
    int x,
    int y,
    int width,
    int height) {
    uint8_t line[kBytesPerRow * 2];
    const int clampedX = std::max(0, x);
    const int clampedY = std::max(0, y);
    const int xEnd = std::min(kPanelWidth, x + width);
    const int yEnd = std::min(kPanelHeight, y + height);
    const int alignedX = (clampedX / 8) * 8;
    const int alignedEnd = std::min(kPanelWidth, ((xEnd + 7) / 8) * 8);
    const int alignedWidth = alignedEnd - alignedX;
    const int regionHeight = yEnd - clampedY;
    if (alignedWidth <= 0 || regionHeight <= 0) {
        return;
    }

    const size_t dirtyStartByte = static_cast<size_t>(alignedX / 8);
    const size_t dirtyEndByte =
        dirtyStartByte + static_cast<size_t>(alignedWidth / 8);
    sendCommand(0x10);
    waitBusy("partial_ram");
    for (int row = 0; row < kPanelHeight; ++row) {
        const bool dirtyRow = row >= clampedY && row < yEnd;
        const uint8_t* previous =
            previousFrame + static_cast<size_t>(row) * kBytesPerRow;
        const uint8_t* current =
            currentFrame + static_cast<size_t>(row) * kBytesPerRow;
        for (size_t byte = 0; byte < kBytesPerRow; ++byte) {
            const bool dirtyByte =
                dirtyRow && byte >= dirtyStartByte && byte < dirtyEndByte;
            if (dirtyByte) {
                packPartialTransition(previous[byte], current[byte],
                                      line[byte * 2], line[byte * 2 + 1]);
            } else {
                packPartialTransition(current[byte], current[byte],
                                      line[byte * 2], line[byte * 2 + 1]);
            }
        }
        writeBytes(line, sizeof(line));
    }

    turnOnDisplay();
}

void Ssd1683DisplayDriver::turnOnDisplay() {
    sendCommand(0x04);
    waitBusy("power_on");
    sendCommand(0x12);
    sendData(0x00);
    waitBusy("refresh", 15000);
    sendCommand(0x02);
    sendData(0x00);
    waitBusy("power_off");
    powerOff();
}

}  // namespace transitink::hardware
