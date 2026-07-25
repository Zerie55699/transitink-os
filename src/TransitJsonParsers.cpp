#include "TransitJsonParsers.h"

#include "TransitCatalog.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>

namespace {

bool readRequiredString(JsonVariantConst value, std::string& out) {
    if (!value.is<const char*>()) {
        return false;
    }
    const char* text = value.as<const char*>();
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    out = text;
    return true;
}

bool readOptionalString(JsonObjectConst object,
                        const char* key,
                        std::string& out) {
    if (!object.containsKey(key)) {
        out.clear();
        return true;
    }
    JsonVariantConst value = object[key];
    if (!value.is<const char*>()) {
        return false;
    }
    const char* text = value.as<const char*>();
    out = text == nullptr ? std::string() : std::string(text);
    return true;
}

bool readNullableString(JsonObjectConst object,
                        const char* key,
                        std::string& out) {
    if (!object.containsKey(key) || object[key].isNull()) {
        out.clear();
        return true;
    }
    return readOptionalString(object, key, out);
}

bool readServiceType(JsonVariantConst value, std::string& out) {
    if (value.is<int>()) {
        const int number = value.as<int>();
        if (number < 0) {
            return false;
        }
        out = std::to_string(number);
        return true;
    }
    if (!value.is<const char*>()) {
        return false;
    }
    const char* raw = value.as<const char*>();
    if (raw == nullptr || raw[0] == '\0') {
        return false;
    }
    std::string digits(raw);
    for (char digit : digits) {
        if (digit < '0' || digit > '9') {
            return false;
        }
    }
    const std::size_t firstNonZero = digits.find_first_not_of('0');
    out = firstNonZero == std::string::npos ? "0" : digits.substr(firstNonZero);
    return true;
}

bool parseDigits(const std::string& value,
                 std::size_t offset,
                 std::size_t length,
                 int& out) {
    if (offset + length > value.size()) {
        return false;
    }
    int parsed = 0;
    for (std::size_t index = offset; index < offset + length; ++index) {
        const char digit = value[index];
        if (digit < '0' || digit > '9') {
            return false;
        }
        parsed = parsed * 10 + (digit - '0');
    }
    out = parsed;
    return true;
}

int64_t daysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear =
        (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned dayOfEra =
        yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(dayOfEra) -
           719468;
}

bool isLeapYear(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int daysInMonth(int year, int month) {
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return kDays[month - 1];
}

int64_t parseIsoEpoch(const std::string& value) {
    const bool hasZuluTimezone = value.size() == 20 && value[19] == 'Z';
    const bool hasOffsetTimezone =
        value.size() == 25 && (value[19] == '+' || value[19] == '-') &&
        value[22] == ':';
    if ((!hasZuluTimezone && !hasOffsetTimezone) || value[4] != '-' ||
        value[7] != '-' || value[10] != 'T' || value[13] != ':' ||
        value[16] != ':') {
        return 0;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!parseDigits(value, 0, 4, year) || !parseDigits(value, 5, 2, month) ||
        !parseDigits(value, 8, 2, day) || !parseDigits(value, 11, 2, hour) ||
        !parseDigits(value, 14, 2, minute) || !parseDigits(value, 17, 2, second) ||
        month < 1 || month > 12 || day < 1 ||
        day > daysInMonth(year, month) || hour > 23 || minute > 59 ||
        second > 59) {
        return 0;
    }

    int64_t epoch = daysFromCivil(year, static_cast<unsigned>(month),
                                  static_cast<unsigned>(day)) *
                        86400 +
                    static_cast<int64_t>(hour) * 3600 + minute * 60 + second;
    if (hasZuluTimezone) {
        return epoch;
    }

    int offsetHour = 0;
    int offsetMinute = 0;
    if (!parseDigits(value, 20, 2, offsetHour) ||
        !parseDigits(value, 23, 2, offsetMinute) || offsetHour > 14 ||
        offsetMinute > 59 || (offsetHour == 14 && offsetMinute != 0)) {
        return 0;
    }
    const int64_t offsetSeconds =
        static_cast<int64_t>(offsetHour) * 3600 + offsetMinute * 60;
    return value[19] == '+' ? epoch - offsetSeconds : epoch + offsetSeconds;
}

int64_t parseHongKongEpoch(const std::string& value) {
    if (value.size() != 19 || value[4] != '-' || value[7] != '-' ||
        value[10] != ' ' || value[13] != ':' || value[16] != ':') {
        return 0;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!parseDigits(value, 0, 4, year) || !parseDigits(value, 5, 2, month) ||
        !parseDigits(value, 8, 2, day) || !parseDigits(value, 11, 2, hour) ||
        !parseDigits(value, 14, 2, minute) || !parseDigits(value, 17, 2, second) ||
        month < 1 || month > 12 || day < 1 ||
        day > daysInMonth(year, month) || hour > 23 || minute > 59 ||
        second > 59) {
        return 0;
    }

    constexpr int64_t kHongKongOffsetSeconds = 8 * 60 * 60;
    return daysFromCivil(year, static_cast<unsigned>(month),
                         static_cast<unsigned>(day)) *
               86400 +
           static_cast<int64_t>(hour) * 3600 + minute * 60 + second -
           kHongKongOffsetSeconds;
}

bool readRequiredInt(JsonVariantConst value, int& out) {
    if (!value.is<int>()) {
        return false;
    }
    out = value.as<int>();
    return true;
}

bool readStringOrNonNegativeInt(JsonVariantConst value, std::string& out) {
    if (value.is<const char*>()) {
        const char* text = value.as<const char*>();
        if (text == nullptr || text[0] == '\0') return false;
        out = text;
        return true;
    }
    int number = 0;
    if (!readRequiredInt(value, number) || number < 0) return false;
    out = std::to_string(number);
    return true;
}

bool parseLightRailMinutes(const std::string& value,
                           int64_t nowEpoch,
                           int64_t& eventEpoch) {
    eventEpoch = 0;
    if (value == "正在離開" || value == "即將抵達" || value == "-") {
        return true;
    }

    constexpr const char* kSuffix = " 分鐘";
    constexpr std::size_t kSuffixLength = 7;
    if (value.size() <= kSuffixLength ||
        value.compare(value.size() - kSuffixLength, kSuffixLength, kSuffix) != 0) {
        return false;
    }
    const std::size_t digitCount = value.size() - kSuffixLength;
    int64_t minutes = 0;
    for (std::size_t index = 0; index < digitCount; ++index) {
        const char digit = value[index];
        if (digit < '0' || digit > '9' ||
            minutes > (std::numeric_limits<int64_t>::max() - (digit - '0')) / 10) {
            return false;
        }
        minutes = minutes * 10 + (digit - '0');
    }
    if (minutes > (std::numeric_limits<int64_t>::max() - nowEpoch) / 60) {
        return false;
    }
    eventEpoch = nowEpoch + minutes * 60;
    return true;
}

bool parseEtaJson(const char* json,
                  const transitink::BusWidgetConfig& config,
                  const char* expectedCompany,
                  bool hasServiceType,
                  const char* parseError,
                  const char* shapeError,
                  std::vector<transitink::BusEtaRecord>& records,
                  std::string& error) {
    records.clear();
    error.clear();

    DynamicJsonDocument document(16384);
    const DeserializationError jsonError = deserializeJson(document, json);
    if (jsonError) {
        error = parseError;
        return false;
    }
    if (!document["data"].is<JsonArrayConst>()) {
        error = shapeError;
        return false;
    }

    for (JsonObjectConst item : document["data"].as<JsonArrayConst>()) {
        std::string company;
        std::string route;
        std::string direction;
        std::string eta;
        std::string destination;
        std::string remark;
        std::string destinationEn;
        std::string remarkEn;
        std::string serviceType;
        if (!readRequiredString(item["co"], company) ||
            !readRequiredString(item["route"], route) ||
            !readRequiredString(item["dir"], direction) ||
            !readOptionalString(item, "eta", eta) ||
            !readOptionalString(item, "dest_tc", destination) ||
            !readOptionalString(item, "rmk_tc", remark) ||
            !readOptionalString(item, "dest_en", destinationEn) ||
            !readOptionalString(item, "rmk_en", remarkEn) ||
            (hasServiceType &&
             !readServiceType(item["service_type"], serviceType)) ||
            company != expectedCompany) {
            continue;
        }
        transitink::BusEtaRecord record;
        record.operatorId = config.operatorId;
        record.routeId = std::move(route);
        record.directionId = std::move(direction);
        record.serviceType = hasServiceType ? std::move(serviceType) : "";
        record.eventEpoch = parseIsoEpoch(eta);
        record.destinationLabelTc = std::move(destination);
        record.remarkTc = std::move(remark);
        record.destinationLabelEn = std::move(destinationEn);
        record.remarkEn = std::move(remarkEn);
        record.cancelled = record.remarkTc.find("取消") != std::string::npos;
        records.push_back(std::move(record));
    }
    return true;
}

std::string upperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return value;
}

bool splitTflServiceType(const std::string& value,
                         std::string& originator,
                         std::string& destination) {
    const std::size_t separator = value.find('|');
    if (separator == std::string::npos ||
        value.find('|', separator + 1) != std::string::npos) {
        return false;
    }
    originator = value.substr(0, separator);
    destination = value.substr(separator + 1);
    return isOfficialBusIdentifier(originator) &&
           isOfficialBusIdentifier(destination);
}

bool isSupportedTflRailMode(const std::string& value) {
    return value == "tube" || value == "dlr" || value == "overground" ||
           value == "elizabeth-line" || value == "tram";
}

std::string withoutTflStationSuffix(std::string value) {
    static constexpr const char* kSuffixes[] = {
        " Underground Station",
        " Elizabeth Line Station",
        " London Overground Station",
        " DLR Station",
        " Rail Station",
        " Tram Stop",
        " Station",
    };
    for (const char* suffix : kSuffixes) {
        const std::size_t suffixLength = std::char_traits<char>::length(suffix);
        if (value.size() > suffixLength &&
            value.compare(value.size() - suffixLength, suffixLength, suffix) ==
                0) {
            value.resize(value.size() - suffixLength);
            break;
        }
    }
    return value;
}

std::string compactTflPlatformName(std::string value) {
    const std::size_t separator = value.rfind(" - ");
    return separator == std::string::npos ? value : value.substr(separator + 3);
}

}  // namespace

