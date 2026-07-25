#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace transitink {

enum class UiLocale : uint8_t {
    ZhHk,
    EnGb,
};

enum class UiTextId : uint8_t {
    InvalidConfig,
    NoArrivals,
    ClockUnsynced,
    JourneyUnavailable,
    JourneyUpdateFailed,
    DataUnavailable,
    DataExpired,
    MinuteSuffix,
    ArrivingSoon,
    EtaSuspended,
    TrafficCongested,
    TrafficSlow,
    TrafficClear,
    TunnelClosed,
    BatteryUnavailable,
    BatteryLabel,
    BatteryFull,
    BatteryCharging,
    BatteryExternalPower,
    WeekdaySunday,
    WeekdayMonday,
    WeekdayTuesday,
    WeekdayWednesday,
    WeekdayThursday,
    WeekdayFriday,
    WeekdaySaturday,
    NoWidgets,
    OpenSettings,
    Booting,
    SettingsPrefix,
    NetworkLabel,
    VersionLabel,
    SaveAndRestart,
    ConnectionStatus,
    WifiDisconnected,
    Updating,
    PasswordLabel,
    ConnectPhoneToWifi,
    OpenAddress,
    LocalSettings,
    ResetComplete,
    ReleaseVolumeRestart,
    WeatherUnavailable,
    WeatherSunny,
    WeatherSunnyPeriods,
    WeatherShowers,
    WeatherCloudy,
    WeatherOvercast,
    WeatherLightRain,
    WeatherRain,
    WeatherHeavyRain,
    WeatherThunderstorms,
    WeatherFog,
    WeatherSnow,
    WeatherSnowShowers,
    Weather,
    Count,
};

const char* uiLocaleId(UiLocale locale);
bool parseUiLocaleId(const std::string& value, UiLocale& locale);
bool isUiLocaleSupported(UiLocale locale);
void setUiLocale(UiLocale locale);
UiLocale currentUiLocale();
const char* uiText(UiTextId id);
const char* uiText(UiLocale locale, UiTextId id);
const char* weekdayText(int weekday);
std::string englishDisplayLabel(std::string value);
std::string localizedText(const std::string& traditionalChinese,
                          const std::string& english);
std::string localizedDisplayLabel(const std::string& traditionalChinese,
                                  const std::string& english);

}  // namespace transitink
