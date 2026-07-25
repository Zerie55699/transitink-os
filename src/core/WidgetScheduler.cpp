#include "core/WidgetScheduler.h"

#include <limits>
#include <utility>

#include "core/UiText.h"

namespace transitink {
namespace {

void clearValues(WidgetSnapshot& snapshot) {
    snapshot.values = {};
    snapshot.valueCount = 0;
}

uint8_t nextFailureCount(uint8_t current) {
    if (current == std::numeric_limits<uint8_t>::max()) return current;
    return static_cast<uint8_t>(current + 1);
}

bool staleWindowElapsed(const WidgetSnapshot& snapshot,
                        const WidgetConfig& config,
                        int64_t nowEpoch) {
    if (snapshot.fetchedAtEpoch <= 0 || nowEpoch < snapshot.fetchedAtEpoch) return false;
    return nowEpoch - snapshot.fetchedAtEpoch >=
           static_cast<int64_t>(staleWindowSeconds(config));
}

bool pageSwitchCacheUsable(const WidgetSnapshot& cached,
                           const WidgetSnapshot& display,
                           int64_t nowEpoch,
                           uint32_t cacheTtlSeconds) {
    if (cached.type == WidgetType::Disabled) return true;
    if (cacheTtlSeconds == 0 || cached.fetchedAtEpoch <= 0 ||
        nowEpoch <= 0 || nowEpoch < cached.fetchedAtEpoch) {
        return false;
    }
    if (nowEpoch - cached.fetchedAtEpoch >=
        static_cast<int64_t>(cacheTtlSeconds)) {
        return false;
    }
    // Expired arrival times do not prove the provider currently has no service.
    return cached.state != WidgetState::Ready ||
           cached.valueCount == 0 || display.valueCount > 0;
}

void applyProviderError(WidgetSnapshot& snapshot,
                        uint8_t slot,
                        WidgetType type,
                        int64_t nowEpoch,
                        const WidgetSnapshot& providerSnapshot,
                        const char* message) {
    snapshot = providerSnapshot;
    snapshot.slot = slot;
    snapshot.type = type;
    clearValues(snapshot);
    snapshot.state = WidgetState::Error;
    snapshot.providerMessage = message;
    snapshot.freshness = Freshness::Fresh;
    snapshot.consecutiveFailures = 0;
    if (snapshot.fetchedAtEpoch == 0 && nowEpoch > 0) snapshot.fetchedAtEpoch = nowEpoch;
}

}  // namespace

WidgetScheduler::WidgetScheduler(IWidgetProviderRouter& router) : router_(router) {}

void WidgetScheduler::configure(const WidgetSlots& configs, uint32_t nowMs) {
    configs_ = configs;
    roundRobinCursor_ = 0;
    activePage_ = firstEnabledWidgetPage(configs_);
    for (std::size_t index = 0; index < kWidgetSlotCount; ++index) {
        snapshots_[index] = configuredWidgetSnapshot(
            static_cast<uint8_t>(index), configs_[index]);
        nextDueMs_[index] = configs_[index].type == WidgetType::Disabled ? 0 : nowMs;
    }
}

void WidgetScheduler::forceAllDue(uint32_t nowMs) {
    for (std::size_t index = 0; index < kWidgetSlotCount; ++index) {
        if (configs_[index].type != WidgetType::Disabled) nextDueMs_[index] = nowMs;
    }
}

void WidgetScheduler::forceActivePageDue(uint32_t nowMs) {
    const std::size_t start = widgetPageStart(activePage_);
    for (std::size_t offset = 0; offset < kWidgetsPerPage; ++offset) {
        const std::size_t index = start + offset;
        if (configs_[index].type != WidgetType::Disabled) nextDueMs_[index] = nowMs;
    }
}

bool WidgetScheduler::setActivePage(std::size_t page, uint32_t nowMs) {
    if (page >= kWidgetPageCount || !widgetPageHasEnabled(configs_, page)) return false;
    activePage_ = page;
    roundRobinCursor_ = 0;
    forceActivePageDue(nowMs);
    return true;
}

std::size_t WidgetScheduler::activePage() const {
    return activePage_;
}

WidgetTickResult WidgetScheduler::serviceNextDue(uint32_t nowMs, int64_t nowEpoch) {
    const std::size_t start = widgetPageStart(activePage_);
    for (std::size_t offset = 0; offset < kWidgetsPerPage; ++offset) {
        const std::size_t lane = (roundRobinCursor_ + offset) % kWidgetsPerPage;
        const std::size_t index = start + lane;
        const auto type = configs_[index].type;
        if (type == WidgetType::Disabled || !deadlineReached(nowMs, nextDueMs_[index])) {
            continue;
        }

        nextDueMs_[index] = nowMs + refreshIntervalMs(configs_[index]);
        roundRobinCursor_ = (lane + 1) % kWidgetsPerPage;
        const uint8_t slot = static_cast<uint8_t>(index);
        const ProviderResult result = router_.fetch(slot, configs_[index], nowEpoch);
        WidgetTickResult tick{true, slot, false};

        switch (result.outcome) {
            case ProviderOutcome::Success:
                snapshots_[index] = result.snapshot;
                snapshots_[index].slot = slot;
                snapshots_[index].type = type;
                snapshots_[index].freshness = Freshness::Fresh;
                snapshots_[index].consecutiveFailures = 0;
                if (snapshots_[index].fetchedAtEpoch == 0 && nowEpoch > 0) {
                    snapshots_[index].fetchedAtEpoch = nowEpoch;
                }
                tick.success = true;
                break;
            case ProviderOutcome::Empty:
                snapshots_[index] = result.snapshot;
                snapshots_[index].slot = slot;
                snapshots_[index].type = type;
                clearValues(snapshots_[index]);
                snapshots_[index].state = WidgetState::Empty;
                if (snapshots_[index].providerMessage.empty()) {
                    snapshots_[index].providerMessage =
                        uiText(UiTextId::NoArrivals);
                }
                snapshots_[index].freshness = Freshness::Fresh;
                snapshots_[index].consecutiveFailures = 0;
                if (snapshots_[index].fetchedAtEpoch == 0 && nowEpoch > 0) {
                    snapshots_[index].fetchedAtEpoch = nowEpoch;
                }
                tick.success = true;
                break;
            case ProviderOutcome::InvalidConfig:
                applyProviderError(snapshots_[index], slot, type, nowEpoch, result.snapshot,
                                   uiText(UiTextId::InvalidConfig));
                break;
            case ProviderOutcome::ClockUnsynced:
                applyProviderError(snapshots_[index], slot, type, nowEpoch, result.snapshot,
                                   uiText(UiTextId::ClockUnsynced));
                break;
            case ProviderOutcome::Failure: {
                auto& snapshot = snapshots_[index];
                const uint8_t failures = nextFailureCount(snapshot.consecutiveFailures);
                const bool hasLastSuccess =
                    snapshot.fetchedAtEpoch > 0 &&
                    (snapshot.state != WidgetState::Error || snapshot.freshness == Freshness::Stale);
                if (!hasLastSuccess) {
                    snapshot = configuredWidgetSnapshot(
                        slot, configs_[index]);
                    snapshot.state = WidgetState::Error;
                    snapshot.providerMessage =
                        uiText(UiTextId::DataUnavailable);
                    snapshot.freshness = Freshness::Stale;
                    snapshot.consecutiveFailures = failures;
                    break;
                }

                snapshot.freshness = Freshness::Stale;
                snapshot.consecutiveFailures = failures;
                snapshot.providerMessage = uiText(UiTextId::DataUnavailable);
                removeExpiredValues(snapshot, nowEpoch);
                if (staleWindowElapsed(snapshot, configs_[index], nowEpoch)) {
                    clearValues(snapshot);
                    snapshot.state = WidgetState::Error;
                    snapshot.providerMessage = uiText(UiTextId::DataExpired);
                } else if (snapshot.valueCount == 0) {
                    snapshot.state = WidgetState::Error;
                }
                break;
            }
        }
        return tick;
    }
    return {};
}

bool WidgetScheduler::hasPendingDue(uint32_t nowMs) const {
    const std::size_t start = widgetPageStart(activePage_);
    for (std::size_t offset = 0; offset < kWidgetsPerPage; ++offset) {
        const std::size_t index = start + offset;
        if (configs_[index].type != WidgetType::Disabled &&
            deadlineReached(nowMs, nextDueMs_[index])) {
            return true;
        }
    }
    return false;
}

bool WidgetScheduler::hasEnabledWidgets() const {
    for (const auto& config : configs_) {
        if (config.type != WidgetType::Disabled) return true;
    }
    return false;
}

WidgetSnapshotSet WidgetScheduler::displaySnapshots(int64_t nowEpoch) const {
    WidgetSnapshotSet display = snapshots_;
    for (std::size_t index = 0; index < kWidgetSlotCount; ++index) {
        auto& snapshot = display[index];
        if (configs_[index].type == WidgetType::Disabled) continue;

        const std::size_t previousValueCount = snapshot.valueCount;
        removeExpiredValues(snapshot, nowEpoch);
        if (snapshot.freshness == Freshness::Stale &&
            staleWindowElapsed(snapshot, configs_[index], nowEpoch)) {
            clearValues(snapshot);
            snapshot.state = WidgetState::Error;
            snapshot.providerMessage = uiText(UiTextId::DataExpired);
        } else if (previousValueCount > 0 && snapshot.valueCount == 0) {
            if (snapshot.freshness == Freshness::Stale) {
                snapshot.state = WidgetState::Error;
                snapshot.providerMessage = uiText(UiTextId::DataUnavailable);
            } else {
                snapshot.state = WidgetState::Empty;
                snapshot.providerMessage = uiText(UiTextId::NoArrivals);
            }
        }
    }
    return display;
}

WidgetPageSnapshotSet WidgetScheduler::pageSwitchSnapshots(
    int64_t nowEpoch, uint32_t cacheTtlSeconds) const {
    const WidgetSnapshotSet display = displaySnapshots(nowEpoch);
    WidgetPageSnapshotSet page =
        snapshotsForWidgetPage(display, activePage_);
    const std::size_t start = widgetPageStart(activePage_);
    for (std::size_t lane = 0; lane < kWidgetsPerPage; ++lane) {
        const WidgetSnapshot& cached = snapshots_[start + lane];
        WidgetSnapshot& snapshot = page[lane];
        if (pageSwitchCacheUsable(
                cached, snapshot, nowEpoch, cacheTtlSeconds)) {
            continue;
        }
        clearValues(snapshot);
        snapshot.state = WidgetState::Empty;
        snapshot.providerMessage = uiText(UiTextId::Updating);
        snapshot.fetchedAtEpoch = 0;
        snapshot.dataAtEpoch = 0;
        snapshot.freshness = Freshness::Fresh;
        snapshot.consecutiveFailures = 0;
    }
    return page;
}

const WidgetSnapshot& WidgetScheduler::snapshot(std::size_t slot) const {
    return snapshots_.at(slot);
}

}  // namespace transitink
