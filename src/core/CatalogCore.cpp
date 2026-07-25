#include "core/CatalogCore.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cctype>
#include <set>
#include <tuple>

namespace transitink {
namespace {

bool isOfficialId(const std::string& value) {
    if (value.empty() || value.size() > 64) {
        return false;
    }
    for (unsigned char c : value) {
        if (!std::isalnum(c) && c != '-') {
            return false;
        }
    }
    return true;
}

bool readText(JsonObjectConst object,
              const char* key,
              std::string& value,
              std::string& error,
              bool allowInteger = false) {
    JsonVariantConst field = object[key];
    if (field.is<const char*>()) {
        value = field.as<const char*>();
    } else if (allowInteger && field.is<int>()) {
        value = std::to_string(field.as<int>());
    } else {
        error = "目錄欄位格式錯誤";
        return false;
    }
    if (value.empty() || value.size() > 96) {
        error = "目錄欄位長度不正確";
        return false;
    }
    return true;
}

bool readOptionalText(JsonObjectConst object,
                      const char* key,
                      std::string& value,
                      std::string& error) {
    value.clear();
    if (!object.containsKey(key) || object[key].isNull()) {
        return true;
    }
    return readText(object, key, value, error);
}

bool readIntegerOrNumericString(JsonVariantConst field, int& value) {
    if (field.is<int>()) {
        value = field.as<int>();
        return true;
    }
    if (!field.is<const char*>()) {
        return false;
    }
    const char* text = field.as<const char*>();
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    int parsed = 0;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        if (!std::isdigit(static_cast<unsigned char>(*cursor))) {
            return false;
        }
        parsed = std::min(10000, parsed * 10 + (*cursor - '0'));
    }
    value = parsed;
    return true;
}

template <typename Handler>
bool scanDataObjects(CatalogByteReader& reader,
                     Handler handler,
                     std::string& error,
                     bool requireNonEmpty = true) {
    const std::string dataKey = "\"data\"";
    std::string window;
    int current = -1;
    bool foundData = false;
    while ((current = reader.readByte()) >= 0) {
        window.push_back(static_cast<char>(current));
        if (window.size() > dataKey.size()) {
            window.erase(0, 1);
        }
        if (window == dataKey) {
            foundData = true;
            break;
        }
    }
    if (!foundData) {
        error = "目錄缺少資料陣列";
        return false;
    }

    bool foundColon = false;
    while ((current = reader.readByte()) >= 0) {
        if (current == ':') {
            foundColon = true;
            break;
        }
        if (!std::isspace(static_cast<unsigned char>(current))) {
            error = "目錄資料陣列格式錯誤";
            return false;
        }
    }
    if (!foundColon) {
        error = "目錄資料陣列格式錯誤";
        return false;
    }
    while ((current = reader.readByte()) >= 0 &&
           std::isspace(static_cast<unsigned char>(current))) {
    }
    if (current != '[') {
        error = "目錄資料陣列格式錯誤";
        return false;
    }

    std::size_t objectCount = 0;
    while (true) {
        do {
            current = reader.readByte();
        } while (current >= 0 &&
                 (std::isspace(static_cast<unsigned char>(current)) || current == ','));
        if (current == ']') {
            if (requireNonEmpty && objectCount == 0) {
                error = "目錄資料不可為空";
                return false;
            }
            error.clear();
            return true;
        }
        if (current != '{') {
            error = "目錄項目格式錯誤";
            return false;
        }

        std::string objectJson(1, '{');
        int depth = 1;
        bool inString = false;
        bool escaped = false;
        while (depth > 0 && (current = reader.readByte()) >= 0) {
            objectJson.push_back(static_cast<char>(current));
            if (objectJson.size() > kMaxCatalogObjectBytes) {
                error = "目錄項目過大";
                return false;
            }
            if (inString) {
                if (escaped) {
                    escaped = false;
                } else if (current == '\\') {
                    escaped = true;
                } else if (current == '"') {
                    inString = false;
                }
            } else if (current == '"') {
                inString = true;
            } else if (current == '{') {
                ++depth;
            } else if (current == '}') {
                --depth;
            }
        }
        if (depth != 0 || inString) {
            error = "目錄項目不完整";
            return false;
        }
        if (!handler(objectJson, error)) {
            return false;
        }
        ++objectCount;
    }
}

bool parseObject(const std::string& json, StaticJsonDocument<2048>& doc, std::string& error) {
    const DeserializationError jsonError = deserializeJson(doc, json);
    if (jsonError || doc.overflowed() || !doc.is<JsonObject>()) {
        error = "目錄項目 JSON 錯誤";
        return false;
    }
    return true;
}

bool parseRouteObject(const std::string& json,
                      BusCatalogFormat format,
                      std::vector<BusCatalogRoute>* parsed,
                      std::string& error) {
    StaticJsonDocument<2048> doc;
    if (!parseObject(json, doc, error)) {
        return false;
    }
    JsonObjectConst object = doc.as<JsonObjectConst>();
    std::string route;
    std::string origin;
    std::string destination;
    std::string originEn;
    std::string destinationEn;
    if (!readText(object, "route", route, error) ||
        !readText(object, "orig_tc", origin, error) ||
        !readText(object, "dest_tc", destination, error) ||
        !readOptionalText(object, "orig_en", originEn, error) ||
        !readOptionalText(object, "dest_en", destinationEn, error) ||
        !isOfficialId(route)) {
        if (error.empty()) {
            error = "路線編號格式不正確";
        }
        return false;
    }
    if (format == BusCatalogFormat::Citybus) {
        if (parsed != nullptr) {
            parsed->push_back(
                {route, "I", "1", destination, origin, destinationEn, originEn});
            parsed->push_back(
                {route, "O", "1", origin, destination, originEn, destinationEn});
        }
        return true;
    }

    std::string direction;
    std::string serviceType;
    if (!readText(object, "bound", direction, error) ||
        !readText(object, "service_type", serviceType, error, true) ||
        (direction != "I" && direction != "O") || !isOfficialId(serviceType)) {
        if (error.empty()) {
            error = "路線方向或服務類型格式不正確";
        }
        return false;
    }
    if (parsed != nullptr) {
        parsed->push_back(
            {route, direction, serviceType, origin, destination, originEn,
             destinationEn});
    }
    return true;
}

bool parseStopLabels(CatalogByteReader& reader,
                     const std::set<std::string>* requested,
                     std::vector<BusStopLabel>& labels,
                     std::string& error) {
    std::vector<BusStopLabel> parsed;
    const bool ok = scanDataObjects(reader, [&](const std::string& json, std::string& itemError) {
        StaticJsonDocument<2048> doc;
        if (!parseObject(json, doc, itemError)) {
            return false;
        }
        std::string stop;
        std::string label;
        std::string labelEn;
        JsonObjectConst object = doc.as<JsonObjectConst>();
        if (!readText(object, "stop", stop, itemError) ||
            !readText(object, "name_tc", label, itemError) ||
            !readOptionalText(object, "name_en", labelEn, itemError) ||
            !isOfficialId(stop)) {
            if (itemError.empty()) {
                itemError = "站牌目錄格式不正確";
            }
            return false;
        }
        if (requested == nullptr || requested->count(stop) != 0) {
            parsed.push_back({stop, label, labelEn});
        }
        return true;
    }, error);
    if (!ok) {
        return false;
    }
    std::sort(parsed.begin(), parsed.end(), [](const BusStopLabel& lhs, const BusStopLabel& rhs) {
        return lhs.stopId < rhs.stopId;
    });
    parsed.erase(std::unique(parsed.begin(), parsed.end(), [](const BusStopLabel& lhs,
                                                              const BusStopLabel& rhs) {
        return lhs.stopId == rhs.stopId;
    }), parsed.end());
    labels = std::move(parsed);
    return true;
}

}  // namespace

