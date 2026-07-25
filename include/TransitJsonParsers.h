#pragma once

#include <string>
#include <vector>

#include "core/CatalogCore.h"
#include "core/StaticCatalogCore.h"
#include "core/WidgetCore.h"

namespace transitink {

struct GmbCatalogDirection {
    std::string region;
    std::string routeId;
    std::string routeSeq;
    std::string originLabelTc;
    std::string destinationLabelTc;
    std::string descriptionTc;
    std::string originLabelEn{};
    std::string destinationLabelEn{};
    std::string descriptionEn{};
};

struct GmbCatalogStop {
    std::string stopId;
    std::string stopSeq;
    std::string labelTc;
    std::string labelEn{};
};

}  // namespace transitink

bool isOfficialBusIdentifier(const std::string& value);
bool isOfficialTflIdentifier(const std::string& value);
bool mapCitybusDirectionPath(const std::string& direction, std::string& path);

bool parseCitybusEtaJson(const char* json,
                         const transitink::BusWidgetConfig& config,
                         std::vector<transitink::BusEtaRecord>& records,
                         std::string& error);

bool parseKmbEtaJson(const char* json,
                     const transitink::BusWidgetConfig& config,
                     std::vector<transitink::BusEtaRecord>& records,
                     std::string& error);

bool parseTflDirectionsJson(
    const char* json,
    const std::string& route,
    std::vector<transitink::BusCatalogRoute>& directions,
    std::string& error);

bool parseTflRouteSequenceJson(
    const char* json,
    const std::string& route,
    const std::string& direction,
    const std::string& serviceType,
    std::vector<transitink::BusCatalogStop>& stops,
    std::string& error);

bool parseTflEtaJson(const char* json,
                     const transitink::BusWidgetConfig& config,
                     int64_t nowEpoch,
                     std::vector<transitink::BusEtaRecord>& records,
                     std::string& error);

bool parseTflRailLinesJson(
    const char* json,
    std::vector<transitink::StaticCatalogEntry>& lines,
    std::string& error);

bool parseTflRailStationsJson(
    const char* json,
    std::vector<transitink::StaticCatalogEntry>& stations,
    std::string& error);

bool parseTflRailArrivalsJson(
    const char* json,
    const transitink::MtrWidgetConfig& config,
    int64_t nowEpoch,
    std::vector<transitink::RailArrivalRecord>& records,
    std::string& error);

bool parseMtrNextTrainJson(const char* json,
                           const transitink::MtrWidgetConfig& config,
                           std::vector<transitink::RailArrivalRecord>& records,
                           int64_t& dataEpoch,
                           std::string& error);

bool parseLightRailJson(const char* json,
                        const transitink::MtrWidgetConfig& config,
                        int64_t nowEpoch,
                        std::vector<transitink::RailArrivalRecord>& records,
                        int64_t& dataEpoch,
                        std::string& error);

bool parseGmbRouteCodesJson(const char* json,
                            const std::string& region,
                            std::vector<std::string>& routeCodes,
                            std::string& error);

bool parseGmbDirectionsJson(
    const char* json,
    const std::string& region,
    const std::string& routeCode,
    std::vector<transitink::GmbCatalogDirection>& directions,
    std::string& error);

bool parseGmbStopsJson(const char* json,
                       const std::string& routeId,
                       const std::string& routeSeq,
                       std::vector<transitink::GmbCatalogStop>& stops,
                       std::string& error);

bool parseGmbEtaJson(const char* json,
                     const transitink::GmbWidgetConfig& config,
                     transitink::GmbEtaPayload& payload,
                     std::string& error);
