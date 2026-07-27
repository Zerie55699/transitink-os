#ifndef BUTTON_MAPPER

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <ctime>

#include "AppConfig.h"
#include "BatteryMonitor.h"
#include "ConfigPortal.h"
#include "ConfigStore.h"
#include "CitybusClient.h"
#include "EInkDisplay.h"
#include "FirmwareUpdateService.h"
#include "GmbClient.h"
#include "JourneyTimeClient.h"
#include "KmbClient.h"
#include "LightRailClient.h"
#include "MtrClient.h"
#include "ProductConfig.h"
#include "TflClient.h"
#include "WeatherClient.h"
#include "WidgetCatalogService.h"
#include "core/BusEtaCore.h"
#include "core/UiText.h"
#include "core/WidgetScheduler.h"
#include "hardware/BoardProfile.h"
#include "hardware/BoardSupport.h"
#include "providers/BusProvider.h"
#include "providers/GmbProvider.h"
#include "providers/JourneyTimeProvider.h"
#include "providers/LightRailProvider.h"
#include "providers/MtrProvider.h"
#include "providers/TflRailProvider.h"
#include "providers/WidgetProviderRouter.h"

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

ConfigStore configStore;
BatteryMonitor chargeMonitor;
DeviceConfig deviceConfig;
KmbClient kmbClient;
CitybusClient citybusClient;
TflClient tflClient;
GmbClient gmbClient;
MtrClient mtrClient;
LightRailClient lightRailClient;
JourneyTimeClient journeyTimeClient;
BusProvider busProvider(kmbClient, citybusClient, tflClient);
GmbProvider gmbProvider(gmbClient);
MtrProvider mtrProvider(mtrClient);
LightRailProvider lightRailProvider(lightRailClient);
TflRailProvider tflRailProvider(tflClient);
JourneyTimeProvider journeyTimeProvider(journeyTimeClient);
WidgetProviderRouter widgetProviderRouter(
    busProvider, gmbProvider, mtrProvider, lightRailProvider, tflRailProvider,
    journeyTimeProvider);
transitink::WidgetScheduler widgetScheduler(widgetProviderRouter);
WidgetCatalogService widgetCatalogService(kmbClient, citybusClient, gmbClient,
                                          tflClient);
WeatherClient weatherClient;
WeatherSnapshot weatherSnapshot;
EInkDisplay einkDisplay;
ConfigPortal configPortal(deviceConfig, configStore, widgetCatalogService);

unsigned long nextClockRefreshMs = 0;
unsigned long nextWeatherRefreshMs = 0;
unsigned long wakeStartedAtMs = 0;
unsigned long lastChargeStatusPollMs = 0;
bus_eta::BatterySnapshot chargeSnapshot;
bool factoryResetPendingRestart = false;
bool factoryResetApplied = false;
bool configAccessMode = false;
bool dashboardVisible = false;
bool sleepMaintenanceWake = false;
bool scheduledWakeSession = false;
bool sleepScreenPrepared = false;
bool chargeStatusLogged = false;
RTC_DATA_ATTR uint8_t activeWidgetPage = 0;
enum class HomeWakeRefreshPhase : uint8_t {
    Idle,
    ConnectingWifi,
    WaitingForTime,
    Widgets,
    Weather,
};
HomeWakeRefreshPhase homeWakeRefreshPhase = HomeWakeRefreshPhase::Idle;
unsigned long homeWakePhaseStartedMs = 0;
uint8_t homeWakeWidgetAttempts = 0;
constexpr uint32_t kHomeWakeWifiTimeoutMs = 15000;
constexpr uint32_t kHomeWakeTimeTimeoutMs = 2000;
constexpr uint32_t kSleepResumeMarker = 0x54524E53U;
RTC_NOINIT_ATTR uint32_t sleepResumeMarker;
RTC_NOINIT_ATTR uint32_t sleepResumeMarkerInverse;

