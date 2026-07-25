#include "providers/WidgetProviderRouter.h"

#include "providers/BusProvider.h"
#include "providers/GmbProvider.h"
#include "providers/JourneyTimeProvider.h"
#include "providers/LightRailProvider.h"
#include "providers/MtrProvider.h"
#include "providers/TflRailProvider.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using transitink::ProviderOutcome;
using transitink::ProviderResult;
using transitink::RailMode;
using transitink::WidgetConfig;
using transitink::WidgetSnapshot;
using transitink::WidgetState;
using transitink::WidgetType;

enum class ProviderKind { Bus, Gmb, Mtr, LightRail, TflRail, JourneyTime };

struct ProviderCall {
    ProviderKind kind;
    uint8_t slot;
    const WidgetConfig* config;
    int64_t nowEpoch;
};

std::vector<ProviderCall> calls;

ProviderResult markerResult(ProviderKind kind, uint8_t slot, WidgetType type,
                            int64_t nowEpoch) {
    WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = type;
    snapshot.state = WidgetState::Ready;
    snapshot.fetchedAtEpoch = nowEpoch;
    snapshot.title = std::to_string(static_cast<int>(kind));
    return {ProviderOutcome::Success, snapshot};
}

WidgetConfig busConfig() {
    WidgetConfig config;
    config.type = WidgetType::BusEta;
    config.bus.routeId = "A31";
    config.bus.directionId = "O";
    config.bus.serviceType = "1";
    config.bus.stopId = "STOP-BUS";
    return config;
}

WidgetConfig railConfig(RailMode mode) {
    WidgetConfig config;
    config.type = WidgetType::MtrEta;
    config.mtr.mode = mode;
    config.mtr.lineOrRouteId =
        mode == RailMode::HeavyRail
            ? "TML"
            : mode == RailMode::LightRail ? "610" : "victoria";
    config.mtr.stationId =
        mode == RailMode::HeavyRail
            ? "YUL"
            : mode == RailMode::LightRail ? "100" : "940GZZLUVIC";
    config.mtr.directionId =
        mode == RailMode::HeavyRail
            ? "UP"
            : mode == RailMode::LightRail ? "920" : "outbound";
    return config;
}

WidgetConfig gmbConfig() {
    WidgetConfig config;
    config.type = WidgetType::GmbEta;
    config.gmb.region = "HKI";
    config.gmb.routeCode = "69";
    config.gmb.routeId = "2000410";
    config.gmb.routeSeq = "1";
    config.gmb.stopId = "20003337";
    config.gmb.stopSeq = "1";
    return config;
}

WidgetConfig journeyConfig() {
    WidgetConfig config;
    config.type = WidgetType::JourneyTime;
    config.journeyTime.locationId = "H1";
    config.journeyTime.destinationId = "K1";
    return config;
}

void assertForwarded(const ProviderCall& call, ProviderKind kind, uint8_t slot,
                     const WidgetConfig& config, int64_t nowEpoch) {
    assert(call.kind == kind);
    assert(call.slot == slot);
    assert(call.config == &config);
    assert(call.nowEpoch == nowEpoch);
}

void assertInvalid(const ProviderResult& result, uint8_t slot, WidgetType type) {
    assert(result.outcome == ProviderOutcome::InvalidConfig);
    assert(result.snapshot.slot == slot);
    assert(result.snapshot.type == type);
    assert(result.snapshot.state == WidgetState::Error);
    assert(result.snapshot.providerMessage == "設定不完整");
    assert(result.snapshot.valueCount == 0);
}

}  // namespace

BusProvider::BusProvider(KmbClient& kmb, CitybusClient& citybus,
                         TflClient& tfl)
    : kmb_(kmb), citybus_(citybus), tfl_(tfl) {}