bool BoundedBodyAccumulator::append(const uint8_t* bytes,
                                    std::size_t length,
                                    std::string& error) {
    if (bytes == nullptr || length > limit_ - body_.size()) {
        error = "回應內容超過大小上限";
        return false;
    }
    body_.append(reinterpret_cast<const char*>(bytes), length);
    error.clear();
    return true;
}

bool BoundedBodyAccumulator::complete(int expectedLength, std::string& error) const {
    if (expectedLength >= 0 && static_cast<std::size_t>(expectedLength) != body_.size()) {
        error = "回應內容不完整";
        return false;
    }
    error.clear();
    return true;
}

CatalogRefreshAction decideCatalogRefresh(bool refreshRequested,
                                          bool cacheUsable,
                                          bool cacheFresh,
                                          bool clockValid) {
    if (refreshRequested) {
        return CatalogRefreshAction::Fetch;
    }
    if (cacheFresh || (cacheUsable && !clockValid)) {
        return CatalogRefreshAction::UseCache;
    }
    return CatalogRefreshAction::Fetch;
}

AtomicCacheRecoveryPlan planAtomicCacheRecovery(bool dataExists,
                                                bool backupExists,
                                                bool tempExists) {
    AtomicCacheRecoveryPlan plan;
    plan.removeTemp = tempExists;
    plan.restoreBackup = !dataExists && backupExists;
    return plan;
}

