#include "providers/WidgetProviderRouter.h"

#include "providers/BusProvider.h"
#include "providers/GmbProvider.h"
#include "providers/JourneyTimeProvider.h"
#include "providers/LightRailProvider.h"
#include "providers/MtrProvider.h"

namespace {

transitink::ProviderResult invalidResult(uint8_t slot,
                                        const transitink::WidgetConfig& config,
                                        int64_t nowEpoch) {
    transitink::WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = config.type;
    snapshot.state = transitink::WidgetState::Error;
    snapshot.providerMessage = "設定不完整";
    snapshot.fetchedAtEpoch = nowEpoch;
    return {transitink::ProviderOutcome::InvalidConfig, snapshot};
}

transitink::ProviderResult disabledResult(uint8_t slot) {
    transitink::WidgetSnapshot snapshot;
    snapshot.slot = slot;
    snapshot.type = transitink::WidgetType::Disabled;
    snapshot.state = transitink::WidgetState::Empty;
    return {transitink::ProviderOutcome::Empty, snapshot};
}

}  // namespace

WidgetProviderRouter::WidgetProviderRouter(BusProvider& bus,
                                           GmbProvider& gmb,
                                           MtrProvider& mtr,
                                           LightRailProvider& lightRail,
                                           JourneyTimeProvider& journey)
    : bus_(bus), gmb_(gmb), mtr_(mtr), lightRail_(lightRail), journey_(journey) {}

transitink::ProviderResult WidgetProviderRouter::fetch(
    uint8_t slot, const transitink::WidgetConfig& config, int64_t nowEpoch) {
    if (slot >= transitink::kWidgetSlotCount || !transitink::isWidgetConfigValid(config)) {
        return invalidResult(slot, config, nowEpoch);
    }

    switch (config.type) {
        case transitink::WidgetType::Disabled:
            return disabledResult(slot);
        case transitink::WidgetType::BusEta:
            return bus_.fetch(slot, config, nowEpoch);
        case transitink::WidgetType::GmbEta:
            return gmb_.fetch(slot, config, nowEpoch);
        case transitink::WidgetType::MtrEta:
            switch (config.mtr.mode) {
                case transitink::RailMode::HeavyRail:
                    return mtr_.fetch(slot, config, nowEpoch);
                case transitink::RailMode::LightRail:
                    return lightRail_.fetch(slot, config, nowEpoch);
            }
            break;
        case transitink::WidgetType::JourneyTime:
            return journey_.fetch(slot, config, nowEpoch);
    }
    return invalidResult(slot, config, nowEpoch);
}