void serviceFactoryResetButtons();
void serviceConfigButton();
void serviceWidgetPageButton();
void setupFactoryResetButtons();
void showConfigAccessScreen();
void returnToDashboard();
void refreshWeatherNow();
void refreshAllWidgetsNow();
void serviceOneWidgetIfDue();
void startHomeWakeRefresh();
void serviceHomeWakeRefresh();
void finishHomeWakeRefresh();
bool homeWakeRefreshActive();
transitink::WidgetPageSnapshotSet currentDisplaySnapshots();
transitink::WidgetPageSnapshotSet homeWakeLoadingSnapshots();
transitink::WidgetPageSnapshotSet pageSwitchSnapshots();
uint8_t dashboardPageCount();
bool hasValidTime();
void applyConfiguredTimeZone();
void configureNetworkTime();
void syncTimeAndWeatherBeforeDashboard(bool homeWake);
bus_eta::SleepSettings sleepSettingsFromConfig();
bool scheduledWakeWindowActiveNow();
unsigned long long scheduledWakeDelayUs();
void stopNetworkForSleep();
void configureLightSleepWakeup();
void armSleepResumeMarker();
void clearSleepResumeMarker();
void clearPersistentSleepResumeMarker();
bool consumeSleepResumeMarker();
void waitForHomeRelease();
void returnFromLightSleep();
void performLightSleepMaintenance();
void enterSleepMode(const char* reason);
void serviceChargeStatus(bool force = false);

String configApSsid() {
    uint64_t mac = ESP.getEfuseMac();
    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%06X", static_cast<unsigned int>(mac & 0xFFFFFF));
    return String(CONFIG_AP_PREFIX) + "-" + suffix;
}

String configAccessPointMessage(const String& configUrl) {
    return String(transitink::uiText(transitink::UiTextId::PasswordLabel)) +
           configPortal.apPassword() + "\n" +
           transitink::uiText(
               transitink::UiTextId::ConnectPhoneToWifi) +
           "\n" +
           transitink::uiText(transitink::UiTextId::OpenAddress) + configUrl;
}

bool connectWifi(const DeviceConfig& config) {
    if (config.wifiSsid.isEmpty()) {
        return false;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
    Serial.println("Connecting to configured Wi-Fi");
    unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) {
        serviceFactoryResetButtons();
        if (factoryResetPendingRestart) {
            return false;
        }
        delay(250);
    }
    Serial.print("Wi-Fi status: ");
    Serial.println(static_cast<int>(WiFi.status()));
    return WiFi.status() == WL_CONNECTED;
}

bool waitForTimeSync(uint32_t timeoutMs) {
    unsigned long started = millis();
    while (millis() - started < timeoutMs) {
        serviceFactoryResetButtons();
        if (factoryResetPendingRestart) {
            return false;
        }
        if (hasValidTime()) {
            return true;
        }
        delay(250);
    }
    return false;
}

bool hasValidTime() {
    return time(nullptr) >= 1700000000;
}

void applyConfiguredTimeZone() {
    setenv("TZ", transitink::devicePosixTimeZone(deviceConfig.timeZone), 1);
    tzset();
}

void configureNetworkTime() {
    configTzTime(transitink::devicePosixTimeZone(deviceConfig.timeZone),
                 "pool.ntp.org", "time.cloudflare.com", "time.nist.gov");
}

void syncTimeAndWeatherBeforeDashboard(bool homeWake) {
    configureNetworkTime();
    if (homeWake) {
        if (!hasValidTime()) {
            waitForTimeSync(2000);
        }
        refreshWeatherNow();
        return;
    }
    waitForTimeSync(hasValidTime() ? 1000 : 15000);
    refreshWeatherNow();
}

uint32_t secondsUntilNextMinute(time_t now) {
    if (now < 1700000000) {
        return 60;
    }
    uint32_t seconds = 60 - (now % 60);
    return seconds == 0 ? 60 : seconds;
}

void scheduleNextClockRefresh() {
    nextClockRefreshMs = millis() + secondsUntilNextMinute(time(nullptr)) * 1000UL;
    Serial.print("Next clock refresh ms: ");
    Serial.println(nextClockRefreshMs);
}

void refreshClockNow() {
    Serial.println("Clock refresh start");
    einkDisplay.refreshClock(currentDisplaySnapshots(), weatherSnapshot);
    scheduleNextClockRefresh();
}

void scheduleNextWeatherRefresh(uint32_t seconds = WEATHER_REFRESH_SECONDS) {
    nextWeatherRefreshMs = millis() + seconds * 1000UL;
    Serial.print("Next weather refresh ms: ");
    Serial.println(nextWeatherRefreshMs);
}

