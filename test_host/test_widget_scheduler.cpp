#include "core/WidgetScheduler.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using transitink::Freshness;
using transitink::ProviderOutcome;
using transitink::ProviderResult;
using transitink::WidgetConfig;
using transitink::WidgetSnapshot;
using transitink::WidgetState;
using transitink::WidgetType;

WidgetConfig configFor(WidgetType type) {
    WidgetConfig config;
    config.type = type;
    if (type == WidgetType::BusEta) {
        config.bus.routeId = "11";
        config.bus.directionId = "I";
        config.bus.serviceType = "1";
        config.bus.stopId = "STOP-A";
    } else if (type == WidgetType::MtrEta) {
        config.mtr.lineOrRouteId = "TML";
        config.mtr.stationId = "YUL";
        config.mtr.directionId = "UP";
    } else if (type == WidgetType::JourneyTime) {
        config.journeyTime.locationId = "H1";
        config.journeyTime.destinationId = "K1";
    }
    return config;
}

ProviderResult ready(uint8_t slot, WidgetType type, int64_t fetchedAt, int64_t eventEpoch,
                     const std::string& text) {
    WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = type;
    snapshot.valueCount = 1;
    snapshot.values[0] = {text, "", eventEpoch};
    snapshot.state = WidgetState::Ready;
    snapshot.fetchedAtEpoch = fetchedAt;
    snapshot.dataAtEpoch = fetchedAt;
    return {ProviderOutcome::Success, snapshot};
}

ProviderResult outcome(ProviderOutcome providerOutcome, uint8_t slot, WidgetType type,
                       const std::string& message = {}) {
    WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = type;
    snapshot.state = providerOutcome == ProviderOutcome::Empty ? WidgetState::Empty
                                                                : WidgetState::Error;
    snapshot.providerMessage = message;
    return {providerOutcome, snapshot};
}

class FakeRouter : public transitink::IWidgetProviderRouter {
public:
    std::array<std::vector<ProviderResult>, transitink::kWidgetSlotCount> scripted{};
    std::array<std::size_t, transitink::kWidgetSlotCount> cursors{};
    std::vector<uint8_t> calls;

    ProviderResult fetch(uint8_t slot, const WidgetConfig&, int64_t) override {
        calls.push_back(slot);
        assert(cursors[slot] < scripted[slot].size());
        return scripted[slot][cursors[slot]++];
    }
};

}  // namespace

