#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/BusEtaCore.h"

namespace transitink {

constexpr uint16_t kConfigSchemaVersion = 2;
constexpr std::size_t kWidgetSlotCount = 4;
constexpr std::size_t kMaxStableIdBytes = 64;
constexpr std::size_t kMaxConfigLabelBytes = 96;

enum class WidgetType : uint8_t { Disabled, BusEta, GmbEta, MtrEta, JourneyTime };
enum class BusOperator : uint8_t { Kmb, LongWin, Citybus };
enum class RailMode : uint8_t { HeavyRail, LightRail };

struct BusWidgetConfig {
    BusOperator operatorId = BusOperator::Kmb;
    std::string routeId, directionId, serviceType, stopId;
    std::string routeLabelTc, stopLabelTc, destinationLabelTc;
};

struct GmbWidgetConfig {
    std::string region, routeCode, routeId, routeSeq, stopId, stopSeq;
    std::string routeLabelTc, stopLabelTc, directionLabelTc;
};

struct MtrWidgetConfig {
    RailMode mode = RailMode::HeavyRail;
    std::string lineOrRouteId, stationId, directionId;
    std::string lineOrRouteLabelTc, stationLabelTc, directionLabelTc;
};

struct JourneyTimeWidgetConfig {
    std::string locationId, destinationId;
    std::string locationLabelTc, destinationLabelTc;
};

struct WidgetConfig {
    WidgetType type = WidgetType::Disabled;
    BusWidgetConfig bus;
    GmbWidgetConfig gmb;
    MtrWidgetConfig mtr;
    JourneyTimeWidgetConfig journeyTime;
};

using WidgetSlots = std::array<WidgetConfig, kWidgetSlotCount>;

const char* widgetTypeId(WidgetType value);
bool parseWidgetTypeId(const std::string& value, WidgetType& out);
const char* busOperatorId(BusOperator value);
bool parseBusOperatorId(const std::string& value, BusOperator& out);
const char* railModeId(RailMode value);
bool parseRailModeId(const std::string& value, RailMode& out);
bool isGmbRegionId(const std::string& value);
bool isWidgetConfigValid(const WidgetConfig& widget);
WidgetSlots migrateLegacyRoutes(const std::vector<bus_eta::RouteSelection>& routes,
                                const std::string& stopNameTc);

}  // namespace transitink