void refreshWeatherNow() {
    Serial.println("Weather refresh start");
    if (WiFi.status() != WL_CONNECTED) {
        weatherSnapshot.valid = false;
        weatherSnapshot.error =
            transitink::uiText(transitink::UiTextId::WifiDisconnected);
        scheduleNextWeatherRefresh(60);
        if (dashboardVisible) {
            einkDisplay.refreshWeatherFooter(currentDisplaySnapshots(), weatherSnapshot);
        }
        return;
    }

    String error;
    bool ok = weatherClient.fetchCurrentWeather(deviceConfig.weatherLocationTc, weatherSnapshot, error);
    Serial.print("Weather refresh ok: ");
    Serial.println(ok ? "yes" : "no");
    if (!ok) {
        Serial.print("Weather error: ");
        Serial.println(error);
    }
    scheduleNextWeatherRefresh();
    if (dashboardVisible) {
        einkDisplay.refreshWeatherFooter(currentDisplaySnapshots(), weatherSnapshot);
    }
}

transitink::WidgetPageSnapshotSet currentDisplaySnapshots() {
    const int64_t nowEpoch = hasValidTime() ? static_cast<int64_t>(time(nullptr)) : 0;
    return transitink::snapshotsForWidgetPage(
        widgetScheduler.displaySnapshots(nowEpoch), activeWidgetPage);
}

transitink::WidgetPageSnapshotSet homeWakeLoadingSnapshots() {
    transitink::WidgetPageSnapshotSet snapshots = currentDisplaySnapshots();
    for (auto& snapshot : snapshots) {
        if (snapshot.type == transitink::WidgetType::Disabled) {
            continue;
        }
        snapshot.values = {};
        snapshot.valueCount = 0;
        snapshot.state = transitink::WidgetState::Empty;
        snapshot.providerMessage =
            transitink::uiText(transitink::UiTextId::Updating);
        snapshot.fetchedAtEpoch = 0;
        snapshot.dataAtEpoch = 0;
        snapshot.freshness = transitink::Freshness::Fresh;
        snapshot.consecutiveFailures = 0;
    }
    return snapshots;
}

transitink::WidgetPageSnapshotSet pageSwitchSnapshots() {
    const int64_t nowEpoch =
        hasValidTime() ? static_cast<int64_t>(time(nullptr)) : 0;
    return widgetScheduler.pageSwitchSnapshots(
        nowEpoch, WIDGET_PAGE_CACHE_TTL_SECONDS);
}

uint8_t dashboardPageCount() {
    return transitink::enabledWidgetPageCount(deviceConfig.widgets) > 1
               ? static_cast<uint8_t>(transitink::kWidgetPageCount)
               : 1;
}

void refreshAllWidgetsNow() {
    Serial.println("Widget refresh active page start");
    const uint32_t nowMs = millis();
    const int64_t nowEpoch = hasValidTime() ? static_cast<int64_t>(time(nullptr)) : 0;
    widgetScheduler.forceActivePageDue(nowMs);
    for (std::size_t attempts = 0;
         attempts < transitink::kWidgetsPerPage && widgetScheduler.hasPendingDue(nowMs);
         ++attempts) {
        widgetScheduler.serviceNextDue(nowMs, nowEpoch);
    }
    einkDisplay.showDashboard(currentDisplaySnapshots(), weatherSnapshot,
                              activeWidgetPage, dashboardPageCount());
    dashboardVisible = true;
    scheduleNextClockRefresh();
}

void serviceOneWidgetIfDue() {
    if (!dashboardVisible || WiFi.status() != WL_CONNECTED) {
        return;
    }
    const uint32_t nowMs = millis();
    const int64_t nowEpoch = hasValidTime() ? static_cast<int64_t>(time(nullptr)) : 0;
    const transitink::WidgetTickResult tick = widgetScheduler.serviceNextDue(nowMs, nowEpoch);
    if (!tick.ran) {
        return;
    }
    const std::size_t pageStart = transitink::widgetPageStart(activeWidgetPage);
    if (tick.slot < pageStart ||
        tick.slot >= pageStart + transitink::kWidgetsPerPage) {
        return;
    }
    const uint8_t lane = static_cast<uint8_t>(tick.slot - pageStart);
    einkDisplay.refreshWidgetLane(lane, currentDisplaySnapshots(), weatherSnapshot);
}

bool homeWakeRefreshActive() {
    return homeWakeRefreshPhase != HomeWakeRefreshPhase::Idle;
}

void finishHomeWakeRefresh() {
    if (!homeWakeRefreshActive()) {
        return;
    }
    homeWakeRefreshPhase = HomeWakeRefreshPhase::Idle;
    clearPersistentSleepResumeMarker();
    Serial.println("Home wake background refresh complete");
}

