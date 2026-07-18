#ifndef BUTTON_MAPPER

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <ctime>

#include "AppConfig.h"
#include "BatteryMonitor.h"
#include "ConfigPortal.h"
#include "ConfigStore.h"
#include "CitybusClient.h"
#include "EInkDisplay.h"
#include "GmbClient.h"
#include "JourneyTimeClient.h"
#include "KmbClient.h"
#include "LightRailClient.h"
#include "MtrClient.h"
#include "ProductConfig.h"
#include "WeatherClient.h"
#include "WidgetCatalogService.h"
#include "core/BusEtaCore.h"
#include "core/WidgetScheduler.h"
#include "hardware/BoardProfile.h"
#include "hardware/BoardSupport.h"
#include "providers/BusProvider.h"
#include "providers/GmbProvider.h"
#include "providers/JourneyTimeProvider.h"
#include "providers/LightRailProvider.h"
#include "providers/MtrProvider.h"
#include "providers/WidgetProviderRouter.h"

ConfigStore configStore;
BatteryMonitor chargeMonitor;
DeviceConfig deviceConfig;
KmbClient kmbClient;
CitybusClient citybusClient;
GmbClient gmbClient;
MtrClient mtrClient;
LightRailClient lightRailClient;
JourneyTimeClient journeyTimeClient;
BusProvider busProvider(kmbClient, citybusClient);
GmbProvider gmbProvider(gmbClient);
MtrProvider mtrProvider(mtrClient);
LightRailProvider lightRailProvider(lightRailClient);
JourneyTimeProvider journeyTimeProvider(journeyTimeClient);
WidgetProviderRouter widgetProviderRouter(
    busProvider, gmbProvider, mtrProvider, lightRailProvider, journeyTimeProvider);
transitink::WidgetScheduler widgetScheduler(widgetProviderRouter);
WidgetCatalogService widgetCatalogService(kmbClient, citybusClient, gmbClient);
WeatherClient weatherClient;
WeatherSnapshot weatherSnapshot;
EInkDisplay einkDisplay;
ConfigPortal configPortal(deviceConfig, configStore, widgetCatalogService);

unsigned long nextClockRefreshMs = 0;
unsigned long nextWeatherRefreshMs = 0;
unsigned long wakeStartedAtMs = 0;
unsigned long lastChargeStatusPollMs = 0;
bus_eta::BatterySnapshot chargeSnapshot;
bus_eta::DualButtonHoldDetector factoryResetDetector(
    transitink::hardware::kBoardProfile.buttons.factoryResetHoldMs);
bus_eta::SingleButtonClickDetector configButtonDetector(
    transitink::hardware::kBoardProfile.buttons.configDebounceMs,
    transitink::hardware::kBoardProfile.buttons.configMaxClickMs);
bool factoryResetPendingRestart = false;
bool factoryResetApplied = false;
bool configAccessMode = false;
bool dashboardVisible = false;
bool sleepMaintenanceWake = false;
bool sleepScreenPrepared = false;
bool chargeStatusLogged = false;
constexpr uint32_t kSleepResumeMarker = 0x54524E53U;
RTC_NOINIT_ATTR uint32_t sleepResumeMarker;
RTC_NOINIT_ATTR uint32_t sleepResumeMarkerInverse;

void serviceFactoryResetButtons();
void serviceConfigButton();
void setupFactoryResetButtons();
void showConfigAccessScreen();
void returnToDashboard();
void refreshWeatherNow();
void refreshAllWidgetsNow();
void serviceOneWidgetIfDue();
transitink::WidgetSnapshotSet currentDisplaySnapshots();
bool hasValidTime();
void syncTimeAndWeatherBeforeDashboard(bool homeWake);
bus_eta::SleepSettings sleepSettingsFromConfig();
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

