#pragma once

#include <Arduino.h>

#include <vector>

#include "core/CatalogCore.h"
#include "core/StaticCatalogCore.h"
#include "core/WidgetCore.h"

class TflClient {
public:
    bool fetchDirections(
        const String& route,
        std::vector<transitink::BusCatalogRoute>& directions,
        String& error);
    bool fetchStops(const String& route,
                    const String& direction,
                    const String& serviceType,
                    std::vector<transitink::BusCatalogStop>& stops,
                    String& error);
    bool fetchEtaRecords(const transitink::BusWidgetConfig& config,
                         int64_t nowEpoch,
                         std::vector<transitink::BusEtaRecord>& records,
                         String& error);
    bool fetchRailLines(
        std::vector<transitink::StaticCatalogEntry>& lines,
        String& error);
    bool fetchRailStations(
        const String& line,
        std::vector<transitink::StaticCatalogEntry>& stations,
        String& error);
    bool fetchRailArrivals(
        const transitink::MtrWidgetConfig& config,
        int64_t nowEpoch,
        std::vector<transitink::RailArrivalRecord>& records,
        String& error);

private:
    bool httpGetBounded(const String& url,
                        std::size_t limit,
                        String& body,
                        String& error);
    bool fetchFilteredRouteSequence(const String& url,
                                    String& body,
                                    String& error);
    bool fetchFilteredRailStations(const String& url,
                                   String& body,
                                   String& error);
};
