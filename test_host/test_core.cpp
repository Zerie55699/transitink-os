#include "core/BusEtaCore.h"
#include "core/BatteryStatus.h"

#include <cassert>
#include <string>
#include <vector>

using bus_eta::EtaRecord;
using bus_eta::RouteSelection;

int main() {
    assert(bus_eta::kmbRoutesUrl() == "https://data.etabus.gov.hk/v1/transport/kmb/route/");
    assert(bus_eta::kmbRouteUrl("32M", "O", "1") ==
           "https://data.etabus.gov.hk/v1/transport/kmb/route/32M/outbound/1");
    assert(bus_eta::kmbRouteUrl("32M", "I", "1") ==
           "https://data.etabus.gov.hk/v1/transport/kmb/route/32M/inbound/1");
    assert(bus_eta::kmbStopsUrl() == "https://data.etabus.gov.hk/v1/transport/kmb/stop");
    assert(bus_eta::kmbStopUrl("B15BDCDB640C46BC") ==
           "https://data.etabus.gov.hk/v1/transport/kmb/stop/B15BDCDB640C46BC");
    assert(bus_eta::kmbRouteStopsUrl("1A", "O", "1") ==
           "https://data.etabus.gov.hk/v1/transport/kmb/route-stop/1A/outbound/1");
    assert(bus_eta::kmbRouteStopsUrl("1A", "I", "1") ==
           "https://data.etabus.gov.hk/v1/transport/kmb/route-stop/1A/inbound/1");
    assert(bus_eta::kmbEtaUrl("18492910339410B1", "1", "1") ==
           "https://data.etabus.gov.hk/v1/transport/kmb/eta/18492910339410B1/1/1");

    const long now = bus_eta::parseHongKongIso("2026-07-06T19:10:00+08:00");
    assert(bus_eta::parseHongKongIso("2026-07-06T19:15:00+08:00") - now == 300);
    assert(bus_eta::formatCountdown(300) == "5 分鐘");
    assert(bus_eta::formatCountdown(1) == "即將到站");
    assert(bus_eta::formatCountdown(-30) == "即將到站");
    assert(bus_eta::formatCountdown(bus_eta::NoEtaSeconds) == "暫無班次");

    std::vector<EtaRecord> records = {
        {"1", "I", "1", 2, "2026-07-06T19:20:00+08:00", "竹園邨", ""},
        {"1", "O", "1", 2, "2026-07-06T19:35:00+08:00", "尖沙咀碼頭", "原定班次"},
        {"1", "O", "1", 1, "2026-07-06T19:15:00+08:00", "尖沙咀碼頭", "原定班次"},
    };
    RouteSelection selection{"1", "O", "1", "18492910339410B1", "尖沙咀碼頭"};
    const auto picked = bus_eta::selectEtas(records, selection, now, 2);
    assert(picked.size() == 2);
    assert(picked[0].etaSeq == 1);
    assert(picked[0].countdownText == "5 分鐘");
    assert(picked[1].etaSeq == 2);
    assert(picked[1].countdownText == "25 分鐘");

    bus_eta::DualButtonHoldDetector resetDetector(5000);
    assert(!resetDetector.update(false, false, 1000));
    assert(!resetDetector.update(true, false, 2000));
    assert(!resetDetector.update(true, true, 3000));
    assert(!resetDetector.update(true, true, 7999));
    assert(resetDetector.update(true, true, 8000));
    assert(!resetDetector.update(true, true, 9000));
    assert(!resetDetector.update(false, true, 9100));
    assert(!resetDetector.update(true, true, 10000));
    assert(resetDetector.update(true, true, 15000));

    bus_eta::DualButtonHoldDetector bootHeldDetector(5000);
    assert(!bootHeldDetector.update(true, true, 0));
    assert(!bootHeldDetector.update(true, true, 6000));
    assert(!bootHeldDetector.update(false, false, 6100));
    assert(!bootHeldDetector.update(true, true, 6200));
    assert(!bootHeldDetector.update(true, true, 11199));
    assert(bootHeldDetector.update(true, true, 11200));

    bus_eta::SingleButtonClickDetector configButtonDetector(30, 1200);
    assert(!configButtonDetector.update(false, false, 1000));
    assert(!configButtonDetector.update(true, false, 1100));
    assert(configButtonDetector.update(false, false, 1250));
    assert(!configButtonDetector.update(false, false, 1300));
    assert(!configButtonDetector.update(true, true, 2000));
    assert(!configButtonDetector.update(false, false, 2100));
    assert(!configButtonDetector.update(true, false, 3000));
    assert(!configButtonDetector.update(false, false, 5001));

    bus_eta::DebouncedButtonPressDetector homeButtonDetector(30);
    assert(!homeButtonDetector.update(false, 6000));
    assert(!homeButtonDetector.update(true, 6100));
    assert(!homeButtonDetector.update(true, 6129));
    assert(homeButtonDetector.update(true, 6130));
    assert(!homeButtonDetector.update(true, 6200));
    assert(!homeButtonDetector.update(false, 6300));
    assert(!homeButtonDetector.update(false, 6330));
    assert(!homeButtonDetector.update(true, 6400));
    assert(homeButtonDetector.update(true, 6430));

    bus_eta::SleepSettings sleepSettings;
    assert(sleepSettings.enabled);
    assert(sleepSettings.wakeDurationMinutes == 5);
    assert(sleepSettings.maintenanceHours == 12);
    assert(!bus_eta::shouldAutoSleep(sleepSettings, 1000, 1000 + 5 * 60 * 1000 - 1, false));
    assert(bus_eta::shouldAutoSleep(sleepSettings, 1000, 1000 + 5 * 60 * 1000, false));
    assert(!bus_eta::shouldAutoSleep(sleepSettings, 1000, 1000 + 6 * 60 * 1000, true));
    sleepSettings.enabled = false;
    assert(!bus_eta::shouldAutoSleep(sleepSettings, 1000, 1000 + 6 * 60 * 1000, false));
    sleepSettings.enabled = true;
    sleepSettings.wakeDurationMinutes = 0;
    assert(!bus_eta::shouldAutoSleep(sleepSettings, 1000, 1000 + 6 * 60 * 1000, false));
    sleepSettings.wakeDurationMinutes = 5;
    assert(bus_eta::sleepMaintenanceIntervalUs(sleepSettings) == 43200000000ULL);
    sleepSettings.maintenanceHours = 0;
    assert(bus_eta::sleepMaintenanceIntervalUs(sleepSettings) == 0);

    using bus_eta::SleepResumeAction;
    assert(bus_eta::decideSleepResumeAction(false, false, false, false, false) ==
           SleepResumeAction::NormalBoot);
    assert(bus_eta::decideSleepResumeAction(true, false, false, false, false) ==
           SleepResumeAction::ResumeSleep);
    assert(bus_eta::decideSleepResumeAction(true, false, false, false, true) ==
           SleepResumeAction::ShowDashboard);
    assert(bus_eta::decideSleepResumeAction(true, false, false, true, false) ==
           SleepResumeAction::ShowDashboard);
    assert(bus_eta::decideSleepResumeAction(true, false, true, false, false) ==
           SleepResumeAction::ShowDashboard);
    assert(bus_eta::decideSleepResumeAction(true, true, false, false, true) ==
           SleepResumeAction::RunMaintenance);

    assert(bus_eta::batteryPercentFromMillivolts(0) == 0);
    assert(bus_eta::batteryPercentFromMillivolts(3200) == 0);
    assert(bus_eta::batteryPercentFromMillivolts(3700) > 40);
    assert(bus_eta::batteryPercentFromMillivolts(3700) < 80);
    assert(bus_eta::batteryPercentFromMillivolts(4200) == 100);

    const bus_eta::BatterySnapshot charging =
        bus_eta::batterySnapshotFromSignals(3700, true, true);
    assert(charging.valid);
    assert(charging.powerPresent);
    assert(charging.charging);
    assert(!charging.full);
    assert(charging.percent < 100);
    assert(bus_eta::chargeIndicatorOn(charging, 0));
    assert(!bus_eta::chargeIndicatorOn(charging, 500));
    assert(bus_eta::chargeIndicatorOn(charging, 1000));

    const bus_eta::BatterySnapshot chargingAtHighVoltage =
        bus_eta::batterySnapshotFromSignals(4300, true, true);
    assert(chargingAtHighVoltage.charging);
    assert(!chargingAtHighVoltage.full);
    assert(chargingAtHighVoltage.percent == 99);

    const bus_eta::BatterySnapshot full =
        bus_eta::batterySnapshotFromSignals(4100, false, true);
    assert(full.powerPresent);
    assert(!full.charging);
    assert(full.full);
    assert(full.percent == 100);
    assert(bus_eta::chargeIndicatorOn(full, 0));
    assert(bus_eta::chargeIndicatorOn(full, 500));

    const bus_eta::BatterySnapshot onBattery =
        bus_eta::batterySnapshotFromSignals(3700, false, false);
    assert(!onBattery.powerPresent);
    assert(!onBattery.charging);
    assert(!onBattery.full);
    assert(!bus_eta::chargeIndicatorOn(onBattery, 0));

    return 0;
}