bool isOfficialBusIdentifier(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    for (char character : value) {
        const bool isDigit = character >= '0' && character <= '9';
        const bool isUpper = character >= 'A' && character <= 'Z';
        const bool isLower = character >= 'a' && character <= 'z';
        if (!isDigit && !isUpper && !isLower) {
            return false;
        }
    }
    return true;
}

bool isOfficialTflIdentifier(const std::string& value) {
    if (value.empty() || value.size() > transitink::kMaxStableIdBytes) {
        return false;
    }
    for (const char character : value) {
        const bool isDigit = character >= '0' && character <= '9';
        const bool isUpper = character >= 'A' && character <= 'Z';
        const bool isLower = character >= 'a' && character <= 'z';
        if (!isDigit && !isUpper && !isLower && character != '-') {
            return false;
        }
    }
    return true;
}

bool mapCitybusDirectionPath(const std::string& direction, std::string& path) {
    path.clear();
    if (direction == "I") {
        path = "inbound";
        return true;
    }
    if (direction == "O") {
        path = "outbound";
        return true;
    }
    return false;
}

bool parseGmbRouteCodesJson(const char* json,
                            const std::string& region,
                            std::vector<std::string>& routeCodes,
                            std::string& error) {
    routeCodes.clear();
    error.clear();
    if (!transitink::isGmbRegionId(region)) {
        error = "專線小巴地區設定不正確";
        return false;
    }

    StaticJsonDocument<128> filter;
    filter["data"]["routes"][0] = true;
    DynamicJsonDocument document(16384);
    const DeserializationError jsonError = deserializeJson(
        document, json, DeserializationOption::Filter(filter));
    if (jsonError) {
        error = "專線小巴路線資料無法解析";
        return false;
    }
    if (!document["data"]["routes"].is<JsonArrayConst>()) {
        error = "專線小巴路線資料格式錯誤";
        return false;
    }
    for (JsonVariantConst value : document["data"]["routes"].as<JsonArrayConst>()) {
        std::string routeCode;
        if (readRequiredString(value, routeCode) &&
            isOfficialBusIdentifier(routeCode)) {
            routeCodes.push_back(std::move(routeCode));
        }
    }
    return true;
}

