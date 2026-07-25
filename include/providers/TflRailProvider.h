#pragma once

#include "TflClient.h"
#include "core/WidgetCore.h"

class TflRailProvider {
public:
    explicit TflRailProvider(TflClient& client);

    transitink::ProviderResult fetch(uint8_t slot,
                                     const transitink::WidgetConfig& config,
                                     int64_t nowEpoch);

private:
    TflClient& client_;
};
