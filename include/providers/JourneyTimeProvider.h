#pragma once

#include <cstdint>

#include "JourneyTimeClient.h"
#include "core/WidgetCore.h"

class JourneyTimeProvider {
public:
    explicit JourneyTimeProvider(JourneyTimeClient& client);

    transitink::ProviderResult fetch(uint8_t slot,
                                     const transitink::WidgetConfig& config,
                                     int64_t nowEpoch);

private:
    JourneyTimeClient& client_;
};