bool parseGmbDirectionsJson(
    const char* json,
    const std::string& region,
    const std::string& routeCode,
    std::vector<transitink::GmbCatalogDirection>& directions,
    std::string& error) {
    directions.clear();
    error.clear();
    if (!transitink::isGmbRegionId(region) ||
        !isOfficialBusIdentifier(routeCode)) {
        error = "專線小巴路線設定不正確";
        return false;
    }

    StaticJsonDocument<768> filter;
    filter["data"][0]["route_id"] = true;
    filter["data"][0]["region"] = true;
    filter["data"][0]["route_code"] = true;
    filter["data"][0]["description_tc"] = true;
    filter["data"][0]["description_en"] = true;
    filter["data"][0]["directions"][0]["route_seq"] = true;
    filter["data"][0]["directions"][0]["orig_tc"] = true;
    filter["data"][0]["directions"][0]["dest_tc"] = true;
    filter["data"][0]["directions"][0]["orig_en"] = true;
    filter["data"][0]["directions"][0]["dest_en"] = true;
    DynamicJsonDocument document(16384);
    const DeserializationError jsonError = deserializeJson(
        document, json, DeserializationOption::Filter(filter));
    if (jsonError) {
        error = "專線小巴方向資料無法解析";
        return false;
    }
    if (!document["data"].is<JsonArrayConst>()) {
        error = "專線小巴方向資料格式錯誤";
        return false;
    }

    for (JsonObjectConst route : document["data"].as<JsonArrayConst>()) {
        std::string parsedRouteId;
        std::string parsedRegion;
        std::string parsedRouteCode;
        std::string description;
        std::string descriptionEn;
        if (!readStringOrNonNegativeInt(route["route_id"], parsedRouteId) ||
            !readRequiredString(route["region"], parsedRegion) ||
            !readRequiredString(route["route_code"], parsedRouteCode) ||
            !readNullableString(route, "description_tc", description) ||
            !readNullableString(route, "description_en", descriptionEn) ||
            parsedRegion != region || parsedRouteCode != routeCode ||
            !route["directions"].is<JsonArrayConst>()) {
            continue;
        }
        for (JsonObjectConst direction :
             route["directions"].as<JsonArrayConst>()) {
            transitink::GmbCatalogDirection item;
            if (!readStringOrNonNegativeInt(direction["route_seq"],
                                            item.routeSeq) ||
                !readRequiredString(direction["orig_tc"],
                                    item.originLabelTc) ||
                !readRequiredString(direction["dest_tc"],
                                    item.destinationLabelTc)) {
                continue;
            }
            readNullableString(direction, "orig_en", item.originLabelEn);
            readNullableString(direction, "dest_en", item.destinationLabelEn);
            item.region = parsedRegion;
            item.routeId = parsedRouteId;
            item.descriptionTc = description;
            item.descriptionEn = descriptionEn;
            directions.push_back(std::move(item));
        }
    }
    return true;
}

bool parseGmbStopsJson(const char* json,
                       const std::string& routeId,
                       const std::string& routeSeq,
                       std::vector<transitink::GmbCatalogStop>& stops,
                       std::string& error) {
    stops.clear();
    error.clear();
    if (!isOfficialBusIdentifier(routeId) ||
        !isOfficialBusIdentifier(routeSeq)) {
        error = "專線小巴方向設定不正確";
        return false;
    }

    StaticJsonDocument<256> filter;
    filter["data"]["route_stops"][0]["stop_seq"] = true;
    filter["data"]["route_stops"][0]["stop_id"] = true;
    filter["data"]["route_stops"][0]["name_tc"] = true;
    filter["data"]["route_stops"][0]["name_en"] = true;
    DynamicJsonDocument document(32768);
    const DeserializationError jsonError = deserializeJson(
        document, json, DeserializationOption::Filter(filter));
    if (jsonError) {
        error = "專線小巴站點資料無法解析";
        return false;
    }
    if (!document["data"]["route_stops"].is<JsonArrayConst>()) {
        error = "專線小巴站點資料格式錯誤";
        return false;
    }
    for (JsonObjectConst stop :
         document["data"]["route_stops"].as<JsonArrayConst>()) {
        transitink::GmbCatalogStop item;
        if (!readStringOrNonNegativeInt(stop["stop_id"], item.stopId) ||
            !readStringOrNonNegativeInt(stop["stop_seq"], item.stopSeq) ||
            !readRequiredString(stop["name_tc"], item.labelTc)) {
            continue;
        }
        readNullableString(stop, "name_en", item.labelEn);
        stops.push_back(std::move(item));
    }
    return true;
}