bool parseBusRoutes(CatalogByteReader& reader,
                    BusCatalogFormat format,
                    std::vector<BusCatalogRoute>& rows,
                    std::string& error) {
    std::vector<BusCatalogRoute> parsed;
    const bool ok = scanDataObjects(reader, [&](const std::string& json, std::string& itemError) {
        return parseRouteObject(json, format, &parsed, itemError);
    }, error);
    if (!ok) {
        return false;
    }

    std::sort(parsed.begin(), parsed.end(), [](const BusCatalogRoute& lhs, const BusCatalogRoute& rhs) {
        return std::tie(lhs.routeId, lhs.directionId, lhs.serviceType) <
               std::tie(rhs.routeId, rhs.directionId, rhs.serviceType);
    });
    parsed.erase(std::unique(parsed.begin(), parsed.end(), [](const BusCatalogRoute& lhs,
                                                              const BusCatalogRoute& rhs) {
        return lhs.routeId == rhs.routeId && lhs.directionId == rhs.directionId &&
               lhs.serviceType == rhs.serviceType;
    }), parsed.end());
    rows = std::move(parsed);
    return true;
}

bool parseBusRouteIds(CatalogByteReader& reader,
                      BusCatalogFormat format,
                      std::vector<std::string>& routeIds,
                      std::string& error) {
    std::vector<std::string> parsed;
    const bool ok = scanDataObjects(reader, [&](const std::string& json, std::string& itemError) {
        std::vector<BusCatalogRoute> objectRows;
        if (!parseRouteObject(json, format, &objectRows, itemError)) {
            return false;
        }
        const std::string& routeId = objectRows.front().routeId;
        if (std::find(parsed.begin(), parsed.end(), routeId) == parsed.end()) {
            if (parsed.size() >= kMaxCatalogRoutes) {
                itemError = "路線目錄項目過多";
                return false;
            }
            parsed.push_back(routeId);
        }
        return true;
    }, error);
    if (!ok) {
        return false;
    }
    std::sort(parsed.begin(), parsed.end());
    routeIds = std::move(parsed);
    return true;
}