void startHomeWakeRefresh() {
    Serial.println("Home wake: restore dashboard before network refresh");
    wakeStartedAtMs = millis();
    homeWakeWidgetAttempts = 0;
    widgetScheduler.forceActivePageDue(wakeStartedAtMs);
    einkDisplay.showDashboard(homeWakeLoadingSnapshots(), weatherSnapshot,
                              activeWidgetPage, dashboardPageCount());
    dashboardVisible = true;
    scheduleNextClockRefresh();

    if (deviceConfig.wifiSsid.isEmpty()) {
        weatherSnapshot.valid = false;
        weatherSnapshot.error =
            transitink::uiText(transitink::UiTextId::WifiDisconnected);
        scheduleNextWeatherRefresh(60);
        homeWakeRefreshPhase = HomeWakeRefreshPhase::Weather;
        finishHomeWakeRefresh();
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(deviceConfig.wifiSsid.c_str(), deviceConfig.wifiPassword.c_str());
    homeWakePhaseStartedMs = millis();
    homeWakeRefreshPhase = HomeWakeRefreshPhase::ConnectingWifi;
    Serial.println("Home wake: Wi-Fi connection started in background");
}

void serviceHomeWakeRefresh() {
    const uint32_t nowMs = millis();
    switch (homeWakeRefreshPhase) {
        case HomeWakeRefreshPhase::Idle:
            return;
        case HomeWakeRefreshPhase::ConnectingWifi:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("Home wake: Wi-Fi connected");
                configureNetworkTime();
                homeWakePhaseStartedMs = nowMs;
                homeWakeRefreshPhase = HomeWakeRefreshPhase::WaitingForTime;
                return;
            }
            if (nowMs - homeWakePhaseStartedMs >= kHomeWakeWifiTimeoutMs) {
                Serial.println("Home wake: Wi-Fi connection timed out");
                weatherSnapshot.valid = false;
                weatherSnapshot.error =
                    transitink::uiText(
                        transitink::UiTextId::WifiDisconnected);
                scheduleNextWeatherRefresh(60);
                finishHomeWakeRefresh();
            }
            return;
        case HomeWakeRefreshPhase::WaitingForTime:
            if (hasValidTime() || nowMs - homeWakePhaseStartedMs >= kHomeWakeTimeTimeoutMs) {
                refreshClockNow();
                homeWakeRefreshPhase = HomeWakeRefreshPhase::Widgets;
            }
            return;
        case HomeWakeRefreshPhase::Widgets:
            if (WiFi.status() != WL_CONNECTED) {
                WiFi.reconnect();
                homeWakePhaseStartedMs = nowMs;
                homeWakeRefreshPhase = HomeWakeRefreshPhase::ConnectingWifi;
                return;
            }
            if (homeWakeWidgetAttempts < static_cast<uint8_t>(transitink::kWidgetsPerPage) &&
                widgetScheduler.hasPendingDue(nowMs)) {
                ++homeWakeWidgetAttempts;
                serviceOneWidgetIfDue();
                return;
            }
            homeWakeRefreshPhase = HomeWakeRefreshPhase::Weather;
            return;
        case HomeWakeRefreshPhase::Weather:
            refreshWeatherNow();
            finishHomeWakeRefresh();
            return;
    }
}

bus_eta::SleepSettings sleepSettingsFromConfig() {
    bus_eta::SleepSettings settings;
    settings.enabled = deviceConfig.sleepEnabled;
    settings.wakeDurationMinutes = deviceConfig.wakeDurationMinutes;
    settings.maintenanceHours = deviceConfig.sleepMaintenanceHours;
    settings.scheduledWakeEnabled = deviceConfig.scheduledWakeEnabled;
    settings.scheduledWakeStartMinutes = deviceConfig.scheduledWakeStartMinutes;
    settings.scheduledWakeEndMinutes = deviceConfig.scheduledWakeEndMinutes;
    return settings;
}

bool localSecondOfDay(unsigned int& secondOfDay) {
    if (!hasValidTime()) {
        return false;
    }
    const time_t now = time(nullptr);
    struct tm localTime;
    if (localtime_r(&now, &localTime) == nullptr) {
        return false;
    }
    secondOfDay = static_cast<unsigned int>(localTime.tm_hour * 60 * 60 +
                                            localTime.tm_min * 60 +
                                            localTime.tm_sec);
    return true;
}

