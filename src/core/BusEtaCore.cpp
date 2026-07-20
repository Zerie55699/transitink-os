#include "core/BusEtaCore.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace bus_eta {
namespace {

constexpr const char* kKmbBase = "https://data.etabus.gov.hk/v1/transport/kmb";

std::string normalizeBoundForRouteStop(const std::string& bound) {
    if (bound == "I" || bound == "i" || bound == "inbound") {
        return "inbound";
    }
    return "outbound";
}

std::string normalizeBoundForRoute(const std::string& bound) {
    if (bound == "I" || bound == "i" || bound == "inbound") {
        return "inbound";
    }
    return "outbound";
}

int parseInt(const std::string& value, std::size_t offset, std::size_t len) {
    if (offset + len > value.size()) {
        return 0;
    }
    return std::atoi(value.substr(offset, len).c_str());
}

long daysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<long>(era) * 146097L + static_cast<long>(doe) - 719468L;
}

bool sameText(const std::string& a, const std::string& b) {
    return a == b;
}

}  // namespace

std::string kmbRoutesUrl() {
    return std::string(kKmbBase) + "/route/";
}

std::string kmbRouteUrl(const std::string& route, const std::string& bound, const std::string& serviceType) {
    return std::string(kKmbBase) + "/route/" + route + "/" + normalizeBoundForRoute(bound) + "/" + serviceType;
}

std::string kmbStopsUrl() {
    return std::string(kKmbBase) + "/stop";
}

std::string kmbStopUrl(const std::string& stopId) {
    return std::string(kKmbBase) + "/stop/" + stopId;
}

std::string kmbRouteStopsUrl(const std::string& route, const std::string& bound, const std::string& serviceType) {
    return std::string(kKmbBase) + "/route-stop/" + route + "/" + normalizeBoundForRouteStop(bound) + "/" + serviceType;
}

std::string kmbEtaUrl(const std::string& stopId, const std::string& route, const std::string& serviceType) {
    return std::string(kKmbBase) + "/eta/" + stopId + "/" + route + "/" + serviceType;
}

DualButtonHoldDetector::DualButtonHoldDetector(unsigned long thresholdMs) : thresholdMs_(thresholdMs) {}

bool DualButtonHoldDetector::update(bool firstPressed, bool secondPressed, unsigned long nowMs) {
    const bool bothPressed = firstPressed && secondPressed;
    if (!bothPressed) {
        armed_ = true;
        wasBothPressed_ = false;
        fired_ = false;
        bothPressedAtMs_ = 0;
        return false;
    }
    if (!armed_) {
        return false;
    }
    if (!wasBothPressed_) {
        wasBothPressed_ = true;
        fired_ = false;
        bothPressedAtMs_ = nowMs;
        return false;
    }
    if (!fired_ && nowMs - bothPressedAtMs_ >= thresholdMs_) {
        fired_ = true;
        return true;
    }
    return false;
}

SingleButtonClickDetector::SingleButtonClickDetector(unsigned long debounceMs, unsigned long maxClickMs)
    : debounceMs_(debounceMs), maxClickMs_(maxClickMs) {}

bool SingleButtonClickDetector::update(bool pressed, bool inhibited, unsigned long nowMs) {
    if (inhibited) {
        cancelled_ = true;
        wasPressed_ = pressed;
        if (!pressed) {
            pressedAtMs_ = 0;
        }
        return false;
    }
    if (pressed) {
        if (!wasPressed_) {
            pressedAtMs_ = nowMs;
            cancelled_ = false;
        }
        wasPressed_ = true;
        return false;
    }
    if (!wasPressed_) {
        cancelled_ = false;
        return false;
    }

    wasPressed_ = false;
    const unsigned long durationMs = nowMs - pressedAtMs_;
    pressedAtMs_ = 0;
    const bool clicked = !cancelled_ && durationMs >= debounceMs_ && durationMs <= maxClickMs_;
    cancelled_ = false;
    return clicked;
}

DebouncedButtonPressDetector::DebouncedButtonPressDetector(unsigned long debounceMs)
    : debounceMs_(debounceMs) {}

