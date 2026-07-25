#pragma once

#include <cstdint>

#include "CitybusClient.h"
#include "KmbClient.h"
#include "TflClient.h"
#include "core/WidgetCore.h"

class BusProvider {
public:
    BusProvider(KmbClient& kmb, CitybusClient& citybus, TflClient& tfl);

    transitink::ProviderResult fetch(uint8_t slot,
                                     const transitink::WidgetConfig& config,
                                     int64_t nowEpoch);

private:
    KmbClient& kmb_;
    CitybusClient& citybus_;
    TflClient& tfl_;
};