bool scheduledWakeWindowActiveNow() {
    unsigned int secondOfDay = 0;
    return localSecondOfDay(secondOfDay) &&
           bus_eta::isScheduledWakeWindow(sleepSettingsFromConfig(), secondOfDay / 60);
}

unsigned long long scheduledWakeDelayUs() {
    unsigned int secondOfDay = 0;
    if (!localSecondOfDay(secondOfDay)) {
        return 0;
    }
    const unsigned int delaySeconds =
        bus_eta::secondsUntilScheduledWakeStart(sleepSettingsFromConfig(), secondOfDay);
    return static_cast<unsigned long long>(delaySeconds) * 1000000ULL;
}

void stopNetworkForSleep() {
    configPortal.stop();
    WiFi.disconnect(true, true);
    esp_wifi_stop();
    WiFi.mode(WIFI_OFF);
}

void configureLightSleepWakeup() {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    transitink::hardware::configureHomeWakeup();
    // Charge state is polled while awake. It must not be able to impersonate
    // a Home press and expose the dashboard while the device is sleeping.

    const bus_eta::SleepSettings settings = sleepSettingsFromConfig();
    const unsigned long long timerUs = settings.scheduledWakeEnabled
                                           ? scheduledWakeDelayUs()
                                           : bus_eta::sleepMaintenanceIntervalUs(settings);
    if (timerUs > 0) {
        esp_sleep_enable_timer_wakeup(timerUs);
    }
}

void armSleepResumeMarker() {
    sleepResumeMarker = kSleepResumeMarker;
    sleepResumeMarkerInverse = ~kSleepResumeMarker;
    if (!configStore.setSleepResumePending(true)) {
        Serial.println("Unable to persist sleep resume marker");
    }
}

void clearSleepResumeMarker() {
    sleepResumeMarker = 0;
    sleepResumeMarkerInverse = 0;
}

void clearPersistentSleepResumeMarker() {
    if (!configStore.setSleepResumePending(false)) {
        Serial.println("Unable to clear persistent sleep resume marker");
    }
}

bool consumeSleepResumeMarker() {
    const bool pending = sleepResumeMarker == kSleepResumeMarker &&
                         sleepResumeMarkerInverse == ~kSleepResumeMarker;
    clearSleepResumeMarker();
    return pending;
}

void waitForHomeRelease() {
    unsigned long started = millis();
    while (transitink::hardware::homeButtonPressed() &&
           millis() - started < 5000 && !factoryResetPendingRestart) {
        serviceFactoryResetButtons();
        delay(20);
    }
}

void returnFromLightSleep() {
    Serial.println("Home wake from light sleep");
    setupFactoryResetButtons();
    serviceChargeStatus(true);
    einkDisplay.begin(false);
    sleepScreenPrepared = false;
    startHomeWakeRefresh();
}

void performLightSleepMaintenance() {
    Serial.println("Light sleep maintenance wake");
    setupFactoryResetButtons();
    dashboardVisible = false;
    configAccessMode = false;
    bool wifiOk = connectWifi(deviceConfig);
    if (wifiOk) {
        syncTimeAndWeatherBeforeDashboard(false);
    } else {
        weatherSnapshot.valid = false;
        weatherSnapshot.error =
            transitink::uiText(transitink::UiTextId::WifiDisconnected);
        scheduleNextWeatherRefresh(60);
    }
    stopNetworkForSleep();
    einkDisplay.refreshSleepStatusAndWeather(
        currentDisplaySnapshots(), weatherSnapshot, activeWidgetPage,
        dashboardPageCount());
    einkDisplay.prepareForSleep();
    sleepScreenPrepared = true;
}