int main() {
    using namespace transitink;

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = configFor(WidgetType::BusEta);
        configs[1] = configFor(WidgetType::MtrEta);
        configs[2] = configFor(WidgetType::JourneyTime);
        router.scripted[0] = {ready(0, WidgetType::BusEta, 1000, 1300, "巴士")};
        router.scripted[1] = {ready(1, WidgetType::MtrEta, 1000, 1300, "港鐵")};
        router.scripted[2] = {ready(2, WidgetType::JourneyTime, 1000, 0, "24 分鐘")};

        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 500);
        assert(scheduler.hasEnabledWidgets());
        assert(scheduler.hasPendingDue(500));

        const auto first = scheduler.serviceNextDue(500, 1000);
        assert(first.ran && first.slot == 0 && first.success);
        assert(router.calls.size() == 1);
        const auto second = scheduler.serviceNextDue(500, 1000);
        assert(second.ran && second.slot == 1 && second.success);
        assert(router.calls.size() == 2);
        const auto third = scheduler.serviceNextDue(500, 1000);
        assert(third.ran && third.slot == 2 && third.success);
        assert(router.calls.size() == 3);
        assert(!scheduler.serviceNextDue(500, 1000).ran);
        assert((router.calls == std::vector<uint8_t>{0, 1, 2}));
        assert(!scheduler.hasPendingDue(500));
        assert(!scheduler.hasPendingDue(500 + 29999));

        router.scripted[1].push_back(ready(1, WidgetType::MtrEta, 1030, 1330, "港鐵 2"));
        assert(scheduler.hasPendingDue(500 + 30000));
        const auto railDue = scheduler.serviceNextDue(500 + 30000, 1030);
        assert(railDue.ran && railDue.slot == 1);

        router.scripted[0].push_back(ready(0, WidgetType::BusEta, 1060, 1360, "巴士 2"));
        router.scripted[1].push_back(ready(1, WidgetType::MtrEta, 1060, 1360, "港鐵 3"));
        const auto fairBus = scheduler.serviceNextDue(500 + 60000, 1060);
        const auto fairRail = scheduler.serviceNextDue(500 + 60000, 1060);
        assert(fairBus.slot == 0);
        assert(fairRail.slot == 1);
        assert(router.calls.back() == 1);

        router.scripted[2].push_back(ready(2, WidgetType::JourneyTime, 1060, 0, "25 分鐘"));
        scheduler.forceAllDue(500 + 60000);
        const std::size_t before = router.calls.size();
        const auto forced = scheduler.serviceNextDue(500 + 60000, 1060);
        assert(forced.ran && forced.slot == 2);
        assert(router.calls.size() == before + 1);
        assert(scheduler.snapshot(3).type == WidgetType::Disabled);
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = configFor(WidgetType::BusEta);
        router.scripted[0] = {
            ready(0, WidgetType::BusEta, 1000, 2000, "原有班次"),
            outcome(ProviderOutcome::Failure, 0, WidgetType::BusEta),
            outcome(ProviderOutcome::Failure, 0, WidgetType::BusEta),
            outcome(ProviderOutcome::Failure, 0, WidgetType::BusEta),
            ready(0, WidgetType::BusEta, 1240, 2200, "恢復班次"),
        };

        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 100);
        assert(scheduler.serviceNextDue(100, 1000).success);

        const auto failedOnce = scheduler.serviceNextDue(60100, 1060);
        assert(failedOnce.ran && !failedOnce.success);
        assert(scheduler.snapshot(0).freshness == Freshness::Stale);
        assert(scheduler.snapshot(0).consecutiveFailures == 1);
        assert(scheduler.snapshot(0).valueCount == 1);
        assert(scheduler.snapshot(0).values[0].text == "原有班次");

        scheduler.serviceNextDue(120100, 1120);
        assert(scheduler.snapshot(0).valueCount == 1);
        scheduler.serviceNextDue(180100, 1180);
        assert(scheduler.snapshot(0).valueCount == 0);
        assert(scheduler.snapshot(0).state == WidgetState::Error);
        assert(scheduler.snapshot(0).providerMessage == "資料已逾期");
        assert(scheduler.snapshot(0).consecutiveFailures == 3);

        const auto recovered = scheduler.serviceNextDue(240100, 1240);
        assert(recovered.success);
        assert(scheduler.snapshot(0).freshness == Freshness::Fresh);
        assert(scheduler.snapshot(0).consecutiveFailures == 0);
        assert(scheduler.snapshot(0).valueCount == 1);
        assert(scheduler.snapshot(0).values[0].text == "恢復班次");
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = configFor(WidgetType::BusEta);
        configs[1] = configFor(WidgetType::BusEta);
        router.scripted[0] = {
            outcome(ProviderOutcome::Failure, 0, WidgetType::BusEta),
        };
        router.scripted[1] = {
            ready(1, WidgetType::BusEta, 1000, 1300, "第二格"),
        };

        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 10);
        scheduler.serviceNextDue(10, 1000);
        assert(scheduler.snapshot(0).state == WidgetState::Error);
        assert(scheduler.snapshot(0).providerMessage == "暫未能取得資料");
        assert(scheduler.snapshot(0).valueCount == 0);
        assert(scheduler.snapshot(1).valueCount == 0);
        scheduler.serviceNextDue(10, 1000);
        assert(scheduler.snapshot(1).values[0].text == "第二格");
        assert(scheduler.snapshot(0).providerMessage == "暫未能取得資料");
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = configFor(WidgetType::BusEta);
        router.scripted[0] = {
            ready(0, WidgetType::BusEta, 1000, 2000, "舊資料"),
            outcome(ProviderOutcome::InvalidConfig, 0, WidgetType::BusEta, "設定不完整"),
            ready(0, WidgetType::BusEta, 1120, 2000, "新資料"),
            outcome(ProviderOutcome::ClockUnsynced, 0, WidgetType::BusEta,
                    "時間尚未同步"),
        };

        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 0);
        scheduler.serviceNextDue(0, 1000);
        scheduler.serviceNextDue(60000, 1060);
        assert(scheduler.snapshot(0).valueCount == 0);
        assert(scheduler.snapshot(0).providerMessage == "設定不完整");
        scheduler.serviceNextDue(120000, 1120);
        assert(scheduler.snapshot(0).valueCount == 1);
        scheduler.serviceNextDue(180000, 0);
        assert(scheduler.snapshot(0).valueCount == 0);
        assert(scheduler.snapshot(0).providerMessage == "時間尚未同步");
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = configFor(WidgetType::BusEta);
        router.scripted[0] = {ready(0, WidgetType::BusEta, 1000, 1050, "快到期")};
        WidgetScheduler scheduler(router);
        scheduler.configure(configs, 0);
        scheduler.serviceNextDue(0, 1000);
        const auto beforeExpiry = scheduler.displaySnapshots(1049);
        assert(beforeExpiry[0].valueCount == 1);
        const auto afterExpiry = scheduler.displaySnapshots(1050);
        assert(afterExpiry[0].valueCount == 0);
        assert(afterExpiry[0].state == WidgetState::Empty);
        assert(afterExpiry[0].providerMessage == "暫無班次");
    }

    {
        FakeRouter router;
        WidgetSlots configs{};
        configs[0] = configFor(WidgetType::BusEta);
        router.scripted[0] = {ready(0, WidgetType::BusEta, 1000, 2000, "繞回")};
        WidgetScheduler scheduler(router);
        constexpr uint32_t configuredAt = 0xfffffff5U;
        scheduler.configure(configs, configuredAt);
        scheduler.serviceNextDue(configuredAt, 1000);
        assert(!scheduler.hasPendingDue(20));
        assert(!scheduler.hasPendingDue(59988));
        router.scripted[0].push_back(ready(0, WidgetType::BusEta, 1060, 2030, "繞回 2"));
        assert(scheduler.hasPendingDue(59989));
        assert(scheduler.serviceNextDue(59989, 1060).ran);
    }

    {
        FakeRouter router;
        WidgetSlots initial{};
        initial[0] = configFor(WidgetType::BusEta);
        initial[1] = configFor(WidgetType::MtrEta);
        router.scripted[0] = {ready(0, WidgetType::BusEta, 1000, 1300, "舊巴士")};
        router.scripted[1] = {ready(1, WidgetType::MtrEta, 1000, 1300, "舊港鐵")};

        WidgetScheduler scheduler(router);
        scheduler.configure(initial, 100);
        scheduler.serviceNextDue(100, 1000);
        scheduler.serviceNextDue(100, 1000);
        assert(scheduler.snapshot(0).valueCount == 1);
        assert(scheduler.snapshot(1).valueCount == 1);

        WidgetSlots reconfigured{};
        reconfigured[1] = configFor(WidgetType::MtrEta);
        reconfigured[1].mtr.stationId = "MKK";
        const std::size_t callsBeforeReconfigure = router.calls.size();
        scheduler.configure(reconfigured, 500);

        assert(scheduler.snapshot(0).type == WidgetType::Disabled);
        assert(scheduler.snapshot(0).valueCount == 0);
        assert(scheduler.snapshot(1).type == WidgetType::MtrEta);
        assert(scheduler.snapshot(1).valueCount == 0);
        assert(router.calls.size() == callsBeforeReconfigure);
        assert(!scheduler.hasPendingDue(499));
        assert(scheduler.hasPendingDue(500));

        router.scripted[1].push_back(ready(1, WidgetType::MtrEta, 1001, 1400, "新港鐵"));
        const auto refreshed = scheduler.serviceNextDue(500, 1001);
        assert(refreshed.ran && refreshed.slot == 1);
        assert(scheduler.snapshot(1).values[0].text == "新港鐵");
    }

    {
        FakeRouter router;
        WidgetScheduler scheduler(router);
        scheduler.configure(WidgetSlots{}, 0);
        assert(!scheduler.hasEnabledWidgets());
        assert(!scheduler.hasPendingDue(0));
        assert(!scheduler.serviceNextDue(0, 1000).ran);
        assert(router.calls.empty());
    }

    return 0;
}