bool connectWifi(const DeviceConfig& config) {
    if (config.wifiSsid.isEmpty()) {
        return false;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
    Serial.print("Connecting Wi-Fi SSID: ");
    Serial.println(config.wifiSsid);
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

void syncTimeAndWeatherBeforeDashboard(bool homeWake) {
    configTzTime("HKT-8", "pool.ntp.org", "time.cloudflare.com", "time.nist.gov");
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
        weatherSnapshot.error = "Wi-Fi 未連接";
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

transitink::WidgetSnapshotSet currentDisplaySnapshots() {
    const int64_t nowEpoch = hasValidTime() ? static_cast<int64_t>(time(nullptr)) : 0;
    return widgetScheduler.displaySnapshots(nowEpoch);
}

void refreshAllWidgetsNow() {
    Serial.println("Widget refresh all start");
    const uint32_t nowMs = millis();
    const int64_t nowEpoch = hasValidTime() ? static_cast<int64_t>(time(nullptr)) : 0;
    widgetScheduler.forceAllDue(nowMs);
    for (std::size_t attempts = 0;
         attempts < transitink::kWidgetSlotCount && widgetScheduler.hasPendingDue(nowMs);
         ++attempts) {
        widgetScheduler.serviceNextDue(nowMs, nowEpoch);
    }
    einkDisplay.showDashboard(currentDisplaySnapshots(), weatherSnapshot);
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
    einkDisplay.refreshWidgetLane(tick.slot, currentDisplaySnapshots(), weatherSnapshot);
}

bus_eta::SleepSettings sleepSettingsFromConfig() {
    bus_eta::SleepSettings settings;
    settings.enabled = deviceConfig.sleepEnabled;
    settings.wakeDurationMinutes = deviceConfig.wakeDurationMinutes;
    settings.maintenanceHours = deviceConfig.sleepMaintenanceHours;
    return settings;
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
    transitink::hardware::configureChargeWakeup();

    const unsigned long long maintenanceUs = bus_eta::sleepMaintenanceIntervalUs(sleepSettingsFromConfig());
    if (maintenanceUs > 0) {
        esp_sleep_enable_timer_wakeup(maintenanceUs);
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
    wakeStartedAtMs = millis();
    bool wifiOk = connectWifi(deviceConfig);
    if (wifiOk) {
        syncTimeAndWeatherBeforeDashboard(true);
    } else {
        weatherSnapshot.valid = false;
        weatherSnapshot.error = "Wi-Fi 未連接";
        scheduleNextWeatherRefresh(60);
    }
    refreshAllWidgetsNow();
    clearPersistentSleepResumeMarker();
}

void performLightSleepMaintenance() {
    Serial.println("Light sleep maintenance wake");
    setupFactoryResetButtons();
    einkDisplay.begin(false);
    bool wifiOk = connectWifi(deviceConfig);
    if (wifiOk) {
        syncTimeAndWeatherBeforeDashboard(false);
    } else {
        weatherSnapshot.valid = false;
        weatherSnapshot.error = "Wi-Fi 未連接";
        scheduleNextWeatherRefresh(60);
    }
    const uint32_t nowMs = millis();
    const int64_t nowEpoch = hasValidTime() ? static_cast<int64_t>(time(nullptr)) : 0;
    widgetScheduler.forceAllDue(nowMs);
    for (std::size_t attempts = 0;
         attempts < transitink::kWidgetSlotCount && widgetScheduler.hasPendingDue(nowMs);
         ++attempts) {
        widgetScheduler.serviceNextDue(nowMs, nowEpoch);
    }
    const transitink::WidgetSnapshotSet sleepSnapshots =
        widgetScheduler.displaySnapshots(nowEpoch);
    dashboardVisible = false;
    configAccessMode = false;
    einkDisplay.showSleep(sleepSnapshots, weatherSnapshot);
    stopNetworkForSleep();
    einkDisplay.prepareForSleep();
    sleepScreenPrepared = true;
}

void enterSleepMode(const char* reason) {
    if (!deviceConfig.sleepEnabled || factoryResetPendingRestart) {
        return;
    }
    Serial.print("Entering sleep mode: ");
    Serial.println(reason);
    if (!sleepScreenPrepared) {
        dashboardVisible = false;
        configAccessMode = false;
        einkDisplay.showSleep(currentDisplaySnapshots(), weatherSnapshot);
        stopNetworkForSleep();
        einkDisplay.prepareForSleep();
    }
    sleepScreenPrepared = false;
    setupFactoryResetButtons();

    while (deviceConfig.sleepEnabled && !factoryResetPendingRestart) {
        configureLightSleepWakeup();
        armSleepResumeMarker();
        esp_err_t sleepResult = esp_light_sleep_start();
        clearSleepResumeMarker();
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        transitink::hardware::disableHomeWakeup();
        transitink::hardware::disableChargeWakeup();
        setupFactoryResetButtons();

        if (sleepResult != ESP_OK) {
            Serial.print("Light sleep failed: ");
            Serial.println(static_cast<int>(sleepResult));
            delay(200);
            continue;
        }

        const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
        Serial.print("Light sleep wake cause: ");
        Serial.println(static_cast<int>(wakeCause));
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
            waitForHomeRelease();
            if (!factoryResetPendingRestart) {
                returnFromLightSleep();
            }
            return;
        }
        if (sleepMaintenanceWake) {
            performLightSleepMaintenance();
            continue;
        }
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
            performLightSleepMaintenance();
            continue;
        }
        delay(50);
    }
}

void setupFactoryResetButtons() {
    transitink::hardware::configureButtons();
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
    einkDisplay.showWifiStatus("已重設裝置\n放開音量鍵後重啟");
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
    if (factoryResetDetector.update(upPressed, downPressed, millis())) {
        applyFactoryReset();
    }
}

String configPageUrl() {
    if (WiFi.status() == WL_CONNECTED) {
        return "http://" + WiFi.localIP().toString() + "/";
    }
    return "http://192.168.4.1/";
}

String currentWifiLabel() {
    if (deviceConfig.wifiSsid.isEmpty()) {
        return "未設定";
    }
    if (WiFi.status() == WL_CONNECTED) {
        return deviceConfig.wifiSsid + "（已連接）";
    }
    return deviceConfig.wifiSsid + "（未連接）";
}

void showConfigAccessScreen() {
    Serial.println("Config button clicked");
    configPortal.begin(WiFi.status() != WL_CONNECTED);
    configAccessMode = true;
    dashboardVisible = false;
    String configUrl = configPageUrl();
    String networkName = WiFi.status() == WL_CONNECTED ? deviceConfig.wifiSsid : configApSsid();
    String message = "裝置設定\n" + configUrl + "\n目前 Wi-Fi\n" + currentWifiLabel() + "\n掃描右方 QR Code";
    einkDisplay.showConfigMode(networkName, message, configUrl);
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
    const bool configPressed = transitink::hardware::configButtonPressed();
    const bool downPressed = transitink::hardware::factoryResetDownButtonPressed();
    if (configButtonDetector.update(configPressed, downPressed || factoryResetPendingRestart, millis())) {
        if (configAccessMode) {
            returnToDashboard();
        } else {
            showConfigAccessScreen();
        }
    }
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
    const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    const bool rtcResetWake = consumeSleepResumeMarker();
    const bool configStoreReady = configStore.begin();
    const bool persistentResetWake = configStoreReady && configStore.sleepResumePending();
    const bool resetWake = rtcResetWake || persistentResetWake;
    sleepMaintenanceWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
    const bool homeWake = wakeCause == ESP_SLEEP_WAKEUP_GPIO || resetWake;
    Serial.print("Wake cause: ");
    Serial.println(static_cast<int>(wakeCause));
    Serial.print("Reset wake marker: ");
    Serial.println(persistentResetWake ? "persistent" : (rtcResetWake ? "rtc" : "no"));
    setupFactoryResetButtons();
    chargeMonitor.begin();
    serviceChargeStatus(true);
    // Keep the retained e-paper dashboard visible until fresh data is ready.
    // Battery wake can reset the MCU, so a transient boot screen is never safe here.
    einkDisplay.begin(false);

    bool loaded = configStore.load(deviceConfig);
    Serial.print("Config loaded: ");
    Serial.println(loaded ? "yes" : "no");
    if (!loaded || !hasUsableConfig(deviceConfig)) {
        Serial.println("Starting config portal: missing or invalid config");
        configPortal.begin(true);
        einkDisplay.showConfigMode(
            configApSsid(),
            String("連接此熱點\n開啟 http://192.168.4.1/\n完成 ") +
                FIRMWARE_SHORT_NAME + " 設定");
        return;
    }

    widgetScheduler.configure(deviceConfig.widgets, millis());

    if (deviceConfig.sleepEnabled && sleepMaintenanceWake) {
        performLightSleepMaintenance();
        sleepMaintenanceWake = false;
        enterSleepMode("maintenance complete");
        return;
    }

    bool wifiOk = connectWifi(deviceConfig);
    if (wifiOk) {
        syncTimeAndWeatherBeforeDashboard(homeWake);
    } else {
        weatherSnapshot.valid = false;
        weatherSnapshot.error = "Wi-Fi 未連接";
        scheduleNextWeatherRefresh(60);
    }

    Serial.println("Config portal deferred until button press");
    wakeStartedAtMs = millis();
    refreshAllWidgetsNow();
    if (resetWake) {
        clearPersistentSleepResumeMarker();
    }
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
    if (configAccessMode) {
        delay(5);
        return;
    }
    if (hasUsableConfig(deviceConfig)) {
        const bool sleepBlocked = configAccessMode || chargeSnapshot.powerPresent;
        if (bus_eta::shouldAutoSleep(
                sleepSettingsFromConfig(), wakeStartedAtMs, millis(), sleepBlocked)) {
            enterSleepMode("wake window expired");
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
