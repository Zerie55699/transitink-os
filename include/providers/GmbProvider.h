#pragma once

#include <cstdint>

#include "GmbClient.h"
#include "core/WidgetCore.h"

class GmbProvider {
public:
    explicit GmbProvider(GmbClient& client);

    transitink::ProviderResult fetch(uint8_t slot,
                                     const transitink::WidgetConfig& config,
                                     int64_t nowEpoch);

private:
    GmbClient& client_;
};
