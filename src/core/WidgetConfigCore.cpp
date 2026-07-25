#include "core/WidgetConfigCore.h"

namespace transitink {

namespace {

bool isRequiredIdValid(const std::string& value) {
    return !value.empty() && value.size() <= kMaxStableIdBytes;
}

bool isLabelValid(const std::string& value) {
    return value.size() <= kMaxConfigLabelBytes;
}

bool isRequiredNumericIdValid(const std::string& value) {
    if (!isRequiredIdValid(value)) return false;
    for (const char character : value) {
        if (character < '0' || character > '9') return false;
    }
    return true;
}

}  // namespace

const char* widgetTypeId(WidgetType value) {
    switch (value) {
        case WidgetType::Disabled:
            return "disabled";
        case WidgetType::BusEta:
            return "bus_eta";
        case WidgetType::GmbEta:
            return "gmb_eta";
        case WidgetType::MtrEta:
            return "mtr_eta";
        case WidgetType::JourneyTime:
            return "journey_time";
    }
    return "";
}

bool parseWidgetTypeId(const std::string& value, WidgetType& out) {
    if (value == "disabled") {
        out = WidgetType::Disabled;
    } else if (value == "bus_eta") {
        out = WidgetType::BusEta;
    } else if (value == "gmb_eta") {
        out = WidgetType::GmbEta;
    } else if (value == "mtr_eta") {
        out = WidgetType::MtrEta;
    } else if (value == "journey_time") {
        out = WidgetType::JourneyTime;
    } else {
        return false;
    }
    return true;
}

const char* busOperatorId(BusOperator value) {
    switch (value) {
        case BusOperator::Kmb:
            return "kmb";
        case BusOperator::LongWin:
            return "lwb";
        case BusOperator::Citybus:
            return "ctb";
        case BusOperator::Tfl:
            return "tfl";
    }
    return "";
}

bool parseBusOperatorId(const std::string& value, BusOperator& out) {
    if (value == "kmb") {
        out = BusOperator::Kmb;
    } else if (value == "lwb") {
        out = BusOperator::LongWin;
    } else if (value == "ctb") {
        out = BusOperator::Citybus;
    } else if (value == "tfl") {
        out = BusOperator::Tfl;
    } else {
        return false;
    }
    return true;
}

const char* railModeId(RailMode value) {
    switch (value) {
        case RailMode::HeavyRail:
            return "heavy_rail";
        case RailMode::LightRail:
            return "light_rail";
        case RailMode::LondonRail:
            return "london_rail";
    }
    return "";
}

bool parseRailModeId(const std::string& value, RailMode& out) {
    if (value == "heavy_rail") {
        out = RailMode::HeavyRail;
    } else if (value == "light_rail") {
        out = RailMode::LightRail;
    } else if (value == "london_rail") {
        out = RailMode::LondonRail;
    } else {
        return false;
    }
    return true;
}

bool isGmbRegionId(const std::string& value) {
    return value == "HKI" || value == "KLN" || value == "NT";
}

bool isWidgetConfigValid(const WidgetConfig& widget) {
    const auto optionalLabelValid = [](const std::string& value) {
        return value.empty() || isLabelValid(value);
    };
    switch (widget.type) {
        case WidgetType::Disabled:
            return true;
        case WidgetType::BusEta:
            return busOperatorId(widget.bus.operatorId)[0] != '\0' &&
                   isRequiredIdValid(widget.bus.routeId) &&
                   isRequiredIdValid(widget.bus.directionId) &&
                   isRequiredIdValid(widget.bus.serviceType) && isRequiredIdValid(widget.bus.stopId) &&
                   isLabelValid(widget.bus.routeLabelTc) && isLabelValid(widget.bus.stopLabelTc) &&
                   isLabelValid(widget.bus.destinationLabelTc) &&
                   optionalLabelValid(widget.bus.routeLabelEn) &&
                   optionalLabelValid(widget.bus.stopLabelEn) &&
                   optionalLabelValid(widget.bus.destinationLabelEn);
        case WidgetType::GmbEta:
            return isGmbRegionId(widget.gmb.region) &&
                   isRequiredIdValid(widget.gmb.routeCode) &&
                   isRequiredNumericIdValid(widget.gmb.routeId) &&
                   isRequiredNumericIdValid(widget.gmb.routeSeq) &&
                   isRequiredNumericIdValid(widget.gmb.stopId) &&
                   isRequiredNumericIdValid(widget.gmb.stopSeq) &&
                   isLabelValid(widget.gmb.routeLabelTc) &&
                   isLabelValid(widget.gmb.stopLabelTc) &&
                   isLabelValid(widget.gmb.directionLabelTc) &&
                   optionalLabelValid(widget.gmb.routeLabelEn) &&
                   optionalLabelValid(widget.gmb.stopLabelEn) &&
                   optionalLabelValid(widget.gmb.directionLabelEn);
        case WidgetType::MtrEta:
            return railModeId(widget.mtr.mode)[0] != '\0' &&
                   isRequiredIdValid(widget.mtr.lineOrRouteId) &&
                   isRequiredIdValid(widget.mtr.stationId) && isRequiredIdValid(widget.mtr.directionId) &&
                   isLabelValid(widget.mtr.lineOrRouteLabelTc) && isLabelValid(widget.mtr.stationLabelTc) &&
                   isLabelValid(widget.mtr.directionLabelTc) &&
                   optionalLabelValid(widget.mtr.lineOrRouteLabelEn) &&
                   optionalLabelValid(widget.mtr.stationLabelEn) &&
                   optionalLabelValid(widget.mtr.directionLabelEn);
        case WidgetType::JourneyTime:
            return isRequiredIdValid(widget.journeyTime.locationId) &&
                   isRequiredIdValid(widget.journeyTime.destinationId) &&
                   isLabelValid(widget.journeyTime.locationLabelTc) &&
                   isLabelValid(widget.journeyTime.destinationLabelTc) &&
                   optionalLabelValid(widget.journeyTime.locationLabelEn) &&
                   optionalLabelValid(widget.journeyTime.destinationLabelEn);
    }
    return false;
}

bool widgetPageHasEnabled(const WidgetSlots& widgets, std::size_t page) {
    if (page >= kWidgetPageCount) return false;
    const std::size_t start = widgetPageStart(page);
    for (std::size_t offset = 0; offset < kWidgetsPerPage; ++offset) {
        if (widgets[start + offset].type != WidgetType::Disabled) return true;
    }
    return false;
}

std::size_t enabledWidgetPageCount(const WidgetSlots& widgets) {
    std::size_t count = 0;
    for (std::size_t page = 0; page < kWidgetPageCount; ++page) {
        if (widgetPageHasEnabled(widgets, page)) ++count;
    }
    return count;
}

std::size_t firstEnabledWidgetPage(const WidgetSlots& widgets) {
    for (std::size_t page = 0; page < kWidgetPageCount; ++page) {
        if (widgetPageHasEnabled(widgets, page)) return page;
    }
    return 0;
}

std::size_t nextEnabledWidgetPage(const WidgetSlots& widgets, std::size_t currentPage) {
    if (currentPage >= kWidgetPageCount) currentPage = 0;
    for (std::size_t offset = 1; offset <= kWidgetPageCount; ++offset) {
        const std::size_t candidate = (currentPage + offset) % kWidgetPageCount;
        if (widgetPageHasEnabled(widgets, candidate)) return candidate;
    }
    return currentPage;
}

WidgetSlots migrateLegacyRoutes(const std::vector<bus_eta::RouteSelection>& routes,
                                const std::string& stopNameTc) {
    WidgetSlots slots{};
    const std::size_t count =
        routes.size() < kWidgetsPerPage ? routes.size() : kWidgetsPerPage;
    for (std::size_t index = 0; index < count; ++index) {
        const auto& route = routes[index];
        auto& widget = slots[index];
        widget.type = WidgetType::BusEta;
        widget.bus.operatorId = BusOperator::Kmb;
        widget.bus.routeId = route.route;
        widget.bus.directionId = route.bound;
        widget.bus.serviceType = route.serviceType;
        widget.bus.stopId = route.stopId;
        widget.bus.routeLabelTc = route.route;
        widget.bus.stopLabelTc = stopNameTc;
        widget.bus.destinationLabelTc = route.destTc;
    }
    return slots;
}

}  // namespace transitink
