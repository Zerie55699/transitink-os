#pragma once

#include <cstdint>

#include "MtrClient.h"
#include "core/WidgetCore.h"

class MtrProvider {
public:
    explicit MtrProvider(MtrClient& client);

    transitink::ProviderResult fetch(uint8_t slot,
                                     const transitink::WidgetConfig& config,
                                     int64_t nowEpoch);

private:
    MtrClient& client_;
};