void enterSleepMode(const char* reason) {
    if (!deviceConfig.sleepEnabled || factoryResetPendingRestart) {
        return;
    }
    Serial.print("Entering sleep mode: ");
    Serial.println(reason);
    scheduledWakeSession = false;
    if (!sleepScreenPrepared) {
        transitink::hardware::clearPendingHomePress();
        dashboardVisible = false;
        configAccessMode = false;
        einkDisplay.showSleep(currentDisplaySnapshots(), weatherSnapshot,
                              activeWidgetPage, dashboardPageCount());
        stopNetworkForSleep();
        einkDisplay.prepareForSleep();
    }
    sleepScreenPrepared = false;
    setupFactoryResetButtons();

    while (deviceConfig.sleepEnabled && !factoryResetPendingRestart) {
        if (transitink::hardware::takeHomePress() ||
            transitink::hardware::homeButtonPressed()) {
            Serial.println("Home pressed while preparing sleep");
            waitForHomeRelease();
            if (!factoryResetPendingRestart) {
                returnFromLightSleep();
            }
            return;
        }
        configureLightSleepWakeup();
        armSleepResumeMarker();
        esp_err_t sleepResult = esp_light_sleep_start();
        const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
        clearSleepResumeMarker();
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        transitink::hardware::disableHomeWakeup();
        setupFactoryResetButtons();

        if (sleepResult != ESP_OK) {
            Serial.print("Light sleep failed: ");
            Serial.println(static_cast<int>(sleepResult));
            delay(200);
            continue;
        }

        Serial.print("Light sleep wake cause: ");
        Serial.println(static_cast<int>(wakeCause));
        // configureLightSleepWakeup() clears every source before enabling only
        // the Home GPIO and one optional low-power timer.
        if (wakeCause == ESP_SLEEP_WAKEUP_GPIO) {
            waitForHomeRelease();
            if (!factoryResetPendingRestart) {
                returnFromLightSleep();
            }
            return;
        }
        if (wakeCause == ESP_SLEEP_WAKEUP_TIMER) {
            if (deviceConfig.scheduledWakeEnabled) {
                if (scheduledWakeWindowActiveNow()) {
                    Serial.println("Scheduled wake window started");
                    scheduledWakeSession = true;
                    returnFromLightSleep();
                    return;
                }
                Serial.println("Scheduled wake occurred outside configured window");
                continue;
            }
            performLightSleepMaintenance();
            continue;
        }
        delay(50);
    }
}

void setupFactoryResetButtons() {
    transitink::hardware::configureButtons();
    if (!transitink::hardware::startButtonMonitoring()) {
        Serial.println("Unable to start button monitor");
    }
}

void applyFactoryReset() {
    if (factoryResetApplied) {
        return;
    }
    factoryResetApplied = true;
    factoryResetPendingRestart = true;
    Serial.println("Factory reset requested by volume buttons");
    configStore.clear();
    WiFi.disconnect(true, true);
    if (LittleFS.begin(true)) {
        LittleFS.format();
    }
    einkDisplay.showWifiStatus(
        String(transitink::uiText(transitink::UiTextId::ResetComplete)) +
        "\n" +
        transitink::uiText(transitink::UiTextId::ReleaseVolumeRestart));
}

void serviceFactoryResetButtons() {
    const bool upPressed = transitink::hardware::factoryResetUpButtonPressed();
    const bool downPressed = transitink::hardware::factoryResetDownButtonPressed();
    if (factoryResetPendingRestart) {
        if (!upPressed && !downPressed) {
            delay(300);
            ESP.restart();
        }
        return;
    }
    if (transitink::hardware::takeFactoryResetHold()) {
        applyFactoryReset();
    }
}

void showConfigAccessScreen() {
    Serial.println("Config button clicked");
    finishHomeWakeRefresh();
    const bool useAccessPoint = WiFi.status() != WL_CONNECTED;
    configPortal.begin(useAccessPoint);
    configAccessMode = true;
    dashboardVisible = false;
    const String configUrl = configPortal.pageUrl();
    if (configPortal.isApMode()) {
        const String message = configAccessPointMessage(configUrl);
        einkDisplay.showConfigMode(configApSsid(), message);
        return;
    }
    String displayedConfigUrl = configUrl;
    const int accessPathStart = displayedConfigUrl.lastIndexOf('/') + 1;
    if (accessPathStart > 0 && accessPathStart < displayedConfigUrl.length()) {
        displayedConfigUrl =
            displayedConfigUrl.substring(0, accessPathStart) + "\n" +
            displayedConfigUrl.substring(accessPathStart);
    }
    const String message =
        String(transitink::uiText(transitink::UiTextId::LocalSettings)) +
        "\n" + displayedConfigUrl;
    einkDisplay.showConfigMode(deviceConfig.wifiSsid, message, configUrl);
}

void returnToDashboard() {
    Serial.println("Config button clicked: returning to dashboard");
    if (!hasUsableConfig(deviceConfig)) {
        showConfigAccessScreen();
        return;
    }
    configPortal.stop();
    configAccessMode = false;
    wakeStartedAtMs = millis();
    if (WiFi.status() != WL_CONNECTED && connectWifi(deviceConfig)) {
        syncTimeAndWeatherBeforeDashboard(true);
    }
    refreshAllWidgetsNow();
}

