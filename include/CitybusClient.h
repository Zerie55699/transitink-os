#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "core/WidgetCore.h"
#include "core/CatalogCore.h"

class CitybusClient {
public:
    bool fetchRoutesJson(String& body, String& error);
    bool fetchRoutesToFile(fs::FS& fs, const char* path, String& error);
    bool fetchStopsToFile(fs::FS& fs, const char* path, String& error);
    bool fetchStopJson(const String& stopId, String& body, String& error);
    bool fetchRouteStopsJson(const String& route,
                             const String& direction,
                             String& body,
                             String& error);
    bool fetchStopLabels(const std::vector<std::string>& stopIds,
                         std::vector<transitink::BusStopLabel>& labels,
                         String& error);
    bool fetchEtaRecords(const transitink::BusWidgetConfig& config,
                         std::vector<transitink::BusEtaRecord>& records,
                         String& error);

private:
    bool httpGet(const String& url, String& body, String& error);
    bool httpGetBounded(const String& url,
                        std::size_t limit,
                        String& body,
                        String& error);
    bool httpGetToFile(const String& url, fs::FS& fs, const char* path, String& error);
};
