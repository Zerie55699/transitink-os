#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

#include "core/WidgetCore.h"

class LightRailClient {
public:
    static std::string requestUrl(const transitink::MtrWidgetConfig& config) {
        return "https://rt.data.gov.hk/v1/transport/mtr/lrt/getSchedule?station_id=" +
               config.stationId + "&with_special=1";
    }

    bool fetchArrivals(const transitink::MtrWidgetConfig& config,
                       int64_t nowEpoch,
                       std::vector<transitink::RailArrivalRecord>& records,
                       int64_t& dataEpoch,
                       String& error);
};
