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
    void showDashboard(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);
    void refreshWidgetLane(uint8_t slot, const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);
    void refreshClock(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);
    void refreshWeatherFooter(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);
    void showSleep(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);
    void prepareForSleep();

private:
    void fullRefresh();
    void partialRefresh(int x, int y, int w, int h);
};
