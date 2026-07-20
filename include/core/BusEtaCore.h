#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace bus_eta {

constexpr int NoEtaSeconds = -1;

struct RouteSelection {
    std::string route;
    std::string bound;
    std::string serviceType;
    std::string stopId;
    std::string destTc;
};

struct EtaRecord {
    std::string route;
    std::string dir;
    std::string serviceType;
    int etaSeq;
    std::string etaIso;
    std::string destTc;
    std::string remarkTc;
};

struct DisplayEta {
    int etaSeq;
    int secondsUntil;
    std::string countdownText;
    std::string etaIso;
    std::string remarkTc;
};

class DualButtonHoldDetector {
public:
    explicit DualButtonHoldDetector(unsigned long thresholdMs);
    bool update(bool firstPressed, bool secondPressed, unsigned long nowMs);

private:
    unsigned long thresholdMs_;
    unsigned long bothPressedAtMs_ = 0;
    bool armed_ = false;
    bool wasBothPressed_ = false;
    bool fired_ = false;
};

class SingleButtonClickDetector {
public:
    SingleButtonClickDetector(unsigned long debounceMs, unsigned long maxClickMs);
    bool update(bool pressed, bool inhibited, unsigned long nowMs);

private:
    unsigned long debounceMs_;
    unsigned long maxClickMs_;
    unsigned long pressedAtMs_ = 0;
    bool wasPressed_ = false;
    bool cancelled_ = false;
};

class DebouncedButtonPressDetector {
public:
    explicit DebouncedButtonPressDetector(unsigned long debounceMs);
    bool update(bool pressed, unsigned long nowMs);

private:
    unsigned long debounceMs_;
    unsigned long rawChangedAtMs_ = 0;
    bool rawPressed_ = false;
    bool stablePressed_ = false;
};

struct SleepSettings {
    bool enabled = true;
    unsigned int wakeDurationMinutes = 5;
    unsigned int maintenanceHours = 12;
};

enum class SleepResumeAction {
    NormalBoot,
    ShowDashboard,
    RunMaintenance,
    ResumeSleep,
};

std::string kmbRoutesUrl();
std::string kmbRouteUrl(const std::string& route, const std::string& bound, const std::string& serviceType);
std::string kmbStopsUrl();
std::string kmbStopUrl(const std::string& stopId);
std::string kmbRouteStopsUrl(const std::string& route, const std::string& bound, const std::string& serviceType);
std::string kmbEtaUrl(const std::string& stopId, const std::string& route, const std::string& serviceType);

long parseHongKongIso(const std::string& iso);
std::string formatCountdown(int secondsUntil);
std::vector<DisplayEta> selectEtas(
    const std::vector<EtaRecord>& records,
    const RouteSelection& selection,
    long nowEpoch,
    std::size_t limit);
bool shouldAutoSleep(const SleepSettings& settings, unsigned long wakeStartedAtMs, unsigned long nowMs, bool configAccessMode);
unsigned long long sleepMaintenanceIntervalUs(const SleepSettings& settings);
SleepResumeAction decideSleepResumeAction(bool sleepMarkerPending,
                                          bool timerWake,
                                          bool homeGpioWake,
                                          bool homePressedAtBoot,
                                          bool powerOnReset);

}  // namespace bus_eta
