#include "core/StaticCatalogCore.h"

#include <algorithm>

#include "TransitCatalog.h"

namespace transitink {
namespace {

bool validMode(RailMode mode) {
    return mode == RailMode::HeavyRail || mode == RailMode::LightRail;
}

bool appendEntry(const TransitCatalogItem& item,
                 std::vector<StaticCatalogEntry>& entries,
                 std::string& error) {
    const std::string id = item.id == nullptr ? "" : item.id;
    const std::string label = item.labelTc == nullptr ? "" : item.labelTc;
    const std::string labelEn = item.labelEn == nullptr ? "" : item.labelEn;
    if (id.empty() || id.size() > kMaxStaticCatalogIdBytes || label.empty() ||
        label.size() > kMaxStaticCatalogLabelBytes ||
        labelEn.size() > kMaxStaticCatalogLabelBytes) {
        error = "靜態目錄項目格式不正確";
        return false;
    }
    if (entries.size() >= kMaxStaticCatalogEntries) {
        error = "靜態目錄項目過多";
        return false;
    }
    entries.push_back({id, label, labelEn});
    return true;
}

bool copyItems(const TransitCatalogItem* items,
               std::size_t count,
               std::vector<StaticCatalogEntry>& entries,
               std::string& error) {
    std::vector<StaticCatalogEntry> parsed;
    if (items == nullptr || count == 0 || count > kMaxStaticCatalogEntries) {
        error = count > kMaxStaticCatalogEntries ? "靜態目錄項目過多" : "靜態目錄不可為空";
        entries.clear();
        return false;
    }
    parsed.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        if (!appendEntry(items[index], parsed, error)) {
            entries.clear();
            return false;
        }
    }
    entries = std::move(parsed);
    error.clear();
    return true;
}

}  // namespace

bool listStaticRailLines(RailMode mode,
                         std::vector<StaticCatalogEntry>& entries,
                         std::string& error) {
    entries.clear();
    if (!validMode(mode)) {
        error = "鐵路類型設定不正確";
        return false;
    }
    const auto catalog = mode == RailMode::HeavyRail ? heavyRailCatalog() : lightRailCatalog();
    std::vector<StaticCatalogEntry> parsed;
    parsed.reserve(catalog.groupCount);
    for (std::size_t index = 0; index < catalog.groupCount; ++index) {
        const auto& group = catalog.groups[index];
        const TransitCatalogItem item{group.id, group.labelTc, group.labelEn};
        if (!appendEntry(item, parsed, error)) {
            return false;
        }
    }
    entries = std::move(parsed);
    error.clear();
    return true;
}

bool listStaticRailStations(RailMode mode,
                            const std::string& lineOrRoute,
                            std::vector<StaticCatalogEntry>& entries,
                            std::string& error) {
    entries.clear();
    if (!validMode(mode)) {
        error = "鐵路類型設定不正確";
        return false;
    }
    const auto* group = findTransitCatalogGroup(mode, lineOrRoute);
    if (group == nullptr) {
        error = "鐵路路線設定不正確";
        return false;
    }
    return copyItems(group->stations, group->stationCount, entries, error);
}

bool listStaticRailDirections(RailMode mode,
                              const std::string& lineOrRoute,
                              const std::string& station,
                              std::vector<StaticCatalogEntry>& entries,
                              std::string& error) {
    entries.clear();
    if (!validMode(mode)) {
        error = "鐵路類型設定不正確";
        return false;
    }
    const auto* group = findTransitCatalogGroup(mode, lineOrRoute);
    if (group == nullptr || findTransitCatalogStation(mode, lineOrRoute, station) == nullptr) {
        error = "鐵路車站設定不正確";
        return false;
    }
    return copyItems(group->directions, group->directionCount, entries, error);
}

bool listStaticJourneyLocations(std::vector<StaticCatalogEntry>& entries,
                                std::string& error) {
    const auto catalog = journeyTimeCatalog();
    return copyItems(catalog.locations, catalog.locationCount, entries, error);
}

bool listStaticJourneyDestinations(const std::string& locationId,
                                   std::vector<StaticCatalogEntry>& entries,
                                   std::string& error) {
    entries.clear();
    if (findJourneyTimeLocation(locationId) == nullptr) {
        error = "行車時間地點設定不正確";
        return false;
    }
    const auto catalog = journeyTimeCatalog();
    std::vector<StaticCatalogEntry> parsed;
    for (std::size_t index = 0; index < catalog.pairCount; ++index) {
        const auto& pair = catalog.pairs[index];
        if (locationId != pair.locationId ||
            std::find_if(parsed.begin(), parsed.end(), [&](const StaticCatalogEntry& entry) {
                return entry.id == pair.destinationId;
            }) != parsed.end()) {
            continue;
        }
        const auto* destination = findJourneyTimeDestination(pair.destinationId);
        if (destination == nullptr || !appendEntry(*destination, parsed, error)) {
            entries.clear();
            if (error.empty()) {
                error = "行車時間目的地設定不正確";
            }
            return false;
        }
    }
    if (parsed.empty()) {
        error = "行車時間目的地目錄不可為空";
        return false;
    }
    entries = std::move(parsed);
    error.clear();
    return true;
}

}  // namespace transitink