void serviceConfigButton() {
    if (!transitink::hardware::takeConfigClick() || factoryResetPendingRestart) {
        return;
    }
    if (configAccessMode) {
        returnToDashboard();
    } else {
        showConfigAccessScreen();
    }
    transitink::hardware::clearPendingConfigClick();
}

void serviceWidgetPageButton() {
    if (!transitink::hardware::takeWidgetPageClick() ||
        factoryResetPendingRestart) {
        return;
    }
    if (configAccessMode || !dashboardVisible) {
        transitink::hardware::clearPendingWidgetPageClick();
        return;
    }

    const std::size_t nextPage =
        transitink::nextEnabledWidgetPage(deviceConfig.widgets, activeWidgetPage);
    if (nextPage == activeWidgetPage ||
        !widgetScheduler.setActivePage(nextPage, millis())) {
        return;
    }

    activeWidgetPage = static_cast<uint8_t>(nextPage);
    homeWakeWidgetAttempts = 0;
    Serial.print("Widget page changed to: ");
    Serial.println(static_cast<unsigned int>(activeWidgetPage) + 1);
    einkDisplay.showDashboard(pageSwitchSnapshots(), weatherSnapshot,
                              activeWidgetPage, dashboardPageCount());
}

void serviceChargeStatus(bool force) {
    const unsigned long now = millis();
    if (!force && now - lastChargeStatusPollMs < 500) {
        return;
    }
    lastChargeStatusPollMs = now;

    const bus_eta::BatterySnapshot next = chargeMonitor.readChargeState();
    const bool changed = !chargeStatusLogged ||
                         next.powerPresent != chargeSnapshot.powerPresent ||
                         next.charging != chargeSnapshot.charging ||
                         next.full != chargeSnapshot.full;
    if (!changed) {
        chargeSnapshot.powerPresent = next.powerPresent;
        chargeSnapshot.charging = next.charging;
        chargeSnapshot.full = next.full;
        return;
    }

    chargeSnapshot = chargeMonitor.read();
    chargeStatusLogged = true;
    Serial.print("Charge state: ");
    Serial.print(chargeSnapshot.full
                     ? "full"
                     : (chargeSnapshot.charging ? "charging" : "battery"));
    Serial.print(", voltage_mv=");
    Serial.print(chargeSnapshot.voltageMv);
    Serial.print(", percent=");
    Serial.println(chargeSnapshot.percent);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    applyConfiguredTimeZone();
    const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    const esp_reset_reason_t resetReason = esp_reset_reason();
    setupFactoryResetButtons();
    const bool homePressedAtBoot = transitink::hardware::homeButtonPressed();
    const bool rtcResetWake = consumeSleepResumeMarker();
    const bool configStoreReady = configStore.begin();
    const bool persistentResetWake = configStoreReady && configStore.sleepResumePending();
    const bool resetWake = rtcResetWake || persistentResetWake;
    const bus_eta::SleepResumeAction sleepResumeAction =
        bus_eta::decideSleepResumeAction(
            resetWake,
            wakeCause == ESP_SLEEP_WAKEUP_TIMER,
            wakeCause == ESP_SLEEP_WAKEUP_GPIO,
            homePressedAtBoot,
            resetReason == ESP_RST_POWERON);
    sleepMaintenanceWake =
        sleepResumeAction == bus_eta::SleepResumeAction::RunMaintenance;
    const bool homeWake =
        sleepResumeAction == bus_eta::SleepResumeAction::ShowDashboard;
    const bool resumeSleep =
        sleepResumeAction == bus_eta::SleepResumeAction::ResumeSleep;
    Serial.print("Wake cause: ");
    Serial.println(static_cast<int>(wakeCause));
    Serial.print("Reset reason: ");
    Serial.println(static_cast<int>(resetReason));
    Serial.print("Reset wake marker: ");
    Serial.println(persistentResetWake ? "persistent" : (rtcResetWake ? "rtc" : "no"));
    chargeMonitor.begin();
    serviceChargeStatus(true);
    // Keep the retained e-paper dashboard visible until fresh data is ready.
    // Battery wake can reset the MCU, so a transient boot screen is never safe here.
    einkDisplay.begin(false);

    bool loaded = configStore.load(deviceConfig);
    if (loaded && transitink::isUiLocaleSupported(deviceConfig.uiLocale)) {
        transitink::setUiLocale(deviceConfig.uiLocale);
    }
    if (loaded &&
        transitink::isDeviceTimeZoneSupported(deviceConfig.timeZone)) {
        applyConfiguredTimeZone();
    }
    Serial.print("Config loaded: ");
    Serial.println(loaded ? "yes" : "no");
    if (!loaded || !hasUsableConfig(deviceConfig)) {
        Serial.println("Starting config portal: missing or invalid config");
        configPortal.begin(true);
        configAccessMode = true;
        const String configUrl = configPortal.pageUrl();
        einkDisplay.showConfigMode(
            configApSsid(),
            configAccessPointMessage(configUrl));
        return;
    }

    widgetScheduler.configure(deviceConfig.widgets, millis());
    if (!transitink::widgetPageHasEnabled(deviceConfig.widgets,
                                          activeWidgetPage)) {
        activeWidgetPage = static_cast<uint8_t>(
            transitink::firstEnabledWidgetPage(deviceConfig.widgets));
    }
    widgetScheduler.setActivePage(activeWidgetPage, millis());
    if (!transitink::FirmwareUpdateService::confirmRunningFirmware()) {
        Serial.println("OTA firmware validation failed");
    }

    if (deviceConfig.sleepEnabled && sleepMaintenanceWake) {
        if (deviceConfig.scheduledWakeEnabled) {
            sleepMaintenanceWake = false;
            if (scheduledWakeWindowActiveNow()) {
                Serial.println("Scheduled reset wake: showing dashboard");
                scheduledWakeSession = true;
                startHomeWakeRefresh();
                return;
            }
            Serial.println("Scheduled reset wake outside window: returning to sleep");
            sleepScreenPrepared = true;
            enterSleepMode("scheduled wake outside window");
            return;
        }
        performLightSleepMaintenance();
        sleepMaintenanceWake = false;
        enterSleepMode("maintenance complete");
        return;
    }

    if (deviceConfig.sleepEnabled && resumeSleep) {
        Serial.println("Unconfirmed sleep reset: returning to sleep");
        clearPersistentSleepResumeMarker();
        dashboardVisible = false;
        configAccessMode = false;
        sleepScreenPrepared = true;
        enterSleepMode("unconfirmed sleep reset");
        return;
    }

    if (homeWake) {
        Serial.println("Config portal deferred until button press");
        startHomeWakeRefresh();
        return;
    }

    bool wifiOk = connectWifi(deviceConfig);
    if (wifiOk) {
        syncTimeAndWeatherBeforeDashboard(false);
    } else {
        weatherSnapshot.valid = false;
        weatherSnapshot.error =
            transitink::uiText(transitink::UiTextId::WifiDisconnected);
        scheduleNextWeatherRefresh(60);
    }

    Serial.println("Config portal deferred until button press");
    wakeStartedAtMs = millis();
    refreshAllWidgetsNow();
}