bool parseGmbEtaJson(const char* json,
                     const transitink::GmbWidgetConfig& config,
                     transitink::GmbEtaPayload& payload,
                     std::string& error) {
    payload = {};
    error.clear();
    transitink::WidgetConfig widget;
    widget.type = transitink::WidgetType::GmbEta;
    widget.gmb = config;
    if (!transitink::isWidgetConfigValid(widget)) {
        error = "專線小巴設定不完整";
        return false;
    }

    StaticJsonDocument<384> filter;
    filter["data"]["stop_id"] = true;
    filter["data"]["enabled"] = true;
    filter["data"]["description_tc"] = true;
    filter["data"]["description_en"] = true;
    filter["data"]["eta"][0]["diff"] = true;
    filter["data"]["eta"][0]["remarks_tc"] = true;
    filter["data"]["eta"][0]["remarks_en"] = true;
    DynamicJsonDocument document(8192);
    const DeserializationError jsonError = deserializeJson(
        document, json, DeserializationOption::Filter(filter));
    if (jsonError) {
        error = "專線小巴到站時間資料無法解析";
        return false;
    }
    if (!document["data"].is<JsonObjectConst>()) {
        error = "專線小巴到站時間資料格式錯誤";
        return false;
    }
    JsonObjectConst data = document["data"].as<JsonObjectConst>();
    std::string stopId;
    if (!readStringOrNonNegativeInt(data["stop_id"], stopId) ||
        stopId != config.stopId || !data["enabled"].is<bool>() ||
        !readNullableString(data, "description_tc", payload.descriptionTc) ||
        !readNullableString(data, "description_en", payload.descriptionEn) ||
        !data["eta"].is<JsonArrayConst>()) {
        error = "專線小巴到站時間資料格式錯誤";
        return false;
    }
    payload.enabled = data["enabled"].as<bool>();
    for (JsonObjectConst eta : data["eta"].as<JsonArrayConst>()) {
        transitink::GmbEtaRecord record;
        int diffMinutes = -1;
        if (!readRequiredInt(eta["diff"], diffMinutes) || diffMinutes < 0 ||
            !readNullableString(eta, "remarks_tc", record.remarkTc)) {
            continue;
        }
        readNullableString(eta, "remarks_en", record.remarkEn);
        record.diffMinutes = diffMinutes;
        payload.records.push_back(std::move(record));
    }
    return true;
}

bool parseCitybusEtaJson(const char* json,
                         const transitink::BusWidgetConfig& config,
                         std::vector<transitink::BusEtaRecord>& records,
                         std::string& error) {
    if (config.operatorId != transitink::BusOperator::Citybus) {
        records.clear();
        error = "城巴營辦商設定不正確";
        return false;
    }
    return parseEtaJson(json, config, "CTB", false, "城巴到站時間資料無法解析",
                        "城巴到站時間資料格式錯誤", records, error);
}

bool parseKmbEtaJson(const char* json,
                     const transitink::BusWidgetConfig& config,
                     std::vector<transitink::BusEtaRecord>& records,
                     std::string& error) {
    if (config.operatorId != transitink::BusOperator::Kmb &&
        config.operatorId != transitink::BusOperator::LongWin) {
        records.clear();
        error = "九巴及龍運營辦商設定不正確";
        return false;
    }
    return parseEtaJson(json, config, "KMB", true,
                        "九巴及龍運到站時間資料無法解析",
                        "九巴及龍運到站時間資料格式錯誤", records, error);
}

bool parseTflDirectionsJson(
    const char* json,
    const std::string& route,
    std::vector<transitink::BusCatalogRoute>& directions,
    std::string& error) {
    directions.clear();
    error.clear();
    if (!isOfficialBusIdentifier(route)) {
        error = "倫敦巴士路線代號格式不正確";
        return false;
    }

    DynamicJsonDocument document(16384);
    const DeserializationError jsonError = deserializeJson(document, json);
    if (jsonError) {
        error = "倫敦巴士路線資料無法解析";
        return false;
    }
    std::string responseRoute;
    if (!readRequiredString(document["id"], responseRoute) ||
        upperAscii(responseRoute) != upperAscii(route) ||
        !document["routeSections"].is<JsonArrayConst>()) {
        error = "倫敦巴士路線資料格式錯誤";
        return false;
    }

    std::set<std::string> seen;
    for (JsonObjectConst section :
         document["routeSections"].as<JsonArrayConst>()) {
        std::string direction;
        std::string originator;
        std::string destination;
        std::string originLabel;
        std::string destinationLabel;
        if (!readRequiredString(section["direction"], direction) ||
            (direction != "inbound" && direction != "outbound") ||
            !readRequiredString(section["originator"], originator) ||
            !readRequiredString(section["destination"], destination) ||
            !readRequiredString(section["originationName"], originLabel) ||
            !readRequiredString(section["destinationName"], destinationLabel) ||
            !isOfficialBusIdentifier(originator) ||
            !isOfficialBusIdentifier(destination)) {
            continue;
        }
        const std::string serviceType = originator + "|" + destination;
        const std::string key = direction + ":" + serviceType;
        if (!seen.insert(key).second) {
            continue;
        }
        directions.push_back(
            {upperAscii(route), direction, serviceType, originLabel,
             destinationLabel, originLabel, destinationLabel});
    }
    if (directions.empty()) {
        error = "官方服務找不到此倫敦巴士路線方向";
        return false;
    }
    std::sort(directions.begin(), directions.end(), [](const auto& lhs,
                                                        const auto& rhs) {
        return std::tie(lhs.directionId, lhs.serviceType) <
               std::tie(rhs.directionId, rhs.serviceType);
    });
    return true;
}

