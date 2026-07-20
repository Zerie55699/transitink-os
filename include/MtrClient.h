#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

#include "core/WidgetCore.h"

class MtrClient {
public:
    static std::string requestUrl(const transitink::MtrWidgetConfig& config) {
        return "https://rt.data.gov.hk/v1/transport/mtr/getSchedule.php?line=" +
               config.lineOrRouteId + "&sta=" + config.stationId + "&lang=TC";
    }

    bool fetchArrivals(const transitink::MtrWidgetConfig& config,
                       std::vector<transitink::RailArrivalRecord>& records,
                       int64_t& dataEpoch,
                       String& error);
};
