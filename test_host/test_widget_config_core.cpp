#include "core/WidgetConfigCore.h"

#include <cassert>
#include <string>
#include <vector>

int main() {
    transitink::WidgetSlots slots{};
    static_assert(slots.size() == 12);
    static_assert(transitink::kWidgetsPerPage == 4);
    static_assert(transitink::kWidgetPageCount == 3);
    for (const auto& slot : slots) assert(slot.type == transitink::WidgetType::Disabled);

    slots[0].type = transitink::WidgetType::BusEta;
    slots[1].type = transitink::WidgetType::BusEta;
    assert(transitink::isWidgetConfigValid(slots[0]) == false);
    assert(transitink::widgetPageForSlot(0) == 0);
    assert(transitink::widgetPageForSlot(4) == 1);
    assert(transitink::widgetPageStart(2) == 8);
    assert(transitink::enabledWidgetPageCount(slots) == 1);
    assert(transitink::firstEnabledWidgetPage(slots) == 0);
    slots[0].type = transitink::WidgetType::Disabled;
    slots[1].type = transitink::WidgetType::Disabled;
    slots[8].type = transitink::WidgetType::JourneyTime;
    assert(transitink::enabledWidgetPageCount(slots) == 1);
    assert(transitink::firstEnabledWidgetPage(slots) == 2);
    assert(transitink::nextEnabledWidgetPage(slots, 0) == 2);
    assert(transitink::nextEnabledWidgetPage(slots, 2) == 2);

    std::vector<bus_eta::RouteSelection> legacy = {
        {"268B", "O", "1", "STOP-A", "紅磡碼頭"},
        {"968", "I", "1", "STOP-B", "元朗西"},
    };
    const auto migrated = transitink::migrateLegacyRoutes(legacy, "元朗廣場");
    assert(migrated[0].type == transitink::WidgetType::BusEta);
    assert(migrated[0].bus.operatorId == transitink::BusOperator::Kmb);
    assert(migrated[0].bus.routeId == "268B");
    assert(migrated[2].type == transitink::WidgetType::Disabled);

    transitink::WidgetType widgetType;
    assert(std::string(transitink::widgetTypeId(transitink::WidgetType::Disabled)) == "disabled");
    assert(std::string(transitink::widgetTypeId(transitink::WidgetType::BusEta)) == "bus_eta");
    assert(std::string(transitink::widgetTypeId(transitink::WidgetType::GmbEta)) == "gmb_eta");
    assert(std::string(transitink::widgetTypeId(transitink::WidgetType::MtrEta)) == "mtr_eta");
    assert(std::string(transitink::widgetTypeId(transitink::WidgetType::JourneyTime)) == "journey_time");
    assert(transitink::parseWidgetTypeId("mtr_eta", widgetType));
    assert(widgetType == transitink::WidgetType::MtrEta);
    assert(!transitink::parseWidgetTypeId("unknown", widgetType));
    assert(transitink::parseWidgetTypeId("gmb_eta", widgetType));
    assert(widgetType == transitink::WidgetType::GmbEta);

    transitink::BusOperator busOperator;
    assert(std::string(transitink::busOperatorId(transitink::BusOperator::Kmb)) == "kmb");
    assert(std::string(transitink::busOperatorId(transitink::BusOperator::LongWin)) == "lwb");
    assert(std::string(transitink::busOperatorId(transitink::BusOperator::Citybus)) == "ctb");
    assert(std::string(transitink::busOperatorId(transitink::BusOperator::Tfl)) == "tfl");
    assert(transitink::parseBusOperatorId("lwb", busOperator));
    assert(busOperator == transitink::BusOperator::LongWin);
    assert(!transitink::parseBusOperatorId("unknown", busOperator));
    assert(transitink::parseBusOperatorId("tfl", busOperator));
    assert(busOperator == transitink::BusOperator::Tfl);

    transitink::RailMode railMode;
    assert(std::string(transitink::railModeId(transitink::RailMode::HeavyRail)) == "heavy_rail");
    assert(std::string(transitink::railModeId(transitink::RailMode::LightRail)) == "light_rail");
    assert(std::string(transitink::railModeId(transitink::RailMode::LondonRail)) ==
           "london_rail");
    assert(transitink::parseRailModeId("light_rail", railMode));
    assert(railMode == transitink::RailMode::LightRail);
    assert(transitink::parseRailModeId("london_rail", railMode));
    assert(railMode == transitink::RailMode::LondonRail);
    assert(!transitink::parseRailModeId("unknown", railMode));

    transitink::WidgetConfig busWidget;
    busWidget.type = transitink::WidgetType::BusEta;
    busWidget.bus.routeId = "268B";
    busWidget.bus.directionId = "O";
    busWidget.bus.serviceType = "1";
    busWidget.bus.stopId = "STOP-A";
    assert(transitink::isWidgetConfigValid(busWidget));
    busWidget.bus.operatorId = transitink::BusOperator::Tfl;
    busWidget.bus.directionId = "inbound";
    busWidget.bus.serviceType = "490012280A|490015832E";
    busWidget.bus.stopId = "490000089A";
    assert(transitink::isWidgetConfigValid(busWidget));

    transitink::WidgetConfig gmbWidget;
    gmbWidget.type = transitink::WidgetType::GmbEta;
    gmbWidget.gmb.region = "HKI";
    gmbWidget.gmb.routeCode = "69";
    gmbWidget.gmb.routeId = "2000410";
    gmbWidget.gmb.routeSeq = "1";
    gmbWidget.gmb.stopId = "20003337";
    gmbWidget.gmb.stopSeq = "1";
    assert(transitink::isWidgetConfigValid(gmbWidget));
    gmbWidget.gmb.region = "HK";
    assert(!transitink::isWidgetConfigValid(gmbWidget));

    transitink::WidgetConfig mtrWidget;
    mtrWidget.type = transitink::WidgetType::MtrEta;
    mtrWidget.mtr.lineOrRouteId = "TML";
    mtrWidget.mtr.stationId = "YUL";
    mtrWidget.mtr.directionId = "UP";
    assert(transitink::isWidgetConfigValid(mtrWidget));

    transitink::WidgetConfig journeyWidget;
    journeyWidget.type = transitink::WidgetType::JourneyTime;
    journeyWidget.journeyTime.locationId = "HOME";
    journeyWidget.journeyTime.destinationId = "WORK";
    assert(transitink::isWidgetConfigValid(journeyWidget));
    assert(transitink::isWidgetConfigValid(transitink::WidgetConfig{}));

    return 0;
}