bool parseTflRouteSequenceJson(
    const char* json,
    const std::string& route,
    const std::string& direction,
    const std::string& serviceType,
    std::vector<transitink::BusCatalogStop>& stops,
    std::string& error) {
    stops.clear();
    error.clear();
    std::string expectedOrigin;
    std::string expectedDestination;
    if (!isOfficialBusIdentifier(route) ||
        (direction != "inbound" && direction != "outbound") ||
        !splitTflServiceType(serviceType, expectedOrigin,
                             expectedDestination)) {
        error = "倫敦巴士站牌查詢參數不正確";
        return false;
    }

    DynamicJsonDocument document(49152);
    const DeserializationError jsonError = deserializeJson(document, json);
    if (jsonError) {
        error = "倫敦巴士站牌資料無法解析";
        return false;
    }
    if (!document["orderedLineRoutes"].is<JsonArrayConst>() ||
        !document["stopPointSequences"].is<JsonArrayConst>()) {
        error = "倫敦巴士站牌資料格式錯誤";
        return false;
    }

    std::set<std::vector<std::string>> matchingSegments;
    std::vector<std::string> sole;
    std::size_t usableRouteCount = 0;
    for (JsonObjectConst ordered :
         document["orderedLineRoutes"].as<JsonArrayConst>()) {
        if (!ordered["naptanIds"].is<JsonArrayConst>()) {
            continue;
        }
        JsonArrayConst ids = ordered["naptanIds"].as<JsonArrayConst>();
        if (ids.size() == 0) {
            continue;
        }
        std::vector<std::string> routeIds;
        routeIds.reserve(ids.size());
        bool validRoute = true;
        for (JsonVariantConst value : ids) {
            std::string id;
            if (!readRequiredString(value, id) ||
                !isOfficialBusIdentifier(id)) {
                validRoute = false;
                break;
            }
            routeIds.push_back(std::move(id));
        }
        if (!validRoute || routeIds.empty()) {
            continue;
        }
        ++usableRouteCount;
        sole = routeIds;
        for (auto start = routeIds.begin(); start != routeIds.end(); ++start) {
            if (*start != expectedOrigin) {
                continue;
            }
            const auto end =
                std::find(start, routeIds.end(), expectedDestination);
            if (end != routeIds.end()) {
                matchingSegments.emplace(start, std::next(end));
            }
        }
    }
    std::vector<std::string> selected;
    if (!matchingSegments.empty()) {
        selected = *std::max_element(
            matchingSegments.begin(), matchingSegments.end(),
            [](const auto& lhs, const auto& rhs) {
                return std::make_tuple(lhs.size(), lhs) <
                       std::make_tuple(rhs.size(), rhs);
            });
    } else if (usableRouteCount == 1) {
        selected = std::move(sole);
    }
    if (selected.empty()) {
        error = "官方服務找不到所選倫敦巴士路線分支";
        return false;
    }

    std::map<std::string, std::string> stationLabels;
    for (JsonObjectConst sequence :
         document["stopPointSequences"].as<JsonArrayConst>()) {
        if (!sequence["stopPoint"].is<JsonArrayConst>()) {
            continue;
        }
        for (JsonObjectConst station :
             sequence["stopPoint"].as<JsonArrayConst>()) {
            std::string id;
            std::string label;
            if (readRequiredString(station["id"], id) &&
                readRequiredString(station["name"], label) &&
                isOfficialBusIdentifier(id)) {
                stationLabels.emplace(std::move(id), std::move(label));
            }
        }
    }
    if (selected.size() > std::numeric_limits<uint16_t>::max()) {
        error = "倫敦巴士站牌數量超出支援範圍";
        return false;
    }
    uint16_t sequence = 1;
    for (const std::string& id : selected) {
        const auto label = stationLabels.find(id);
        if (label == stationLabels.end()) {
            stops.clear();
            error = "倫敦巴士站牌名稱資料不完整";
            return false;
        }
        stops.push_back({id, label->second, sequence++, label->second});
    }
    if (stops.empty()) {
        error = "倫敦巴士路線沒有可用站牌";
        return false;
    }
    return true;
}

