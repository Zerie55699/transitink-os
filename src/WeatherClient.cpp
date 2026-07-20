#include "WeatherClient.h"
#include "TransitTlsTrust.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace {

const char* kHkoCurrentWeatherUrl = "https://data.weather.gov.hk/weatherAPI/opendata/weather.php?dataType=rhrread&lang=tc";
const char* kDefaultWeatherLocation = "香港天文台";

String normalizedLocation(const String& value) {
    String out = value;
    out.trim();
    return out.isEmpty() ? String(kDefaultWeatherLocation) : out;
}

bool readTemperatureFor(JsonArrayConst rows, const String& wanted, String& place, int& temperatureC) {
    for (JsonObjectConst item : rows) {
        String itemPlace = item["place"] | "";
        if (itemPlace == wanted) {
            place = itemPlace;
            temperatureC = item["value"] | 0;
            return true;
        }
    }
    return false;
}

}  // namespace

bool WeatherClient::httpGet(const String& url, String& body, String& error) {
    WiFiClientSecure tls;
    transitink::configureVerifiedTls(tls);
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法建立天氣 HTTPS 連線";
        return false;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = String("天氣 HTTP 錯誤: ") + code;
        http.end();
        return false;
    }
    body = http.getString();
    http.end();
    return true;
}

bool WeatherClient::fetchCurrentWeather(const String& locationTc, WeatherSnapshot& snapshot, String& error) {
    String body;
    if (!httpGet(kHkoCurrentWeatherUrl, body, error)) {
        snapshot.valid = false;
        snapshot.error = error;
        return false;
    }

    DynamicJsonDocument doc(32768);
    DeserializationError jsonError = deserializeJson(doc, body);
    if (jsonError) {
        error = String("天氣 JSON 錯誤: ") + jsonError.c_str();
        snapshot.valid = false;
        snapshot.error = error;
        return false;
    }

    JsonArrayConst temperatures = doc["temperature"]["data"].as<JsonArrayConst>();
    String wanted = normalizedLocation(locationTc);
    String place;
    int temperatureC = 0;
    bool found = readTemperatureFor(temperatures, wanted, place, temperatureC);
    if (!found && wanted != kDefaultWeatherLocation) {
        found = readTemperatureFor(temperatures, kDefaultWeatherLocation, place, temperatureC);
    }
    if (!found) {
        for (JsonObjectConst item : temperatures) {
            place = item["place"] | "";
            temperatureC = item["value"] | 0;
            found = place.length() > 0;
            break;
        }
    }
    if (!found) {
        error = "找不到天氣溫度資料";
        snapshot.valid = false;
        snapshot.error = error;
        return false;
    }

    int icon = 0;
    JsonArrayConst icons = doc["icon"].as<JsonArrayConst>();
    for (JsonVariantConst value : icons) {
        icon = value | 0;
        break;
    }

    snapshot.valid = true;
    snapshot.locationTc = place;
    snapshot.temperatureC = temperatureC;
    snapshot.conditionTc = weatherConditionText(icon);
    snapshot.updatedAt = time(nullptr);
    snapshot.error = "";
    return true;
}

String weatherConditionText(int icon) {
    switch (icon) {
        case 50:
        case 70:
        case 71:
            return "晴";
        case 51:
        case 52:
            return "間中有陽光";
        case 53:
        case 54:
            return "有驟雨";
        case 60:
        case 72:
        case 73:
            return "多雲";
        case 61:
            return "密雲";
        case 62:
            return "微雨";
        case 63:
            return "雨";
        case 64:
            return "大雨";
        case 65:
            return "雷暴";
        default:
            return "天氣";
    }
}

String weatherDisplayText(const WeatherSnapshot& snapshot) {
    if (!snapshot.valid) {
        return "天氣暫無資料";
    }
    String text = snapshot.locationTc;
    text += " ";
    text += snapshot.temperatureC;
    text += "°C";
    if (snapshot.conditionTc.length() > 0) {
        text += " ";
        text += snapshot.conditionTc;
    }
    return text;
}
