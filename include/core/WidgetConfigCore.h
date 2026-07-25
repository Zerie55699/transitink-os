#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/BusEtaCore.h"

namespace transitink {

constexpr uint16_t kPreviousConfigSchemaVersion = 2;
constexpr uint16_t kConfigSchemaVersion = 3;
constexpr std::size_t kWidgetsPerPage = 4;
constexpr std::size_t kWidgetPageCount = 3;
constexpr std::size_t kWidgetSlotCount = kWidgetsPerPage * kWidgetPageCount;
constexpr std::size_t kMaxStableIdBytes = 64;
constexpr std::size_t kMaxConfigLabelBytes = 96;
static_assert(kWidgetSlotCount <= 255, "widget slot ids must fit in uint8_t");

enum class WidgetType : uint8_t { Disabled, BusEta, GmbEta, MtrEta, JourneyTime };
enum class BusOperator : uint8_t { Kmb, LongWin, Citybus, Tfl };
enum class RailMode : uint8_t { HeavyRail, LightRail, LondonRail };

struct BusWidgetConfig {
    BusOperator operatorId = BusOperator::Kmb;
    std::string routeId, directionId, serviceType, stopId;
    std::string routeLabelTc, stopLabelTc, destinationLabelTc;
    std::string routeLabelEn{}, stopLabelEn{}, destinationLabelEn{};
};

struct GmbWidgetConfig {
    std::string region, routeCode, routeId, routeSeq, stopId, stopSeq;
    std::string routeLabelTc, stopLabelTc, directionLabelTc;
    std::string routeLabelEn{}, stopLabelEn{}, directionLabelEn{};
};

struct MtrWidgetConfig {
    RailMode mode = RailMode::HeavyRail;
    std::string lineOrRouteId, stationId, directionId;
    std::string lineOrRouteLabelTc, stationLabelTc, directionLabelTc;
    std::string lineOrRouteLabelEn{}, stationLabelEn{}, directionLabelEn{};
};

struct JourneyTimeWidgetConfig {
    std::string locationId, destinationId;
    std::string locationLabelTc, destinationLabelTc;
    std::string locationLabelEn{}, destinationLabelEn{};
};

struct WidgetConfig {
    WidgetType type = WidgetType::Disabled;
    BusWidgetConfig bus;
    GmbWidgetConfig gmb;
    MtrWidgetConfig mtr;
    JourneyTimeWidgetConfig journeyTime;
};

using WidgetSlots = std::array<WidgetConfig, kWidgetSlotCount>;

constexpr std::size_t widgetPageStart(std::size_t page) {
    return page * kWidgetsPerPage;
}

constexpr std::size_t widgetPageForSlot(std::size_t slot) {
    return slot / kWidgetsPerPage;
}

const char* widgetTypeId(WidgetType value);
bool parseWidgetTypeId(const std::string& value, WidgetType& out);
const char* busOperatorId(BusOperator value);
bool parseBusOperatorId(const std::string& value, BusOperator& out);
const char* railModeId(RailMode value);
bool parseRailModeId(const std::string& value, RailMode& out);
bool isGmbRegionId(const std::string& value);
bool isWidgetConfigValid(const WidgetConfig& widget);
bool widgetPageHasEnabled(const WidgetSlots& widgets, std::size_t page);
std::size_t enabledWidgetPageCount(const WidgetSlots& widgets);
std::size_t firstEnabledWidgetPage(const WidgetSlots& widgets);
std::size_t nextEnabledWidgetPage(const WidgetSlots& widgets, std::size_t currentPage);
WidgetSlots migrateLegacyRoutes(const std::vector<bus_eta::RouteSelection>& routes,
                                const std::string& stopNameTc);

}  // namespace transitink