bool parseBusDirectionsForRoute(CatalogByteReader& reader,
                                BusCatalogFormat format,
                                const std::string& routeId,
                                std::vector<BusCatalogRoute>& rows,
                                std::string& error) {
    if (!isOfficialId(routeId)) {
        error = "路線查詢參數不正確";
        return false;
    }
    std::vector<BusCatalogRoute> parsed;
    const bool ok = scanDataObjects(reader, [&](const std::string& json, std::string& itemError) {
        std::vector<BusCatalogRoute> objectRows;
        if (!parseRouteObject(json, format, &objectRows, itemError)) {
            return false;
        }
        for (auto& row : objectRows) {
            if (row.routeId != routeId) {
                continue;
            }
            if (parsed.size() >= kMaxCatalogDirections) {
                itemError = "路線方向項目過多";
                return false;
            }
            parsed.push_back(std::move(row));
        }
        return true;
    }, error);
    if (!ok) {
        return false;
    }
    std::sort(parsed.begin(), parsed.end(), [](const BusCatalogRoute& lhs,
                                               const BusCatalogRoute& rhs) {
        return std::tie(lhs.directionId, lhs.serviceType) <
               std::tie(rhs.directionId, rhs.serviceType);
    });
    parsed.erase(std::unique(parsed.begin(), parsed.end(), [](const BusCatalogRoute& lhs,
                                                              const BusCatalogRoute& rhs) {
        return lhs.directionId == rhs.directionId && lhs.serviceType == rhs.serviceType;
    }), parsed.end());
    rows = std::move(parsed);
    return true;
}

bool validateBusRouteCatalog(CatalogByteReader& reader,
                             BusCatalogFormat format,
                             std::string& error) {
    return scanDataObjects(reader, [&](const std::string& json, std::string& itemError) {
        return parseRouteObject(json, format, nullptr, itemError);
    }, error);
}

bool parseBusStopLabels(CatalogByteReader& reader,
                        std::vector<BusStopLabel>& labels,
                        std::string& error) {
    return parseStopLabels(reader, nullptr, labels, error);
}

bool parseMatchingBusStopLabels(CatalogByteReader& reader,
                                const std::vector<std::string>& requestedStopIds,
                                std::vector<BusStopLabel>& labels,
                                std::string& error) {
    std::set<std::string> requested;
    for (const auto& stopId : requestedStopIds) {
        if (!isOfficialId(stopId)) {
            error = "站牌編號格式不正確";
            return false;
        }
        requested.insert(stopId);
    }
    return parseStopLabels(reader, &requested, labels, error);
}

bool parseBusRouteStops(CatalogByteReader& reader,
                        const std::string& route,
                        const std::string& direction,
                        const std::string& serviceType,
                        std::vector<BusCatalogStop>& stops,
                        std::string& error) {
    if (!isOfficialId(route) || (direction != "I" && direction != "O") ||
        !isOfficialId(serviceType)) {
        error = "路線查詢參數不正確";
        return false;
    }
    std::vector<BusCatalogStop> parsed;
    const bool ok = scanDataObjects(reader, [&](const std::string& json, std::string& itemError) {
        StaticJsonDocument<2048> doc;
        if (!parseObject(json, doc, itemError)) {
            return false;
        }
        JsonObjectConst object = doc.as<JsonObjectConst>();
        std::string rowRoute;
        std::string rowDirection;
        std::string stop;
        int sequence = 0;
        if (!readText(object, "route", rowRoute, itemError) ||
            !readText(object, object.containsKey("dir") ? "dir" : "bound", rowDirection, itemError) ||
            !readText(object, "stop", stop, itemError) ||
            !readIntegerOrNumericString(object["seq"], sequence) ||
            !isOfficialId(rowRoute) || !isOfficialId(stop)) {
            if (itemError.empty()) {
                itemError = "路線站牌項目格式不正確";
            }
            return false;
        }
        std::string rowService = serviceType;
        if (object.containsKey("service_type") &&
            !readText(object, "service_type", rowService, itemError, true)) {
            return false;
        }
        if (sequence < 1 || sequence > 9999) {
            itemError = "站牌次序不正確";
            return false;
        }
        if (rowRoute == route && rowDirection == direction && rowService == serviceType) {
            parsed.push_back({stop, "", static_cast<uint16_t>(sequence), ""});
        }
        return true;
    }, error, false);
    if (!ok) {
        return false;
    }
    std::sort(parsed.begin(), parsed.end(), [](const BusCatalogStop& lhs, const BusCatalogStop& rhs) {
        return std::tie(lhs.sequence, lhs.stopId) < std::tie(rhs.sequence, rhs.stopId);
    });
    parsed.erase(std::unique(parsed.begin(), parsed.end(), [](const BusCatalogStop& lhs,
                                                              const BusCatalogStop& rhs) {
        return lhs.stopId == rhs.stopId && lhs.sequence == rhs.sequence;
    }), parsed.end());
    stops = std::move(parsed);
    return true;
}

