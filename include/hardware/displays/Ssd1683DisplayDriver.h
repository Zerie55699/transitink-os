#pragma once

#include <cstddef>
#include <cstdint>

namespace transitink::hardware {

class Ssd1683DisplayDriver {
public:
    void begin();
    void show(const uint8_t* buffer);
    void showPartialRegion(const uint8_t* current,
                           const uint8_t* previous,
                           int x,
                           int y,
                           int width,
                           int height);

private:
    void powerOn();
    void powerOff();
    void reset();
    bool waitBusy(const char* label, uint32_t timeoutMs = 5000);
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);
    void writeBytes(const uint8_t* data, size_t length);
    void initPanel();
    void pack1bpp(uint8_t input, uint8_t& output0, uint8_t& output1);
    void packPartialTransition(uint8_t previousByte,
                               uint8_t currentByte,
                               uint8_t& output0,
                               uint8_t& output1);
    void displayFrame(const uint8_t* buffer);
    void displayPartialFrameRegion(const uint8_t* current,
                                   const uint8_t* previous,
                                   int x,
                                   int y,
                                   int width,
                                   int height);
    void turnOnDisplay();
};

}  // namespace transitink::hardware