void loop() {
    serviceFactoryResetButtons();
    serviceChargeStatus();
    if (factoryResetPendingRestart) {
        delay(20);
        return;
    }
    configPortal.loop();
    serviceConfigButton();
    serviceWidgetPageButton();
    if (configAccessMode) {
        delay(5);
        return;
    }
    if (hasUsableConfig(deviceConfig)) {
        const bool scheduledWakeWindowActive = scheduledWakeWindowActiveNow();
        if (scheduledWakeWindowActive) {
            scheduledWakeSession = true;
        }
        const bool sleepBlocked = configAccessMode || chargeSnapshot.powerPresent ||
                                  homeWakeRefreshActive();
        if (bus_eta::shouldAutoSleep(
                sleepSettingsFromConfig(),
                wakeStartedAtMs,
                millis(),
                sleepBlocked,
                scheduledWakeSession,
                scheduledWakeWindowActive)) {
            enterSleepMode(scheduledWakeSession
                               ? "scheduled wake window ended"
                               : "button wake window expired");
        } else if (homeWakeRefreshActive()) {
            serviceHomeWakeRefresh();
        } else {
            if (millis() >= nextWeatherRefreshMs) {
                refreshWeatherNow();
            }
            if (millis() >= nextClockRefreshMs) {
                refreshClockNow();
            }
            serviceOneWidgetIfDue();
        }
    }
    delay(5);
}

#endif  // BUTTON_MAPPER
