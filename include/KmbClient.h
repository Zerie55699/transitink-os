#pragma once

#include <Arduino.h>
#include <FS.h>
#include <vector>

#include "core/BusEtaCore.h"
#include "core/WidgetCore.h"

class KmbClient {
public:
    bool fetchRoutesJson(String& body, String& error);
    bool fetchRoutesToFile(fs::FS& fs, const char* path, String& error);
    bool fetchStopsToFile(fs::FS& fs, const char* path, String& error);
    bool fetchRouteJson(const String& route, const String& bound, const String& serviceType, String& body, String& error);
    bool fetchStopsJson(String& body, String& error);
    bool fetchStopJson(const String& stopId, String& body, String& error);
    bool fetchRouteStopsJson(const String& route, const String& bound, const String& serviceType, String& body, String& error);
    bool fetchEtaRecords(const bus_eta::RouteSelection& selection, std::vector<bus_eta::EtaRecord>& records, String& error);
    bool fetchEtaRecords(const transitink::BusWidgetConfig& config,
                         std::vector<transitink::BusEtaRecord>& records,
                         String& error);

private:
    bool httpGet(const String& url, String& body, String& error);
    bool httpGetBounded(const String& url,
                        std::size_t limit,
                        String& body,
                        String& error);
};
