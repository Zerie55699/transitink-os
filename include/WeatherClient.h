#pragma once

#include <Arduino.h>
#include <ctime>

struct WeatherSnapshot {
    bool valid = false;
    String locationTc;
    int temperatureC = 0;
    String conditionTc;
    time_t updatedAt = 0;
    String error;
};

class WeatherClient {
public:
    bool fetchCurrentWeather(const String& locationTc, WeatherSnapshot& snapshot, String& error);

private:
    bool httpGet(const String& url, bool useOpenMeteoTrust,
                 String& body, String& error);
};

String weatherDisplayText(const WeatherSnapshot& snapshot);
String weatherConditionText(int icon);
String openMeteoWeatherConditionText(int weatherCode);
