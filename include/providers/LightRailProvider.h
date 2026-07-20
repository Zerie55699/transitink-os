#pragma once

#include <cstdint>

#include "LightRailClient.h"
#include "core/WidgetCore.h"

class LightRailProvider {
public:
    explicit LightRailProvider(LightRailClient& client);

    transitink::ProviderResult fetch(uint8_t slot,
                                     const transitink::WidgetConfig& config,
                                     int64_t nowEpoch);

private:
    LightRailClient& client_;
};
