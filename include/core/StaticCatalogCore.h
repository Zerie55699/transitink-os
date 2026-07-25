#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/WidgetConfigCore.h"

namespace transitink {

constexpr std::size_t kMaxStaticCatalogEntries = 64;
constexpr std::size_t kMaxStaticCatalogIdBytes = 64;
constexpr std::size_t kMaxStaticCatalogLabelBytes = 192;

struct StaticCatalogEntry {
    std::string id;
    std::string labelTc;
    std::string labelEn{};
};

bool listStaticRailLines(RailMode mode,
                         std::vector<StaticCatalogEntry>& entries,
                         std::string& error);
bool listStaticRailStations(RailMode mode,
                            const std::string& lineOrRoute,
                            std::vector<StaticCatalogEntry>& entries,
                            std::string& error);
bool listStaticRailDirections(RailMode mode,
                              const std::string& lineOrRoute,
                              const std::string& station,
                              std::vector<StaticCatalogEntry>& entries,
                              std::string& error);
bool listStaticJourneyLocations(std::vector<StaticCatalogEntry>& entries,
                                std::string& error);
bool listStaticJourneyDestinations(const std::string& locationId,
                                   std::vector<StaticCatalogEntry>& entries,
                                   std::string& error);

}  // namespace transitink
