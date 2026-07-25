#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/WidgetCore.h"

namespace transitink {

struct WidgetTickResult {
    bool ran = false;
    uint8_t slot = 0;
    bool success = false;
};

class IWidgetProviderRouter {
public:
    virtual ~IWidgetProviderRouter() = default;
    virtual ProviderResult fetch(uint8_t slot, const WidgetConfig& config, int64_t nowEpoch) = 0;
};

class WidgetScheduler {
public:
    explicit WidgetScheduler(IWidgetProviderRouter& router);
    void configure(const WidgetSlots& configs, uint32_t nowMs);
    void forceAllDue(uint32_t nowMs);
    void forceActivePageDue(uint32_t nowMs);
    bool setActivePage(std::size_t page, uint32_t nowMs);
    std::size_t activePage() const;
    WidgetTickResult serviceNextDue(uint32_t nowMs, int64_t nowEpoch);
    bool hasPendingDue(uint32_t nowMs) const;
    bool hasEnabledWidgets() const;
    WidgetSnapshotSet displaySnapshots(int64_t nowEpoch) const;
    WidgetPageSnapshotSet pageSwitchSnapshots(
        int64_t nowEpoch, uint32_t cacheTtlSeconds) const;
    const WidgetSnapshot& snapshot(std::size_t slot) const;

private:
    IWidgetProviderRouter& router_;
    WidgetSlots configs_{};
    WidgetSnapshotSet snapshots_{};
    std::array<uint32_t, kWidgetSlotCount> nextDueMs_{};
    std::size_t roundRobinCursor_ = 0;
    std::size_t activePage_ = 0;
};

}  // namespace transitink
