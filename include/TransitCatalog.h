#pragma once

#include <cstddef>
#include <string>

#include "core/WidgetConfigCore.h"

namespace transitink {

struct TransitCatalogItem {
    const char* id;
    const char* labelTc;
    const char* labelEn = "";
};

struct TransitCatalogGroup {
    const char* id;
    const char* labelTc;
    const TransitCatalogItem* stations;
    std::size_t stationCount;
    const TransitCatalogItem* directions;
    std::size_t directionCount;
    const char* labelEn = "";
};

struct TransitCatalogView {
    const TransitCatalogGroup* groups;
    std::size_t groupCount;
};

struct JourneyTimeCatalogPair {
    const char* locationId;
    const char* destinationId;
};

struct JourneyTimeCatalogView {
    const TransitCatalogItem* locations;
    std::size_t locationCount;
    const TransitCatalogItem* destinations;
    std::size_t destinationCount;
    const JourneyTimeCatalogPair* pairs;
    std::size_t pairCount;
};

TransitCatalogView heavyRailCatalog();
TransitCatalogView lightRailCatalog();
JourneyTimeCatalogView journeyTimeCatalog();
const TransitCatalogItem* findJourneyTimeLocation(const std::string& locationId);
const TransitCatalogItem* findJourneyTimeDestination(
    const std::string& destinationId);
bool isJourneyTimePairValid(const std::string& locationId,
                            const std::string& destinationId);
const TransitCatalogGroup* findTransitCatalogGroup(RailMode mode,
                                                   const std::string& groupId);
const TransitCatalogItem* findTransitCatalogStation(RailMode mode,
                                                    const std::string& groupId,
                                                    const std::string& stationId);
const TransitCatalogItem* findTransitCatalogDirection(
    RailMode mode, const std::string& groupId, const std::string& directionId);
bool lightRailDirectionIdForDestination(const std::string& routeId,
                                        const std::string& destinationText,
                                        std::string& directionId);

}  // namespace transitink