bool parseTflEtaJson(const char* json,
                     const transitink::BusWidgetConfig& config,
                     int64_t nowEpoch,
                     std::vector<transitink::BusEtaRecord>& records,
                     std::string& error) {
    records.clear();
    error.clear();
    if (config.operatorId != transitink::BusOperator::Tfl) {
        error = "倫敦巴士營辦商設定不正確";
        return false;
    }
    if (nowEpoch <= 0) {
        error = "時間尚未同步";
        return false;
    }

    DynamicJsonDocument document(32768);
    const DeserializationError jsonError = deserializeJson(document, json);
    if (jsonError) {
        error = "倫敦巴士到站時間資料無法解析";
        return false;
    }
    if (!document.is<JsonArrayConst>()) {
        error = "倫敦巴士到站時間資料格式錯誤";
        return false;
    }
    for (JsonObjectConst item : document.as<JsonArrayConst>()) {
        std::string route;
        std::string direction;
        std::string destination;
        int seconds = -1;
        if (!readRequiredString(item["lineId"], route) ||
            !readRequiredString(item["direction"], direction) ||
            !readRequiredInt(item["timeToStation"], seconds) ||
            seconds < 0 ||
            !readOptionalString(item, "destinationName", destination) ||
            upperAscii(route) != upperAscii(config.routeId) ||
            direction != config.directionId) {
            continue;
        }
        transitink::BusEtaRecord record;
        record.operatorId = transitink::BusOperator::Tfl;
        record.routeId = upperAscii(config.routeId);
        record.directionId = direction;
        record.eventEpoch = nowEpoch + seconds;
        record.destinationLabelTc =
            destination.empty() ? config.destinationLabelTc : destination;
        record.destinationLabelEn =
            destination.empty() ? config.destinationLabelEn : destination;
        records.push_back(std::move(record));
    }
    return true;
}

bool parseTflRailLinesJson(
    const char* json,
    std::vector<transitink::StaticCatalogEntry>& lines,
    std::string& error) {
    lines.clear();
    error.clear();

    StaticJsonDocument<192> filter;
    filter[0]["id"] = true;
    filter[0]["name"] = true;
    filter[0]["modeName"] = true;
    DynamicJsonDocument document(8192);
    const DeserializationError jsonError = deserializeJson(
        document, json, DeserializationOption::Filter(filter));
    if (jsonError) {
        error = "倫敦鐵路路線資料無法解析";
        return false;
    }
    if (!document.is<JsonArrayConst>()) {
        error = "倫敦鐵路路線資料格式錯誤";
        return false;
    }

    std::set<std::string> seen;
    for (JsonObjectConst item : document.as<JsonArrayConst>()) {
        std::string id;
        std::string name;
        std::string mode;
        if (!readRequiredString(item["id"], id) ||
            !readRequiredString(item["name"], name) ||
            !readRequiredString(item["modeName"], mode) ||
            !isOfficialTflIdentifier(id) ||
            !isSupportedTflRailMode(mode) ||
            name.size() > transitink::kMaxStaticCatalogLabelBytes ||
            !seen.insert(id).second) {
            continue;
        }
        if (lines.size() >= transitink::kMaxStaticCatalogEntries) {
            lines.clear();
            error = "倫敦鐵路路線數量超出支援範圍";
            return false;
        }
        lines.push_back({std::move(id), name, std::move(name)});
    }
    if (lines.empty()) {
        error = "倫敦鐵路路線目錄不可為空";
        return false;
    }
    std::sort(lines.begin(), lines.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.labelEn, lhs.id) < std::tie(rhs.labelEn, rhs.id);
    });
    return true;
}

bool parseTflRailStationsJson(
    const char* json,
    std::vector<transitink::StaticCatalogEntry>& stations,
    std::string& error) {
    stations.clear();
    error.clear();

    DynamicJsonDocument document(24576);
    const DeserializationError jsonError = deserializeJson(document, json);
    if (jsonError) {
        error = "倫敦鐵路車站資料無法解析";
        return false;
    }
    if (!document.is<JsonArrayConst>()) {
        error = "倫敦鐵路車站資料格式錯誤";
        return false;
    }

    std::set<std::string> seen;
    for (JsonObjectConst item : document.as<JsonArrayConst>()) {
        std::string id;
        std::string name;
        if (!readRequiredString(item["id"], id) ||
            !readRequiredString(item["commonName"], name) ||
            !isOfficialTflIdentifier(id) || !seen.insert(id).second) {
            continue;
        }
        name = withoutTflStationSuffix(std::move(name));
        if (name.empty() ||
            name.size() > transitink::kMaxStaticCatalogLabelBytes) {
            continue;
        }
        if (stations.size() >= transitink::kMaxStaticCatalogEntries) {
            stations.clear();
            error = "倫敦鐵路車站數量超出支援範圍";
            return false;
        }
        stations.push_back({std::move(id), name, std::move(name)});
    }
    if (stations.empty()) {
        error = "倫敦鐵路路線沒有可用車站";
        return false;
    }
    std::sort(stations.begin(), stations.end(),
              [](const auto& lhs, const auto& rhs) {
                  return std::tie(lhs.labelEn, lhs.id) <
                         std::tie(rhs.labelEn, rhs.id);
              });
    return true;
}

