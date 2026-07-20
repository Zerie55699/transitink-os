#include "core/WidgetScheduler.h"

#include <limits>
#include <utility>

namespace transitink {
namespace {

constexpr const char* kExpiredMessage = "資料已逾期";
constexpr const char* kUnavailableMessage = "暫未能取得資料";
constexpr const char* kInvalidConfigMessage = "設定不完整";
constexpr const char* kEmptyMessage = "暫無班次";
constexpr const char* kClockUnsyncedMessage = "時間尚未同步";

void clearValues(WidgetSnapshot& snapshot) {
    snapshot.values = {};
    snapshot.valueCount = 0;
}

uint8_t nextFailureCount(uint8_t current) {
    if (current == std::numeric_limits<uint8_t>::max()) return current;
    return static_cast<uint8_t>(current + 1);
}

bool staleWindowElapsed(const WidgetSnapshot& snapshot, WidgetType type, int64_t nowEpoch) {
    if (snapshot.fetchedAtEpoch <= 0 || nowEpoch < snapshot.fetchedAtEpoch) return false;
    return nowEpoch - snapshot.fetchedAtEpoch >=
           static_cast<int64_t>(staleWindowSeconds(type));
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

WidgetTickResult WidgetScheduler::serviceNextDue(uint32_t nowMs, int64_t nowEpoch) {
    for (std::size_t offset = 0; offset < kWidgetSlotCount; ++offset) {
        const std::size_t index = (roundRobinCursor_ + offset) % kWidgetSlotCount;
        const auto type = configs_[index].type;
        if (type == WidgetType::Disabled || !deadlineReached(nowMs, nextDueMs_[index])) {
            continue;
        }

        nextDueMs_[index] = nowMs + refreshIntervalMs(type);
        roundRobinCursor_ = (index + 1) % kWidgetSlotCount;
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
                    snapshots_[index].providerMessage = kEmptyMessage;
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
                                   kInvalidConfigMessage);
                break;
            case ProviderOutcome::ClockUnsynced:
                applyProviderError(snapshots_[index], slot, type, nowEpoch, result.snapshot,
                                   kClockUnsyncedMessage);
                break;
            case ProviderOutcome::Failure: {
                auto& snapshot = snapshots_[index];
                const uint8_t failures = nextFailureCount(snapshot.consecutiveFailures);
                const bool hasLastSuccess =
                    snapshot.fetchedAtEpoch > 0 &&
                    (snapshot.state != WidgetState::Error || snapshot.freshness == Freshness::Stale);
                if (!hasLastSuccess) {
                    snapshot = {};
                    snapshot.slot = slot;
                    snapshot.type = type;
                    snapshot.state = WidgetState::Error;
                    snapshot.providerMessage = kUnavailableMessage;
                    snapshot.freshness = Freshness::Stale;
                    snapshot.consecutiveFailures = failures;
                    break;
                }

                snapshot.freshness = Freshness::Stale;
                snapshot.consecutiveFailures = failures;
                snapshot.providerMessage = kUnavailableMessage;
                removeExpiredValues(snapshot, nowEpoch);
                if (staleWindowElapsed(snapshot, type, nowEpoch)) {
                    clearValues(snapshot);
                    snapshot.state = WidgetState::Error;
                    snapshot.providerMessage = kExpiredMessage;
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
    for (std::size_t index = 0; index < kWidgetSlotCount; ++index) {
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
            staleWindowElapsed(snapshot, configs_[index].type, nowEpoch)) {
            clearValues(snapshot);
            snapshot.state = WidgetState::Error;
            snapshot.providerMessage = kExpiredMessage;
        } else if (previousValueCount > 0 && snapshot.valueCount == 0) {
            if (snapshot.freshness == Freshness::Stale) {
                snapshot.state = WidgetState::Error;
                snapshot.providerMessage = kUnavailableMessage;
            } else {
                snapshot.state = WidgetState::Empty;
                snapshot.providerMessage = kEmptyMessage;
            }
        }
    }
    return display;
}

const WidgetSnapshot& WidgetScheduler::snapshot(std::size_t slot) const {
    return snapshots_.at(slot);
}

}  // namespace transitink
