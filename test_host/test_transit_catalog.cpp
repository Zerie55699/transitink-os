#include "LightRailClient.h"
#include "MtrClient.h"
#include "TransitCatalog.h"

#include <cassert>
#include <set>
#include <string>

namespace {

struct CatalogCounts {
    std::size_t memberships = 0;
    std::size_t directions = 0;
    std::set<std::string> uniqueStations;
};

CatalogCounts validateCatalog(transitink::TransitCatalogView catalog) {
    std::set<std::string> groupIds;
    CatalogCounts counts;
    for (std::size_t groupIndex = 0; groupIndex < catalog.groupCount; ++groupIndex) {
        const auto& group = catalog.groups[groupIndex];
        assert(group.id != nullptr && group.id[0] != '\0');
        assert(group.labelTc != nullptr && group.labelTc[0] != '\0');
        assert(groupIds.insert(group.id).second);

        std::set<std::string> stationIds;
        for (std::size_t index = 0; index < group.stationCount; ++index) {
            const auto& station = group.stations[index];
            assert(station.id != nullptr && station.id[0] != '\0');
            assert(station.labelTc != nullptr && station.labelTc[0] != '\0');
            assert(stationIds.insert(station.id).second);
            counts.uniqueStations.insert(station.id);
            ++counts.memberships;
        }

        std::set<std::string> directionIds;
        for (std::size_t index = 0; index < group.directionCount; ++index) {
            const auto& direction = group.directions[index];
            assert(direction.id != nullptr && direction.id[0] != '\0');
            assert(direction.labelTc != nullptr && direction.labelTc[0] != '\0');
            assert(directionIds.insert(direction.id).second);
            ++counts.directions;
        }
    }
    return counts;
}

}  // namespace

int main() {
    const auto heavy = transitink::heavyRailCatalog();
    const auto light = transitink::lightRailCatalog();
    const auto journey = transitink::journeyTimeCatalog();
    assert(heavy.groupCount == 10);
    assert(light.groupCount == 11);
    assert(journey.locationCount == 35);
    assert(journey.destinationCount == 27);
    assert(journey.pairCount == 83);

    std::set<std::string> journeyLocations;
    std::set<std::string> journeyDestinations;
    std::set<std::string> journeyPairs;
    for (std::size_t index = 0; index < journey.locationCount; ++index) {
        const auto& item = journey.locations[index];
        assert(item.id != nullptr && item.id[0] != '\0');
        assert(item.labelTc != nullptr && item.labelTc[0] != '\0');
        assert(journeyLocations.insert(item.id).second);
    }
    for (std::size_t index = 0; index < journey.destinationCount; ++index) {
        const auto& item = journey.destinations[index];
        assert(item.id != nullptr && item.id[0] != '\0');
        assert(item.labelTc != nullptr && item.labelTc[0] != '\0');
        assert(journeyDestinations.insert(item.id).second);
    }
    for (std::size_t index = 0; index < journey.pairCount; ++index) {
        const auto& pair = journey.pairs[index];
        assert(journeyLocations.count(pair.locationId) == 1);
        assert(journeyDestinations.count(pair.destinationId) == 1);
        assert(journeyPairs.insert(std::string(pair.locationId) + "/" +
                                   pair.destinationId)
                   .second);
    }
    assert(journeyPairs.size() == 83);
    const auto* k07 = transitink::findJourneyTimeLocation("K07");
    assert(k07 != nullptr &&
           std::string(k07->labelTc) == "西九龍公路西行近港鐵南昌站");
    const auto* atsca = transitink::findJourneyTimeDestination("ATSCA");
    assert(atsca != nullptr &&
           std::string(atsca->labelTc) == "機場經八號幹線");
    const auto* h9 = transitink::findJourneyTimeLocation("H9");
    assert(h9 != nullptr &&
           std::string(h9->labelTc) == "鴨脷洲橋道北行近黃竹坑道");
    const auto* tmclk = transitink::findJourneyTimeDestination("TMCLK");
    assert(tmclk != nullptr &&
           std::string(tmclk->labelTc) == "機場經屯門赤鱲角隧道");
    assert(transitink::isJourneyTimePairValid("K07", "ATSCA"));
    assert(!transitink::isJourneyTimePairValid("K07", "CH"));
    assert(!transitink::isJourneyTimePairValid("NO", "PAIR"));

    const auto heavyCounts = validateCatalog(heavy);
    assert(heavyCounts.memberships == 121);
    assert(heavyCounts.uniqueStations.size() == 98);
    assert(heavyCounts.directions == 20);

    const auto lightCounts = validateCatalog(light);
    assert(lightCounts.memberships == 205);
    assert(lightCounts.uniqueStations.size() == 68);
    assert(lightCounts.directions == 20);

    const auto* tsw = transitink::findTransitCatalogStation(
        transitink::RailMode::HeavyRail, "TWL", "TSW");
    assert(tsw != nullptr && std::string(tsw->labelTc) == "荃灣");
    const auto* yul = transitink::findTransitCatalogStation(
        transitink::RailMode::HeavyRail, "TML", "YUL");
    assert(yul != nullptr && std::string(yul->labelTc) == "元朗");
    const auto* lrYuenLong = transitink::findTransitCatalogStation(
        transitink::RailMode::LightRail, "610", "600");
    assert(lrYuenLong != nullptr && std::string(lrYuenLong->labelTc) == "元朗");
    const auto* hoiWongRoad = transitink::findTransitCatalogStation(
        transitink::RailMode::LightRail, "507", "250");
    assert(hoiWongRoad != nullptr && std::string(hoiWongRoad->labelTc) == "海皇路");

    std::string directionId = "sentinel";
    assert(transitink::lightRailDirectionIdForDestination("610", "元朗",
                                                          directionId));
    assert(directionId == "600");
    assert(transitink::lightRailDirectionIdForDestination(
        "610", "Tuen Mun Ferry Pier", directionId));
    assert(directionId == "1");
    assert(transitink::lightRailDirectionIdForDestination("705", "天水圍",
                                                          directionId));
    assert(directionId == "430");
    assert(!transitink::lightRailDirectionIdForDestination("705", "天榮",
                                                           directionId));
    assert(directionId.empty());

    transitink::MtrWidgetConfig heavyConfig;
    heavyConfig.lineOrRouteId = "TWL";
    heavyConfig.stationId = "TSW";
    assert(MtrClient::requestUrl(heavyConfig) ==
           "https://rt.data.gov.hk/v1/transport/mtr/getSchedule.php?line=TWL&sta=TSW&lang=TC");

    transitink::MtrWidgetConfig lightConfig;
    lightConfig.stationId = "600";
    assert(LightRailClient::requestUrl(lightConfig) ==
           "https://rt.data.gov.hk/v1/transport/mtr/lrt/getSchedule?station_id=600&with_special=1");
    return 0;
}