bool parseTflRailArrivalsJson(
    const char* json,
    const transitink::MtrWidgetConfig& config,
    int64_t nowEpoch,
    std::vector<transitink::RailArrivalRecord>& records,
    std::string& error) {
    records.clear();
    error.clear();
    if (config.mode != transitink::RailMode::LondonRail ||
        !isOfficialTflIdentifier(config.lineOrRouteId) ||
        !isOfficialTflIdentifier(config.stationId) ||
        (config.directionId != "inbound" &&
         config.directionId != "outbound")) {
        error = "倫敦鐵路設定不正確";
        return false;
    }
    if (nowEpoch <= 0) {
        error = "時間尚未同步";
        return false;
    }

    StaticJsonDocument<384> filter;
    filter[0]["lineId"] = true;
    filter[0]["modeName"] = true;
    filter[0]["direction"] = true;
    filter[0]["timeToStation"] = true;
    filter[0]["destinationName"] = true;
    filter[0]["platformName"] = true;
    DynamicJsonDocument document(24576);
    const DeserializationError jsonError = deserializeJson(
        document, json, DeserializationOption::Filter(filter));
    if (jsonError) {
        error = "倫敦鐵路到站時間資料無法解析";
        return false;
    }
    if (!document.is<JsonArrayConst>()) {
        error = "倫敦鐵路到站時間資料格式錯誤";
        return false;
    }

    for (JsonObjectConst item : document.as<JsonArrayConst>()) {
        std::string line;
        std::string mode;
        std::string direction;
        std::string destination;
        std::string platform;
        int seconds = -1;
        if (!readRequiredString(item["lineId"], line) ||
            !readRequiredString(item["modeName"], mode) ||
            !readRequiredString(item["direction"], direction) ||
            !readRequiredInt(item["timeToStation"], seconds) ||
            !readNullableString(item, "destinationName", destination) ||
            !readNullableString(item, "platformName", platform) ||
            line != config.lineOrRouteId ||
            !isSupportedTflRailMode(mode) ||
            direction != config.directionId || seconds < 0 ||
            seconds > 24 * 60 * 60) {
            continue;
        }
        destination = withoutTflStationSuffix(std::move(destination));
        platform = compactTflPlatformName(std::move(platform));
        transitink::RailArrivalRecord record;
        record.mode = transitink::RailMode::LondonRail;
        record.lineOrRouteId = config.lineOrRouteId;
        record.stationId = config.stationId;
        record.directionId = config.directionId;
        record.eventEpoch = nowEpoch + seconds;
        record.destinationLabelTc = destination;
        record.destinationLabelEn = std::move(destination);
        record.platformLabelTc = platform;
        record.platformLabelEn = std::move(platform);
        records.push_back(std::move(record));
    }
    return true;
}

bool parseMtrNextTrainJson(
    const char* json,
    const transitink::MtrWidgetConfig& config,
    std::vector<transitink::RailArrivalRecord>& records,
    int64_t& dataEpoch,
    std::string& error) {
    records.clear();
    dataEpoch = 0;
    error.clear();
    if (config.mode != transitink::RailMode::HeavyRail) {
        error = "港鐵網絡設定不正確";
        return false;
    }
    if (transitink::findTransitCatalogStation(transitink::RailMode::HeavyRail,
                                              config.lineOrRouteId,
                                              config.stationId) == nullptr) {
        error = "港鐵路綫或車站設定不正確";
        return false;
    }
    if (transitink::findTransitCatalogDirection(
            transitink::RailMode::HeavyRail, config.lineOrRouteId,
            config.directionId) == nullptr) {
        error = "港鐵方向設定不正確";
        return false;
    }

    DynamicJsonDocument document(16384);
    const DeserializationError jsonError = deserializeJson(document, json);
    if (jsonError) {
        error = "港鐵列車資料無法解析";
        return false;
    }
    int status = 0;
    if (!readRequiredInt(document["status"], status)) {
        error = "港鐵列車資料格式錯誤";
        return false;
    }
    if (status == 0) {
        error = "港鐵服務暫未能提供";
        return false;
    }
    if (status != 1) {
        error = "港鐵列車資料格式錯誤";
        return false;
    }

    std::string message;
    std::string systemTime;
    if (!readRequiredString(document["message"], message) ||
        !readRequiredString(document["sys_time"], systemTime) ||
        !document["data"].is<JsonObjectConst>()) {
        error = "港鐵列車資料格式錯誤";
        return false;
    }
    dataEpoch = parseHongKongEpoch(systemTime);
    if (dataEpoch <= 0) {
        dataEpoch = 0;
        error = "港鐵列車資料格式錯誤";
        return false;
    }

    bool delayed = false;
    if (document.containsKey("isdelay")) {
        std::string delayValue;
        if (!readRequiredString(document["isdelay"], delayValue) ||
            (delayValue != "Y" && delayValue != "N")) {
            dataEpoch = 0;
            error = "港鐵列車資料格式錯誤";
            return false;
        }
        delayed = delayValue == "Y";
    }

    const std::string scheduleKey = config.lineOrRouteId + "-" + config.stationId;
    JsonVariantConst scheduleValue = document["data"][scheduleKey.c_str()];
    if (!scheduleValue.is<JsonObjectConst>()) {
        dataEpoch = 0;
        error = "港鐵列車資料格式錯誤";
        return false;
    }
    JsonObjectConst schedule = scheduleValue.as<JsonObjectConst>();
    const std::string serviceMessage =
        delayed ? "列車服務延誤"
                : (message == "successful" ? "" : "請留意列車服務安排");

    for (const char* direction : {"UP", "DOWN"}) {
        if (!schedule.containsKey(direction)) continue;
        if (!schedule[direction].is<JsonArrayConst>()) {
            records.clear();
            dataEpoch = 0;
            error = "港鐵列車資料格式錯誤";
            return false;
        }
        for (JsonObjectConst item : schedule[direction].as<JsonArrayConst>()) {
            std::string validValue;
            std::string platform;
            std::string arrivalTime;
            std::string destinationId;
            if (!readRequiredString(item["valid"], validValue) ||
                (validValue != "Y" && validValue != "N") ||
                !readStringOrNonNegativeInt(item["plat"], platform) ||
                !readRequiredString(item["time"], arrivalTime) ||
                !readRequiredString(item["dest"], destinationId)) {
                continue;
            }

            const auto* destination = transitink::findTransitCatalogStation(
                transitink::RailMode::HeavyRail, config.lineOrRouteId,
                destinationId);
            transitink::RailArrivalRecord record;
            record.mode = transitink::RailMode::HeavyRail;
            record.lineOrRouteId = config.lineOrRouteId;
            record.stationId = config.stationId;
            record.directionId = direction;
            record.eventEpoch = parseHongKongEpoch(arrivalTime);
            record.destinationLabelTc =
                destination == nullptr || destination->labelTc == nullptr
                    ? ""
                    : destination->labelTc;
            record.destinationLabelEn =
                destination == nullptr || destination->labelEn == nullptr
                    ? ""
                    : destination->labelEn;
            record.platformLabelTc = platform + " 號月台";
            record.platformLabelEn = "Platform " + platform;
            record.messageTc = serviceMessage;
            record.messageEn =
                delayed ? "Train service delayed"
                        : (message == "successful"
                               ? ""
                               : "Please check the latest train service arrangements");
            record.valid = validValue == "Y" && record.eventEpoch > 0 &&
                           destination != nullptr;
            records.push_back(std::move(record));
        }
    }
    return true;
}

