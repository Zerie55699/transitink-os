#pragma once

#include <Arduino.h>

#include "WeatherClient.h"
#include "core/WidgetCore.h"

class EInkDisplay {
public:
    void begin(bool showBootScreen = true);
    void showBoot(const String& message);
    void showConfigMode(const String& ssid, const String& url, const String& qrUrl = "");
    void showWifiStatus(const String& message);
    void showDashboard(const transitink::WidgetPageSnapshotSet& snapshots,
                       const WeatherSnapshot& weather,
                       uint8_t pageIndex = 0,
                       uint8_t pageCount = 1);
    void refreshWidgetLane(uint8_t slot,
                           const transitink::WidgetPageSnapshotSet& snapshots,
                           const WeatherSnapshot& weather);
    void refreshClock(const transitink::WidgetPageSnapshotSet& snapshots,
                      const WeatherSnapshot& weather);
    void refreshWeatherFooter(const transitink::WidgetPageSnapshotSet& snapshots,
                              const WeatherSnapshot& weather);
    void showSleep(const transitink::WidgetPageSnapshotSet& snapshots,
                   const WeatherSnapshot& weather,
                   uint8_t pageIndex = 0,
                   uint8_t pageCount = 1);
    void refreshSleepStatusAndWeather(
        const transitink::WidgetPageSnapshotSet& snapshots,
        const WeatherSnapshot& weather,
        uint8_t pageIndex = 0,
        uint8_t pageCount = 1);
    void prepareForSleep();

private:
    void fullRefresh();
    void partialRefresh(int x, int y, int w, int h);
    uint8_t widgetPageIndex_ = 0;
    uint8_t widgetPageCount_ = 1;
};
