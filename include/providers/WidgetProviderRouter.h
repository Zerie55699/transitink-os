#pragma once

#include <cstdint>

#include "core/WidgetScheduler.h"

class BusProvider;
class GmbProvider;
class JourneyTimeProvider;
class LightRailProvider;
class MtrProvider;
class TflRailProvider;

class WidgetProviderRouter final : public transitink::IWidgetProviderRouter {
public:
    WidgetProviderRouter(BusProvider& bus,
                         GmbProvider& gmb,
                         MtrProvider& mtr,
                         LightRailProvider& lightRail,
                         TflRailProvider& tflRail,
                         JourneyTimeProvider& journey);

    transitink::ProviderResult fetch(uint8_t slot,
                                     const transitink::WidgetConfig& config,
                                     int64_t nowEpoch) override;

private:
    BusProvider& bus_;
    GmbProvider& gmb_;
    MtrProvider& mtr_;
    LightRailProvider& lightRail_;
    TflRailProvider& tflRail_;
    JourneyTimeProvider& journey_;
};
