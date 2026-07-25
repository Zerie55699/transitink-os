#include "WeatherClient.h"
#include "TransitTlsTrust.h"
#include "core/UiText.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <cmath>

namespace {

const char* kHkoCurrentWeatherUrl =
    "https://data.weather.gov.hk/weatherAPI/opendata/weather.php"
    "?dataType=rhrread&lang=";
const char* kOpenMeteoCurrentWeatherUrl =
    "https://api.open-meteo.com/v1/forecast"
    "?current=temperature_2m,weather_code&forecast_days=1"
    "&models=ukmo_seamless";
const char* kDefaultWeatherLocationTc = "香港天文台";
const char* kDefaultWeatherLocationEn = "Hong Kong Observatory";

struct WeatherLocation {
    const char* traditionalChinese;
    const char* english;
};

struct UkWeatherLocation {
    const char* id;
    const char* traditionalChinese;
    const char* english;
    const char* latitude;
    const char* longitude;
};

constexpr WeatherLocation kWeatherLocations[] = {
    {"香港天文台", "Hong Kong Observatory"},
    {"京士柏", "King's Park"},
    {"黃竹坑", "Wong Chuk Hang"},
    {"打鼓嶺", "Ta Kwu Ling"},
    {"流浮山", "Lau Fau Shan"},
    {"大埔", "Tai Po"},
    {"沙田", "Sha Tin"},
    {"屯門", "Tuen Mun"},
    {"將軍澳", "Tseung Kwan O"},
    {"西貢", "Sai Kung"},
    {"長洲", "Cheung Chau"},
    {"赤鱲角", "Chek Lap Kok"},
    {"青衣", "Tsing Yi"},
    {"荃灣可觀", "Tsuen Wan Ho Koon"},
    {"荃灣城門谷", "Tsuen Wan Shing Mun Valley"},
    {"香港公園", "Hong Kong Park"},
    {"筲箕灣", "Shau Kei Wan"},
    {"九龍城", "Kowloon City"},
    {"跑馬地", "Happy Valley"},
    {"黃大仙", "Wong Tai Sin"},
    {"赤柱", "Stanley"},
    {"觀塘", "Kwun Tong"},
    {"深水埗", "Sham Shui Po"},
    {"啟德跑道公園", "Kai Tak Runway Park"},
    {"元朗公園", "Yuen Long Park"},
    {"大美督", "Tai Mei Tuk"},
};

constexpr UkWeatherLocation kUkWeatherLocations[] = {
    {"uk:london", "倫敦", "London", "51.5074", "-0.1278"},
    {"uk:birmingham", "伯明翰", "Birmingham", "52.4862", "-1.8904"},
    {"uk:manchester", "曼徹斯特", "Manchester", "53.4808", "-2.2426"},
    {"uk:liverpool", "利物浦", "Liverpool", "53.4084", "-2.9916"},
    {"uk:leeds", "列斯", "Leeds", "53.8008", "-1.5491"},
    {"uk:bristol", "布里斯托", "Bristol", "51.4545", "-2.5879"},
    {"uk:glasgow", "格拉斯哥", "Glasgow", "55.8642", "-4.2518"},
    {"uk:edinburgh", "愛丁堡", "Edinburgh", "55.9533", "-3.1883"},
    {"uk:cardiff", "卡迪夫", "Cardiff", "51.4816", "-3.1791"},
    {"uk:belfast", "貝爾法斯特", "Belfast", "54.5973", "-5.9301"},
};

String normalizedLocation(const String& value) {
    String out = value;
    out.trim();
    return out.isEmpty() ? String(kDefaultWeatherLocationTc) : out;
}

const WeatherLocation* findWeatherLocation(const String& traditionalChinese) {
    for (const auto& location : kWeatherLocations) {
        if (traditionalChinese == location.traditionalChinese) {
            return &location;
        }
    }
    return nullptr;
}

const UkWeatherLocation* findUkWeatherLocation(const String& id) {
    for (const auto& location : kUkWeatherLocations) {
        if (id == location.id) {
            return &location;
        }
    }
    return nullptr;
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

bool WeatherClient::httpGet(const String& url, bool useOpenMeteoTrust,
                            String& body, String& error) {
    WiFiClientSecure tls;
    if (useOpenMeteoTrust) {
        transitink::configureOpenMeteoVerifiedTls(tls);
    } else {
        transitink::configureVerifiedTls(tls);
    }
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
    const String normalizedTc = normalizedLocation(locationTc);
    if (normalizedTc.startsWith("uk:")) {
        const UkWeatherLocation* ukLocation =
            findUkWeatherLocation(normalizedTc);
        if (ukLocation == nullptr) {
            error = "不支援的英國天氣位置";
            snapshot.valid = false;
            snapshot.error = error;
            return false;
        }

        const String url = String(kOpenMeteoCurrentWeatherUrl) +
                           "&latitude=" + ukLocation->latitude +
                           "&longitude=" + ukLocation->longitude;
        String body;
        if (!httpGet(url, true, body, error)) {
            snapshot.valid = false;
            snapshot.error = error;
            return false;
        }

        StaticJsonDocument<2048> doc;
        const DeserializationError jsonError = deserializeJson(doc, body);
        if (jsonError) {
            error = String("天氣 JSON 錯誤: ") + jsonError.c_str();
            snapshot.valid = false;
            snapshot.error = error;
            return false;
        }
        const JsonObjectConst current =
            doc["current"].as<JsonObjectConst>();
        if (current.isNull() ||
            current["temperature_2m"].isNull() ||
            current["weather_code"].isNull()) {
            error = "英國天氣資料格式錯誤";
            snapshot.valid = false;
            snapshot.error = error;
            return false;
        }

        const bool useEnglish =
            transitink::currentUiLocale() == transitink::UiLocale::EnGb;
        snapshot.valid = true;
        snapshot.locationTc =
            useEnglish ? ukLocation->english
                       : ukLocation->traditionalChinese;
        snapshot.temperatureC =
            static_cast<int>(std::lround(
                current["temperature_2m"].as<float>()));
        snapshot.conditionTc = openMeteoWeatherConditionText(
            current["weather_code"].as<int>());
        snapshot.updatedAt = time(nullptr);
        snapshot.error = "";
        error = "";
        return true;
    }

    const WeatherLocation* location = findWeatherLocation(normalizedTc);
    const bool useEnglish =
        transitink::currentUiLocale() == transitink::UiLocale::EnGb &&
        location != nullptr && location->english[0] != '\0';
    const String url =
        String(kHkoCurrentWeatherUrl) + (useEnglish ? "en" : "tc");
    String body;
    if (!httpGet(url, false, body, error)) {
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
    const String wanted =
        useEnglish ? String(location->english) : normalizedTc;
    const String defaultLocation =
        useEnglish ? String(kDefaultWeatherLocationEn)
                   : String(kDefaultWeatherLocationTc);
    String place;
    int temperatureC = 0;
    bool found = readTemperatureFor(temperatures, wanted, place, temperatureC);
    if (!found && wanted != defaultLocation) {
        found = readTemperatureFor(temperatures, defaultLocation, place,
                                   temperatureC);
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
            return transitink::uiText(transitink::UiTextId::WeatherSunny);
        case 51:
        case 52:
            return transitink::uiText(
                transitink::UiTextId::WeatherSunnyPeriods);
        case 53:
        case 54:
            return transitink::uiText(transitink::UiTextId::WeatherShowers);
        case 60:
        case 72:
        case 73:
            return transitink::uiText(transitink::UiTextId::WeatherCloudy);
        case 61:
            return transitink::uiText(transitink::UiTextId::WeatherOvercast);
        case 62:
            return transitink::uiText(transitink::UiTextId::WeatherLightRain);
        case 63:
            return transitink::uiText(transitink::UiTextId::WeatherRain);
        case 64:
            return transitink::uiText(transitink::UiTextId::WeatherHeavyRain);
        case 65:
            return transitink::uiText(
                transitink::UiTextId::WeatherThunderstorms);
        default:
            return transitink::uiText(transitink::UiTextId::Weather);
    }
}

String openMeteoWeatherConditionText(int weatherCode) {
    switch (weatherCode) {
        case 0:
            return transitink::uiText(transitink::UiTextId::WeatherSunny);
        case 1:
        case 2:
            return transitink::uiText(
                transitink::UiTextId::WeatherSunnyPeriods);
        case 3:
            return transitink::uiText(
                transitink::UiTextId::WeatherOvercast);
        case 45:
        case 48:
            return transitink::uiText(transitink::UiTextId::WeatherFog);
        case 51:
        case 53:
            return transitink::uiText(
                transitink::UiTextId::WeatherLightRain);
        case 55:
        case 56:
        case 57:
        case 61:
        case 63:
        case 66:
        case 67:
            return transitink::uiText(transitink::UiTextId::WeatherRain);
        case 65:
        case 82:
            return transitink::uiText(
                transitink::UiTextId::WeatherHeavyRain);
        case 71:
        case 73:
        case 75:
        case 77:
            return transitink::uiText(transitink::UiTextId::WeatherSnow);
        case 80:
        case 81:
            return transitink::uiText(
                transitink::UiTextId::WeatherShowers);
        case 85:
        case 86:
            return transitink::uiText(
                transitink::UiTextId::WeatherSnowShowers);
        case 95:
        case 96:
        case 99:
            return transitink::uiText(
                transitink::UiTextId::WeatherThunderstorms);
        default:
            return transitink::uiText(transitink::UiTextId::Weather);
    }
}

String weatherDisplayText(const WeatherSnapshot& snapshot) {
    if (!snapshot.valid) {
        return transitink::uiText(transitink::UiTextId::WeatherUnavailable);
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