ProviderResult BusProvider::fetch(uint8_t slot, const WidgetConfig& config, int64_t nowEpoch) {
    (void)kmb_;
    (void)citybus_;
    (void)tfl_;
    calls.push_back({ProviderKind::Bus, slot, &config, nowEpoch});
    return markerResult(ProviderKind::Bus, slot, config.type, nowEpoch);
}

GmbProvider::GmbProvider(GmbClient& client) : client_(client) {}

ProviderResult GmbProvider::fetch(uint8_t slot, const WidgetConfig& config,
                                  int64_t nowEpoch) {
    (void)client_;
    calls.push_back({ProviderKind::Gmb, slot, &config, nowEpoch});
    return markerResult(ProviderKind::Gmb, slot, config.type, nowEpoch);
}

MtrProvider::MtrProvider(MtrClient& client) : client_(client) {}

ProviderResult MtrProvider::fetch(uint8_t slot, const WidgetConfig& config, int64_t nowEpoch) {
    (void)client_;
    calls.push_back({ProviderKind::Mtr, slot, &config, nowEpoch});
    return markerResult(ProviderKind::Mtr, slot, config.type, nowEpoch);
}

LightRailProvider::LightRailProvider(LightRailClient& client) : client_(client) {}

ProviderResult LightRailProvider::fetch(uint8_t slot, const WidgetConfig& config,
                                        int64_t nowEpoch) {
    (void)client_;
    calls.push_back({ProviderKind::LightRail, slot, &config, nowEpoch});
    return markerResult(ProviderKind::LightRail, slot, config.type, nowEpoch);
}

TflRailProvider::TflRailProvider(TflClient& client) : client_(client) {}

ProviderResult TflRailProvider::fetch(uint8_t slot,
                                      const WidgetConfig& config,
                                      int64_t nowEpoch) {
    (void)client_;
    calls.push_back({ProviderKind::TflRail, slot, &config, nowEpoch});
    return markerResult(ProviderKind::TflRail, slot, config.type, nowEpoch);
}

JourneyTimeProvider::JourneyTimeProvider(JourneyTimeClient& client) : client_(client) {}

ProviderResult JourneyTimeProvider::fetch(uint8_t slot, const WidgetConfig& config,
                                          int64_t nowEpoch) {
    (void)client_;
    calls.push_back({ProviderKind::JourneyTime, slot, &config, nowEpoch});
    return markerResult(ProviderKind::JourneyTime, slot, config.type, nowEpoch);
}