bool DebouncedButtonPressDetector::update(bool pressed, unsigned long nowMs) {
    if (pressed != rawPressed_) {
        rawPressed_ = pressed;
        rawChangedAtMs_ = nowMs;
        return false;
    }
    if (stablePressed_ == rawPressed_ || nowMs - rawChangedAtMs_ < debounceMs_) {
        return false;
    }
    stablePressed_ = rawPressed_;
    return stablePressed_;
}

bool shouldAutoSleep(const SleepSettings& settings, unsigned long wakeStartedAtMs, unsigned long nowMs, bool configAccessMode) {
    if (!settings.enabled || configAccessMode || settings.wakeDurationMinutes == 0 || wakeStartedAtMs == 0) {
        return false;
    }
    const unsigned long wakeDurationMs = settings.wakeDurationMinutes * 60UL * 1000UL;
    return nowMs - wakeStartedAtMs >= wakeDurationMs;
}

unsigned long long sleepMaintenanceIntervalUs(const SleepSettings& settings) {
    if (!settings.enabled || settings.maintenanceHours == 0) {
        return 0;
    }
    return static_cast<unsigned long long>(settings.maintenanceHours) * 60ULL * 60ULL * 1000000ULL;
}

SleepResumeAction decideSleepResumeAction(bool sleepMarkerPending,
                                          bool timerWake,
                                          bool homeGpioWake,
                                          bool homePressedAtBoot,
                                          bool powerOnReset) {
    if (timerWake) {
        return SleepResumeAction::RunMaintenance;
    }
    if (homeGpioWake ||
        (sleepMarkerPending && (homePressedAtBoot || powerOnReset))) {
        return SleepResumeAction::ShowDashboard;
    }
    if (sleepMarkerPending) {
        return SleepResumeAction::ResumeSleep;
    }
    return SleepResumeAction::NormalBoot;
}

long parseHongKongIso(const std::string& iso) {
    if (iso.size() < 19) {
        return 0;
    }
    const int year = parseInt(iso, 0, 4);
    const int month = parseInt(iso, 5, 2);
    const int day = parseInt(iso, 8, 2);
    const int hour = parseInt(iso, 11, 2);
    const int minute = parseInt(iso, 14, 2);
    const int second = parseInt(iso, 17, 2);
    long epoch = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day)) * 86400L;
    epoch += static_cast<long>(hour) * 3600L + static_cast<long>(minute) * 60L + second;
    if (iso.size() >= 25 && (iso[19] == '+' || iso[19] == '-')) {
        const int sign = iso[19] == '+' ? 1 : -1;
        const int offsetHours = parseInt(iso, 20, 2);
        const int offsetMinutes = parseInt(iso, 23, 2);
        epoch -= sign * (offsetHours * 3600L + offsetMinutes * 60L);
    }
    return epoch;
}

std::string formatCountdown(int secondsUntil) {
    if (secondsUntil == NoEtaSeconds) {
        return "暫無班次";
    }
    if (secondsUntil < 60) {
        return "即將到站";
    }
    const int minutes = (secondsUntil + 59) / 60;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d 分鐘", minutes);
    return buffer;
}

std::vector<DisplayEta> selectEtas(
    const std::vector<EtaRecord>& records,
    const RouteSelection& selection,
    long nowEpoch,
    std::size_t limit) {
    std::vector<DisplayEta> selected;
    for (const auto& record : records) {
        if (!sameText(record.route, selection.route) ||
            !sameText(record.dir, selection.bound) ||
            !sameText(record.serviceType, selection.serviceType)) {
            continue;
        }
        const long etaEpoch = parseHongKongIso(record.etaIso);
        const int secondsUntil = static_cast<int>(etaEpoch - nowEpoch);
        selected.push_back({record.etaSeq, secondsUntil, formatCountdown(secondsUntil), record.etaIso, record.remarkTc});
    }
    std::sort(selected.begin(), selected.end(), [](const DisplayEta& left, const DisplayEta& right) {
        return left.etaSeq < right.etaSeq;
    });
    if (selected.size() > limit) {
        selected.resize(limit);
    }
    return selected;
}

}  // namespace bus_eta