bool joinBusStopLabels(std::vector<BusCatalogStop>& stops,
                       const std::vector<BusStopLabel>& labels,
                       std::string& error) {
    for (auto& stop : stops) {
        const auto found = std::lower_bound(labels.begin(), labels.end(), stop.stopId,
                                            [](const BusStopLabel& item, const std::string& id) {
            return item.stopId < id;
        });
        if (found == labels.end() || found->stopId != stop.stopId) {
            error = "站牌名稱目錄不完整";
            return false;
        }
        stop.labelTc = found->labelTc;
        stop.labelEn = found->labelEn;
    }
    error.clear();
    return true;
}

bool runValidatedBusCatalogQuery(const std::string& operatorId,
                                 const std::string& route,
                                 const std::string& direction,
                                 const std::string& serviceType,
                                 CatalogFetchCallback fetch,
                                 void* context,
                                 std::string& error) {
    if ((operatorId != "kmb" && operatorId != "lwb" && operatorId != "ctb") ||
        !isOfficialId(route) || (direction != "I" && direction != "O") ||
        !isOfficialId(serviceType) || fetch == nullptr) {
        error = "巴士目錄查詢參數不正確";
        return false;
    }
    if (!fetch(context)) {
        error = "巴士目錄更新失敗";
        return false;
    }
    error.clear();
    return true;
}

CitybusStopDecision resolveCitybusStopLabels(
    const std::vector<std::string>& requiredStopIds,
    const std::vector<BusStopLabel>& freshCache,
    const std::vector<BusStopLabel>& staleCache,
    const std::vector<BusStopLabel>& hydrated,
    bool hydrationAttempted,
    bool hydrationComplete) {
    const auto complete = [&](const std::vector<BusStopLabel>& labels) {
        for (const auto& stopId : requiredStopIds) {
            const auto found = std::find_if(labels.begin(), labels.end(), [&](const BusStopLabel& item) {
                return item.stopId == stopId;
            });
            if (found == labels.end()) {
                return false;
            }
        }
        return true;
    };

    CitybusStopDecision decision;
    if (requiredStopIds.size() > kMaxCitybusRouteStops) {
        decision.error = "城巴路線站數超出安全上限";
        return decision;
    }
    if (complete(freshCache)) {
        decision.resolution = CitybusStopResolution::FreshCache;
        decision.labels = freshCache;
        return decision;
    }
    if (!hydrationAttempted) {
        decision.resolution = CitybusStopResolution::Hydrate;
        decision.shouldFetch = true;
        return decision;
    }
    if (hydrationComplete && complete(hydrated)) {
        decision.resolution = CitybusStopResolution::Hydrated;
        decision.shouldCommit = true;
        decision.labels = hydrated;
        return decision;
    }
    if (complete(staleCache)) {
        decision.resolution = CitybusStopResolution::StaleCache;
        decision.labels = staleCache;
        return decision;
    }
    decision.error = "未能取得完整城巴站名";
    return decision;
}

}  // namespace transitink