int main() {
    KmbClient kmb;
    CitybusClient citybus;
    TflClient tfl;
    GmbClient gmbClient;
    MtrClient mtrClient;
    LightRailClient lightRailClient;
    JourneyTimeClient journeyClient;
    BusProvider bus(kmb, citybus, tfl);
    GmbProvider gmb(gmbClient);
    MtrProvider mtr(mtrClient);
    LightRailProvider lightRail(lightRailClient);
    TflRailProvider tflRail(tfl);
    JourneyTimeProvider journey(journeyClient);
    WidgetProviderRouter router(bus, gmb, mtr, lightRail, tflRail, journey);

    {
        const WidgetConfig config = busConfig();
        const auto result = router.fetch(0, config, 1700000001);
        assert(result.snapshot.title == "0");
        assert(calls.size() == 1);
        assertForwarded(calls.back(), ProviderKind::Bus, 0, config, 1700000001);
    }
    {
        const WidgetConfig config = gmbConfig();
        const auto result = router.fetch(1, config, 1700000002);
        assert(result.snapshot.title == "1");
        assert(calls.size() == 2);
        assertForwarded(calls.back(), ProviderKind::Gmb, 1, config, 1700000002);
    }
    {
        const WidgetConfig config = railConfig(RailMode::HeavyRail);
        const auto result = router.fetch(2, config, 1700000003);
        assert(result.snapshot.title == "2");
        assert(calls.size() == 3);
        assertForwarded(calls.back(), ProviderKind::Mtr, 2, config, 1700000003);
    }
    {
        const WidgetConfig config = railConfig(RailMode::LightRail);
        const auto result = router.fetch(3, config, 1700000004);
        assert(result.snapshot.title == "3");
        assert(calls.size() == 4);
        assertForwarded(calls.back(), ProviderKind::LightRail, 3, config, 1700000004);
    }
    {
        const WidgetConfig config = railConfig(RailMode::LondonRail);
        const auto result = router.fetch(0, config, 1700000005);
        assert(result.snapshot.title == "4");
        assert(calls.size() == 5);
        assertForwarded(calls.back(), ProviderKind::TflRail, 0, config,
                        1700000005);
    }
    {
        const WidgetConfig config = journeyConfig();
        const auto result = router.fetch(0, config, 1700000006);
        assert(result.snapshot.title == "5");
        assert(calls.size() == 6);
        assertForwarded(calls.back(), ProviderKind::JourneyTime, 0, config,
                        1700000006);
    }

    {
        WidgetConfig config;
        const std::size_t before = calls.size();
        const auto result = router.fetch(2, config, 1700000010);
        assert(result.outcome == ProviderOutcome::Empty);
        assert(result.snapshot.slot == 2);
        assert(result.snapshot.type == WidgetType::Disabled);
        assert(result.snapshot.state == WidgetState::Empty);
        assert(result.snapshot.valueCount == 0);
        assert(calls.size() == before);
    }
    {
        WidgetConfig config = busConfig();
        config.bus.stopId.clear();
        const std::size_t before = calls.size();
        assertInvalid(router.fetch(0, config, 1700000011), 0, WidgetType::BusEta);
        assert(calls.size() == before);
    }
    {
        WidgetConfig config = railConfig(RailMode::HeavyRail);
        config.mtr.mode = static_cast<RailMode>(255);
        const std::size_t before = calls.size();
        assertInvalid(router.fetch(1, config, 1700000012), 1, WidgetType::MtrEta);
        assert(calls.size() == before);
    }
    {
        WidgetConfig config = journeyConfig();
        const std::size_t before = calls.size();
        assertInvalid(router.fetch(transitink::kWidgetSlotCount, config,
                                   1700000013),
                      static_cast<uint8_t>(transitink::kWidgetSlotCount),
                      WidgetType::JourneyTime);
        assert(calls.size() == before);
    }
    {
        WidgetConfig config;
        config.type = static_cast<WidgetType>(255);
        const std::size_t before = calls.size();
        assertInvalid(router.fetch(0, config, 1700000014), 0, config.type);
        assert(calls.size() == before);
    }

    {
        calls.clear();
        transitink::WidgetSlots configs{};
        configs[0] = busConfig();
        configs[1] = gmbConfig();
        configs[2] = railConfig(RailMode::HeavyRail);
        configs[3] = journeyConfig();
        transitink::WidgetScheduler scheduler(router);
        scheduler.configure(configs, 500);

        const std::array<ProviderKind, 4> expectedKinds = {
            ProviderKind::Bus, ProviderKind::Gmb, ProviderKind::Mtr,
            ProviderKind::JourneyTime};
        for (uint8_t slot = 0; slot < 4; ++slot) {
            const std::size_t before = calls.size();
            const auto tick = scheduler.serviceNextDue(500, 1700000100 + slot);
            assert(tick.ran && tick.success && tick.slot == slot);
            assert(calls.size() == before + 1);
            assert(calls.back().kind == expectedKinds[slot]);
            assert(calls.back().slot == slot);
            assert(calls.back().config->type == configs[slot].type);
            assert(calls.back().nowEpoch == 1700000100 + slot);
        }
        assert(!scheduler.serviceNextDue(500, 1700000200).ran);
        assert(calls.size() == 4);

        scheduler.forceAllDue(600);
        const auto rotated = scheduler.serviceNextDue(600, 1700000201);
        assert(rotated.ran && rotated.slot == 0);
        assert(calls.size() == 5);
        assert(calls.back().kind == ProviderKind::Bus);
    }

    return 0;
}
