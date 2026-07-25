#pragma once

#include <Arduino.h>
#include <vector>

#include "CitybusClient.h"
#include "GmbClient.h"
#include "KmbClient.h"
#include "TflClient.h"
#include "core/WidgetConfigCore.h"

bool parseCatalogBusOperator(const String& operatorId, transitink::BusOperator& op);
bool parseCatalogRailMode(const String& modeId, transitink::RailMode& mode);

class WidgetCatalogService {
public:
    WidgetCatalogService(KmbClient& kmb,
                         CitybusClient& citybus,
                         GmbClient& gmb,
                         TflClient& tfl);
    bool begin();
    bool listBusRoutes(transitink::BusOperator op, bool refresh, String& json, String& error);
    bool listBusDirections(transitink::BusOperator op,
                           const String& route,
                           String& json,
                           String& error);
    bool listBusStops(transitink::BusOperator op,
                      const String& route,
                      const String& direction,
                      const String& serviceType,
                      bool refresh,
                      String& json,
                      String& error);
    bool listGmbRoutes(bool refresh, String& json, String& error);
    bool listGmbDirections(const String& routeCode,
                           bool refresh,
                           String& json,
                           String& error);
    bool listGmbStops(const String& routeId,
                      const String& routeSeq,
                      bool refresh,
                      String& json,
                      String& error);
    bool listRailLines(transitink::RailMode mode, String& json, String& error);
    bool listRailStations(transitink::RailMode mode,
                          const String& lineOrRoute,
                          String& json,
                          String& error);
    bool listRailDirections(transitink::RailMode mode,
                            const String& lineOrRoute,
                            const String& station,
                            String& json,
                            String& error);
    bool listJourneyLocations(String& json, String& error);
    bool listJourneyDestinations(const String& locationId, String& json, String& error);
    bool readBusRouteOverride(transitink::BusOperator op,
                              const String& route,
                              String& json,
                              String& error) const;
    bool readGmbRouteOverride(const String& routeCode,
                              String& json,
                              String& error) const;
    bool readUpdatedRouteIndex(String& json, String& error) const;
    bool refreshRouteIndex(String& json, String& error);
    bool refreshBusRoute(transitink::BusOperator op,
                         const String& route,
                         bool refreshRouteList,
                         bool refreshSharedStops,
                         String& json,
                         String& error);
    bool refreshGmbRoute(const String& routeCode, String& json, String& error);
    bool appendStatus(String& configJson, String& error) const;

private:
    enum class CacheKind : uint8_t { KmbRoutes, KmbStops, CitybusRoutes, CitybusStops };

    bool ensureCache(CacheKind kind, bool refresh, String& error);
    bool cacheUsable(CacheKind kind) const;
    bool recoverCacheFiles(const String& dataPath,
                           const String& tempPath,
                           const String& backupPath,
                           String& error) const;
    bool downloadCache(CacheKind kind, const char* path, String& error);
    bool validateCache(CacheKind kind, const char* path, String& error) const;
    void writeCacheMetadata(CacheKind kind);
    bool listCitybusStops(const String& route,
                          const String& direction,
                          const String& serviceType,
                          bool refresh,
                          String& json,
                          String& error);
    bool readCitybusAggregateCache(const String& route,
                                   const String& direction,
                                   const String& serviceType,
                                   bool requireFresh,
                                   std::vector<transitink::BusCatalogStop>& stops,
                                   String& error) const;
    bool writeCitybusAggregateCache(const String& route,
                                    const String& direction,
                                    const String& serviceType,
                                    const std::vector<transitink::BusCatalogStop>& stops,
                                    String& error);
    bool readJsonCache(const String& path, String& json) const;
    bool writeJsonCache(const String& path, const String& json, String& error) const;
    void loadUpdateMetadata();
    void writeUpdateMetadata();

    KmbClient& kmb_;
    CitybusClient& citybus_;
    GmbClient& gmb_;
    TflClient& tfl_;
    bool fsReady_ = false;
    time_t lastUpdatedAt_ = 0;
};