bool parseLightRailJson(
    const char* json,
    const transitink::MtrWidgetConfig& config,
    int64_t nowEpoch,
    std::vector<transitink::RailArrivalRecord>& records,
    int64_t& dataEpoch,
    std::string& error) {
    records.clear();
    dataEpoch = 0;
    error.clear();
    if (config.mode != transitink::RailMode::LightRail) {
        error = "輕鐵網絡設定不正確";
        return false;
    }
    if (nowEpoch <= 0) {
        error = "時間尚未同步";
        return false;
    }
    if (transitink::findTransitCatalogStation(transitink::RailMode::LightRail,
                                              config.lineOrRouteId,
                                              config.stationId) == nullptr) {
        error = "輕鐵路綫或車站設定不正確";
        return false;
    }
    if (transitink::findTransitCatalogDirection(
            transitink::RailMode::LightRail, config.lineOrRouteId,
            config.directionId) == nullptr) {
        error = "輕鐵方向設定不正確";
        return false;
    }

    DynamicJsonDocument document(32768);
    const DeserializationError jsonError = deserializeJson(document, json);
    if (jsonError) {
        error = "輕鐵列車資料無法解析";
        return false;
    }
    int status = 0;
    if (!readRequiredInt(document["status"], status)) {
        error = "輕鐵列車資料格式錯誤";
        return false;
    }
    if (status == 0) {
        error = "輕鐵服務暫未能提供";
        return false;
    }
    if (status != 1 || !document["platform_list"].is<JsonArrayConst>()) {
        error = "輕鐵列車資料格式錯誤";
        return false;
    }

    std::string systemTime;
    if (!readRequiredString(document["system_time"], systemTime)) {
        error = "輕鐵列車資料格式錯誤";
        return false;
    }
    dataEpoch = parseHongKongEpoch(systemTime);
    if (dataEpoch <= 0) {
        dataEpoch = 0;
        error = "輕鐵列車資料格式錯誤";
        return false;
    }

    for (JsonObjectConst platform : document["platform_list"].as<JsonArrayConst>()) {
        int platformId = 0;
        if (!readRequiredInt(platform["platform_id"], platformId) || platformId < 0 ||
            !platform["route_list"].is<JsonArrayConst>()) {
            continue;
        }
        for (JsonObjectConst item : platform["route_list"].as<JsonArrayConst>()) {
            int special = 0;
            if (!readRequiredInt(item["special"], special)) continue;
            if (special == 1) continue;
            if (special != 0) continue;

            std::string routeId;
            std::string destinationEn;
            std::string destinationTc;
            std::string timeTc;
            int stopped = 0;
            if (!readRequiredString(item["route_no"], routeId) ||
                !readRequiredString(item["dest_en"], destinationEn) ||
                !readRequiredString(item["dest_ch"], destinationTc) ||
                !readRequiredString(item["time_ch"], timeTc) ||
                !readRequiredInt(item["stop"], stopped)) {
                continue;
            }

            std::string directionId;
            bool mapped = transitink::lightRailDirectionIdForDestination(
                routeId, destinationTc, directionId);
            if (!mapped) {
                mapped = transitink::lightRailDirectionIdForDestination(
                    routeId, destinationEn, directionId);
            }
            int64_t eventEpoch = 0;
            const bool usableTime = stopped != 0 ||
                                    parseLightRailMinutes(timeTc, nowEpoch,
                                                         eventEpoch);

            transitink::RailArrivalRecord record;
            record.mode = transitink::RailMode::LightRail;
            record.lineOrRouteId = std::move(routeId);
            record.stationId = config.stationId;
            record.directionId = std::move(directionId);
            record.eventEpoch = eventEpoch;
            record.destinationLabelTc = std::move(destinationTc);
            record.destinationLabelEn = std::move(destinationEn);
            record.platformLabelTc = std::to_string(platformId) + " 號月台";
            record.platformLabelEn =
                "Platform " + std::to_string(platformId);
            record.messageTc = stopped == 0 ? "" : "列車服務暫停";
            record.messageEn = stopped == 0 ? "" : "Train service suspended";
            record.cancelled = stopped != 0;
            record.valid = mapped && usableTime;
            records.push_back(std::move(record));
        }
    }
    return true;
}
