#include "WidgetCatalogService.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>

#include <algorithm>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "core/CatalogCore.h"
#include "core/StaticCatalogCore.h"
#include "generated/TransitCatalogAssets.h"

namespace {

constexpr time_t kValidEpoch = 1700000000;
constexpr const char* kCatalogUpdateMetadataPath = "/catalog_update.meta";
constexpr const char* kUpdatedRouteIndexPath = "/catalog_route_index.json";
constexpr const char* kGmbRoutesCachePath = "/gmb_routes.json";
constexpr const char* kKmbRouteListCachePath = "/ui_kmb_routes.json";
constexpr const char* kCitybusRouteListCachePath = "/ui_ctb_routes.json";
constexpr const char* kTflRailLinesCachePath = "/tfl_rail_lines.json";

struct CachePaths {
    const char* data;
    const char* temp;
    const char* backup;
    const char* metadata;
};

constexpr CachePaths kCachePaths[] = {
    {"/kmb_routes.json", "/kmb_routes.json.tmp", "/kmb_routes.json.bak", "/kmb_routes.meta"},
    {"/kmb_stops.json", "/kmb_stops.json.tmp", "/kmb_stops.json.bak", "/kmb_stops.meta"},
    {"/ctb_routes.json", "/ctb_routes.json.tmp", "/ctb_routes.json.bak", "/ctb_routes.meta"},
    {"/ctb_stops.json", "/ctb_stops.json.tmp", "/ctb_stops.json.bak", "/ctb_stops.meta"},
};

const CachePaths& pathsFor(uint8_t index) {
    return kCachePaths[index];
}

class FileCatalogReader final : public transitink::CatalogByteReader {
public:
    explicit FileCatalogReader(File& file) : file_(file) {}
    int readByte() override { return file_.available() ? file_.read() : -1; }

private:
    File& file_;
};

class StringCatalogReader final : public transitink::CatalogByteReader {
public:
    explicit StringCatalogReader(const String& value) : value_(value) {}
    int readByte() override {
        return offset_ < value_.length() ? static_cast<unsigned char>(value_[offset_++]) : -1;
    }

private:
    const String& value_;
    std::size_t offset_ = 0;
};

template <typename Fill>
void appendJsonObject(String& json, bool& first, Fill fill) {
    StaticJsonDocument<768> doc;
    fill(doc.to<JsonObject>());
    String item;
    serializeJson(doc, item);
    if (!first) {
        json += ',';
    }
    json += item;
    first = false;
}

void beginList(String& json) {
    json = "{\"data\":[";
}

void endList(String& json) {
    json += "]}";
}

bool isBusQueryValid(const String& route, const String& direction, const String& serviceType) {
    std::string ignored;
    auto noFetch = [](void*) { return true; };
    return transitink::runValidatedBusCatalogQuery(
        "kmb", route.c_str(), direction.c_str(), serviceType.c_str(), noFetch, nullptr, ignored);
}

String citybusAggregateCachePath(const String& route,
                                 const String& direction,
                                 const char* suffix) {
    return String("/ctb_") + route + "_" + direction + suffix;
}

bool fileCacheUsable(const String& dataPath) {
    File file = LittleFS.open(dataPath, FILE_READ);
    const bool usable = file && file.size() > 0;
    if (file) {
        file.close();
    }
    return usable;
}

String kmbRouteStopCachePath(const String& route,
                             const String& direction,
                             const String& serviceType) {
    return String("/kmb_rs_") + route + "_" + direction + "_" + serviceType + ".json";
}

String gmbDirectionCachePath(const String& routeCode) {
    return String("/gmb_dir_") + routeCode + ".json";
}

String gmbStopCachePath(const String& routeId, const String& routeSeq) {
    return String("/gmb_stop_") + routeId + "_" + routeSeq + ".json";
}

String busOverrideCachePath(transitink::BusOperator op, const String& route) {
    return String("/catalog_bus_") + transitink::busOperatorId(op) + "_" + route + ".json";
}

String gmbOverrideCachePath(const String& routeCode) {
    return String("/catalog_gmb_") + routeCode + ".json";
}

String tflRailStationsCachePath(const String& line) {
    return String("/tfl_rail_stations_") + line + ".json";
}

bool extractDataArray(const String& wrapped, String& array, String& error) {
    const int start = wrapped.indexOf('[');
    const int end = wrapped.lastIndexOf(']');
    if (start < 0 || end < start) {
        error = "路線更新回應格式不正確";
        return false;
    }
    array = wrapped.substring(start, end + 1);
    return true;
}

bool isSafeRouteCode(const String& value) {
    return value.length() <= 16 && isOfficialBusIdentifier(value.c_str());
}

bool isValidJsonObject(const String& json) {
    StaticJsonDocument<32> filter;
    filter.to<JsonObject>();
    StaticJsonDocument<32> output;
    return !deserializeJson(output, json,
                            DeserializationOption::Filter(filter)) &&
           output.is<JsonObject>();
}

std::string visibleStopLabel(const std::string& input) {
    if (input.size() < 6 || input.back() != ')') return input;
    const std::size_t open = input.rfind(" (");
    if (open == std::string::npos || open + 3 >= input.size()) return input;
    bool hasLetter = false;
    bool hasDigit = false;
    for (std::size_t index = open + 2; index + 1 < input.size(); ++index) {
        const unsigned char value = static_cast<unsigned char>(input[index]);
        if (value >= 'A' && value <= 'Z') {
            hasLetter = true;
        } else if (value >= '0' && value <= '9') {
            hasDigit = true;
        } else {
            return input;
        }
    }
    return hasLetter && hasDigit ? input.substr(0, open) : input;
}

void writeStopsJson(const std::vector<transitink::BusCatalogStop>& stops, String& json) {
    beginList(json);
    bool first = true;
    for (const auto& stop : stops) {
        appendJsonObject(json, first, [&](JsonObject item) {
            item["id"] = stop.stopId.c_str();
            item["label_tc"] = visibleStopLabel(stop.labelTc);
            item["label_en"] = visibleStopLabel(stop.labelEn);
            item["sequence"] = stop.sequence;
        });
    }
    endList(json);
}

std::vector<transitink::BusStopLabel> labelsFromStops(
    const std::vector<transitink::BusCatalogStop>& stops) {
    std::vector<transitink::BusStopLabel> labels;
    labels.reserve(stops.size());
    for (const auto& stop : stops) {
        labels.push_back({stop.stopId, stop.labelTc, stop.labelEn});
    }
    return labels;
}

}  // namespace

bool parseCatalogBusOperator(const String& operatorId, transitink::BusOperator& op) {
    if (operatorId == "kmb") {
        op = transitink::BusOperator::Kmb;
        return true;
    }
    if (operatorId == "lwb") {
        op = transitink::BusOperator::LongWin;
        return true;
    }
    if (operatorId == "ctb") {
        op = transitink::BusOperator::Citybus;
        return true;
    }
    if (operatorId == "tfl") {
        op = transitink::BusOperator::Tfl;
        return true;
    }
    return false;
}

bool parseCatalogRailMode(const String& modeId, transitink::RailMode& mode) {
    return transitink::parseRailModeId(modeId.c_str(), mode);
}

WidgetCatalogService::WidgetCatalogService(KmbClient& kmb,
                                           CitybusClient& citybus,
                                           GmbClient& gmb,
                                           TflClient& tfl)
    : kmb_(kmb), citybus_(citybus), gmb_(gmb), tfl_(tfl) {}

bool WidgetCatalogService::begin() {
    if (fsReady_) {
        return true;
    }
    fsReady_ = LittleFS.begin(true);
    if (fsReady_) {
        String error;
        for (const auto& paths : kCachePaths) {
            if (!recoverCacheFiles(paths.data, paths.temp, paths.backup, error)) {
                fsReady_ = false;
                break;
            }
        }
        loadUpdateMetadata();
    }
    return fsReady_;
}

bool WidgetCatalogService::recoverCacheFiles(const String& dataPath,
                                             const String& tempPath,
                                             const String& backupPath,
                                             String& error) const {
    const auto plan = transitink::planAtomicCacheRecovery(
        LittleFS.exists(dataPath), LittleFS.exists(backupPath), LittleFS.exists(tempPath));
    if (plan.removeTemp) {
        LittleFS.remove(tempPath);
    }
    if (plan.restoreBackup && !LittleFS.rename(backupPath, dataPath)) {
        error = "無法還原目錄快取備份";
        return false;
    }
    error = "";
    return true;
}

bool WidgetCatalogService::readJsonCache(const String& path, String& json) const {
    json = "";
    String recoveryError;
    if (!fsReady_ ||
        !recoverCacheFiles(path, path + ".tmp", path + ".bak", recoveryError)) {
        return false;
    }
    File file = LittleFS.open(path, FILE_READ);
    if (!file || file.size() == 0 ||
        file.size() > transitink::kMaxCatalogResponseBytes) {
        if (file) file.close();
        return false;
    }
    json.reserve(file.size());
    while (file.available()) {
        const int value = file.read();
        if (value < 0) {
            file.close();
            json = "";
            return false;
        }
        json += static_cast<char>(value);
    }
    file.close();
    if (!json.startsWith("{") || !json.endsWith("}") ||
        !isValidJsonObject(json)) {
        json = "";
        LittleFS.remove(path);
        return false;
    }
    return true;
}

bool WidgetCatalogService::writeJsonCache(const String& path,
                                          const String& json,
                                          String& error) const {
    if (!fsReady_ || json.isEmpty() || !isValidJsonObject(json) ||
        json.length() > transitink::kMaxCatalogResponseBytes) {
        error = "目錄快取內容不正確";
        return false;
    }
    const String tempPath = path + ".tmp";
    const String backupPath = path + ".bak";
    LittleFS.remove(tempPath);
    File file = LittleFS.open(tempPath, FILE_WRITE, true);
    if (!file || file.print(json) != json.length()) {
        if (file) file.close();
        LittleFS.remove(tempPath);
        error = "目錄快取寫入失敗";
        return false;
    }
    file.close();
    LittleFS.remove(backupPath);
    const bool hadOld = LittleFS.exists(path);
    if (hadOld && !LittleFS.rename(path, backupPath)) {
        LittleFS.remove(tempPath);
        error = "目錄快取備份失敗";
        return false;
    }
    if (!LittleFS.rename(tempPath, path)) {
        if (hadOld) LittleFS.rename(backupPath, path);
        LittleFS.remove(tempPath);
        error = "目錄快取啟用失敗";
        return false;
    }
    LittleFS.remove(backupPath);
    error = "";
    return true;
}

void WidgetCatalogService::loadUpdateMetadata() {
    lastUpdatedAt_ = 0;
    File file = LittleFS.open(kCatalogUpdateMetadataPath, FILE_READ);
    if (!file || file.size() == 0 || file.size() > 128) {
        if (file) file.close();
        return;
    }
    StaticJsonDocument<128> doc;
    if (!deserializeJson(doc, file)) {
        lastUpdatedAt_ = static_cast<time_t>(doc["updated_at"] | 0LL);
    }
    file.close();
}

void WidgetCatalogService::writeUpdateMetadata() {
    File file = LittleFS.open(kCatalogUpdateMetadataPath, FILE_WRITE, true);
    if (!file) return;
    StaticJsonDocument<128> doc;
    doc["updated_at"] = static_cast<long long>(lastUpdatedAt_);
    serializeJson(doc, file);
    file.close();
}

bool WidgetCatalogService::cacheUsable(CacheKind kind) const {
    const auto& paths = pathsFor(static_cast<uint8_t>(kind));
    File file = LittleFS.open(paths.data, FILE_READ);
    const bool usable = file && file.size() > 0;
    if (file) {
        file.close();
    }
    return usable;
}

bool WidgetCatalogService::downloadCache(CacheKind kind, const char* path, String& error) {
    switch (kind) {
        case CacheKind::KmbRoutes:
            return kmb_.fetchRoutesToFile(LittleFS, path, error);
        case CacheKind::KmbStops:
            return kmb_.fetchStopsToFile(LittleFS, path, error);
        case CacheKind::CitybusRoutes:
            return citybus_.fetchRoutesToFile(LittleFS, path, error);
        case CacheKind::CitybusStops:
            return citybus_.fetchStopsToFile(LittleFS, path, error);
    }
    error = "不支援的目錄快取";
    return false;
}

bool WidgetCatalogService::validateCache(CacheKind kind,
                                         const char* path,
                                         String& error) const {
    File file = LittleFS.open(path, FILE_READ);
    if (!file || file.size() == 0) {
        error = "目錄快取不完整";
        if (file) {
            file.close();
        }
        return false;
    }
    FileCatalogReader reader(file);
    std::string parserError;
    bool valid = false;
    if (kind == CacheKind::KmbRoutes || kind == CacheKind::CitybusRoutes) {
        valid = transitink::validateBusRouteCatalog(
            reader,
            kind == CacheKind::KmbRoutes ? transitink::BusCatalogFormat::Kmb
                                         : transitink::BusCatalogFormat::Citybus,
            parserError);
    } else {
        std::vector<transitink::BusStopLabel> labels;
        valid = transitink::parseMatchingBusStopLabels(reader, {}, labels, parserError);
    }
    file.close();
    if (!valid) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}

void WidgetCatalogService::writeCacheMetadata(CacheKind kind) {
    const time_t now = time(nullptr);
    if (now < kValidEpoch) {
        return;
    }
    File file = LittleFS.open(pathsFor(static_cast<uint8_t>(kind)).metadata, FILE_WRITE, true);
    if (!file) {
        return;
    }
    StaticJsonDocument<128> doc;
    doc["updated_at"] = static_cast<long>(now);
    serializeJson(doc, file);
    file.close();
}

bool WidgetCatalogService::ensureCache(CacheKind kind, bool refresh, String& error) {
    if (!fsReady_) {
        error = "目錄快取檔案系統未啟用";
        return false;
    }
    const bool usable = cacheUsable(kind);
    if (usable && !refresh) {
        error = "";
        return true;
    }

    const uint8_t index = static_cast<uint8_t>(kind);
    const auto& paths = pathsFor(index);
    LittleFS.remove(paths.temp);
    if (!downloadCache(kind, paths.temp, error) || !validateCache(kind, paths.temp, error)) {
        LittleFS.remove(paths.temp);
        if (!refresh && cacheUsable(kind)) {
            return true;
        }
        return false;
    }

    LittleFS.remove(paths.backup);
    const bool hadOld = LittleFS.exists(paths.data);
    if (hadOld && !LittleFS.rename(paths.data, paths.backup)) {
        LittleFS.remove(paths.temp);
        error = "無法備份舊目錄快取";
        return false;
    }
    if (!LittleFS.rename(paths.temp, paths.data)) {
        if (hadOld && !LittleFS.rename(paths.backup, paths.data)) {
            LittleFS.remove(paths.temp);
            error = "無法還原舊目錄快取";
            return false;
        }
        LittleFS.remove(paths.temp);
        error = "無法更新目錄快取";
        return false;
    }
    LittleFS.remove(paths.backup);
    writeCacheMetadata(kind);
    error = "";
    return true;
}

bool WidgetCatalogService::listBusRoutes(transitink::BusOperator op,
                                         bool refresh,
                                         String& json,
                                         String& error) {
    if (op == transitink::BusOperator::Tfl) {
        json = "{\"data\":[]}";
        error = "";
        return true;
    }
    const bool citybus = op == transitink::BusOperator::Citybus;
    const String responseCache = citybus ? kCitybusRouteListCachePath
                                         : kKmbRouteListCachePath;
    if (!refresh && readJsonCache(responseCache, json)) {
        error = "";
        return true;
    }
    const CacheKind kind = citybus ? CacheKind::CitybusRoutes : CacheKind::KmbRoutes;
    if (!ensureCache(kind, refresh, error)) {
        return false;
    }
    File file = LittleFS.open(pathsFor(static_cast<uint8_t>(kind)).data, FILE_READ);
    FileCatalogReader reader(file);
    std::vector<std::string> routeIds;
    std::string parserError;
    if (!transitink::parseBusRouteIds(reader,
                                     citybus ? transitink::BusCatalogFormat::Citybus
                                             : transitink::BusCatalogFormat::Kmb,
                                     routeIds,
                                     parserError)) {
        file.close();
        error = parserError.c_str();
        return false;
    }
    file.close();
    beginList(json);
    bool first = true;
    for (const auto& routeId : routeIds) {
        appendJsonObject(json, first, [&](JsonObject item) {
            item["id"] = routeId.c_str();
            item["label_tc"] = routeId.c_str();
            item["label_en"] = routeId.c_str();
        });
        if (json.length() > transitink::kMaxCatalogResponseBytes) {
            json = "";
            error = "路線目錄回應過大";
            return false;
        }
    }
    endList(json);
    String cacheError;
    writeJsonCache(responseCache, json, cacheError);
    error = "";
    return true;
}

bool WidgetCatalogService::listBusDirections(transitink::BusOperator op,
                                             const String& route,
                                             String& json,
                                             String& error) {
    if (op == transitink::BusOperator::Tfl) {
        std::vector<transitink::BusCatalogRoute> rows;
        if (!tfl_.fetchDirections(route, rows, error)) {
            return false;
        }
        beginList(json);
        bool first = true;
        for (const auto& row : rows) {
            appendJsonObject(json, first, [&](JsonObject item) {
                const std::string id =
                    row.directionId + ":" + row.serviceType;
                const std::string label =
                    row.originLabelTc + " 往 " + row.destinationLabelTc;
                const std::string labelEn =
                    row.originLabelEn + " to " + row.destinationLabelEn;
                item["id"] = id;
                item["label_tc"] = label;
                item["label_en"] = labelEn;
                item["direction_id"] = row.directionId.c_str();
                item["service_type"] = row.serviceType.c_str();
                item["origin_label_tc"] = row.originLabelTc.c_str();
                item["destination_label_tc"] =
                    row.destinationLabelTc.c_str();
                item["origin_label_en"] = row.originLabelEn.c_str();
                item["destination_label_en"] =
                    row.destinationLabelEn.c_str();
            });
        }
        endList(json);
        error = "";
        return true;
    }
    if (!isBusQueryValid(route, "I", "1")) {
        error = "路線查詢參數不正確";
        return false;
    }
    const bool citybus = op == transitink::BusOperator::Citybus;
    const CacheKind kind = citybus ? CacheKind::CitybusRoutes : CacheKind::KmbRoutes;
    if (!ensureCache(kind, false, error)) {
        return false;
    }
    File file = LittleFS.open(pathsFor(static_cast<uint8_t>(kind)).data, FILE_READ);
    FileCatalogReader reader(file);
    std::vector<transitink::BusCatalogRoute> rows;
    std::string parserError;
    if (!transitink::parseBusDirectionsForRoute(
            reader,
            citybus ? transitink::BusCatalogFormat::Citybus
                    : transitink::BusCatalogFormat::Kmb,
            route.c_str(),
            rows,
            parserError)) {
        file.close();
        error = parserError.c_str();
        return false;
    }
    file.close();
    beginList(json);
    bool first = true;
    for (const auto& row : rows) {
        appendJsonObject(json, first, [&](JsonObject item) {
            const std::string id = row.directionId + ":" + row.serviceType;
            const std::string label =
                row.originLabelTc + " 往 " + row.destinationLabelTc;
            const std::string labelEn =
                row.originLabelEn.empty() || row.destinationLabelEn.empty()
                    ? ""
                    : row.originLabelEn + " to " + row.destinationLabelEn;
            item["id"] = id;
            item["label_tc"] = label;
            item["label_en"] = labelEn;
            item["direction_id"] = row.directionId.c_str();
            item["service_type"] = row.serviceType.c_str();
            item["origin_label_tc"] = row.originLabelTc.c_str();
            item["destination_label_tc"] = row.destinationLabelTc.c_str();
            item["origin_label_en"] = row.originLabelEn.c_str();
            item["destination_label_en"] = row.destinationLabelEn.c_str();
        });
        if (json.length() > transitink::kMaxCatalogResponseBytes) {
            json = "";
            error = "路線方向回應過大";
            return false;
        }
    }
    endList(json);
    error = "";
    return true;
}

bool WidgetCatalogService::readCitybusAggregateCache(
    const String& route,
    const String& direction,
    const String& serviceType,
    bool requireFresh,
    std::vector<transitink::BusCatalogStop>& stops,
    String& error) const {
    const String dataPath = citybusAggregateCachePath(route, direction, ".json");
    const String tempPath = citybusAggregateCachePath(route, direction, ".json.tmp");
    const String backupPath = citybusAggregateCachePath(route, direction, ".json.bak");
    if (!recoverCacheFiles(dataPath, tempPath, backupPath, error)) {
        return false;
    }
    if (requireFresh && !fileCacheUsable(dataPath)) {
        error = "城巴站牌快取已過期";
        return false;
    }
    File routeFile = LittleFS.open(dataPath, FILE_READ);
    if (!routeFile) {
        error = "沒有城巴站牌快取";
        return false;
    }
    FileCatalogReader routeReader(routeFile);
    std::string parserError;
    if (!transitink::parseBusRouteStops(routeReader, route.c_str(), direction.c_str(),
                                        serviceType.c_str(), stops, parserError)) {
        routeFile.close();
        error = parserError.c_str();
        return false;
    }
    routeFile.close();
    std::vector<std::string> stopIds;
    stopIds.reserve(stops.size());
    for (const auto& stop : stops) {
        stopIds.push_back(stop.stopId);
    }
    File labelFile = LittleFS.open(dataPath, FILE_READ);
    FileCatalogReader labelReader(labelFile);
    std::vector<transitink::BusStopLabel> labels;
    if (!transitink::parseMatchingBusStopLabels(labelReader, stopIds, labels, parserError) ||
        !transitink::joinBusStopLabels(stops, labels, parserError)) {
        labelFile.close();
        error = parserError.c_str();
        return false;
    }
    labelFile.close();
    error = "";
    return true;
}

bool WidgetCatalogService::writeCitybusAggregateCache(
    const String& route,
    const String& direction,
    const String& serviceType,
    const std::vector<transitink::BusCatalogStop>& stops,
    String& error) {
    if (stops.empty()) {
        error = "城巴站牌快取不可為空";
        return false;
    }
    const String dataPath = citybusAggregateCachePath(route, direction, ".json");
    const String tempPath = citybusAggregateCachePath(route, direction, ".json.tmp");
    const String backupPath = citybusAggregateCachePath(route, direction, ".json.bak");
    const String metadataPath = citybusAggregateCachePath(route, direction, ".meta");
    LittleFS.remove(tempPath);
    File file = LittleFS.open(tempPath, FILE_WRITE, true);
    if (!file || file.print("{\"data\":[") == 0) {
        if (file) {
            file.close();
        }
        error = "無法寫入城巴站牌快取";
        return false;
    }
    bool first = true;
    for (const auto& stop : stops) {
        if (!first && file.print(',') == 0) {
            file.close();
            LittleFS.remove(tempPath);
            error = "城巴站牌快取寫入失敗";
            return false;
        }
        StaticJsonDocument<512> doc;
        doc["route"] = route;
        doc["dir"] = direction;
        doc["service_type"] = serviceType;
        doc["seq"] = stop.sequence;
        doc["stop"] = stop.stopId.c_str();
        doc["name_tc"] = stop.labelTc.c_str();
        doc["name_en"] = stop.labelEn.c_str();
        if (doc.overflowed() || serializeJson(doc, file) == 0) {
            file.close();
            LittleFS.remove(tempPath);
            error = "城巴站牌快取寫入失敗";
            return false;
        }
        first = false;
    }
    if (file.print("]}") == 0) {
        file.close();
        LittleFS.remove(tempPath);
        error = "城巴站牌快取寫入失敗";
        return false;
    }
    file.flush();
    const bool usable = file.size() > 0;
    file.close();
    if (!usable) {
        LittleFS.remove(tempPath);
        error = "城巴站牌快取不完整";
        return false;
    }

    LittleFS.remove(backupPath);
    const bool hadOld = LittleFS.exists(dataPath);
    if (hadOld && !LittleFS.rename(dataPath, backupPath)) {
        LittleFS.remove(tempPath);
        error = "無法備份城巴站牌快取";
        return false;
    }
    if (!LittleFS.rename(tempPath, dataPath)) {
        if (hadOld && !LittleFS.rename(backupPath, dataPath)) {
            LittleFS.remove(tempPath);
            error = "無法還原城巴站牌快取";
            return false;
        }
        LittleFS.remove(tempPath);
        error = "無法更新城巴站牌快取";
        return false;
    }
    LittleFS.remove(backupPath);
    const time_t now = time(nullptr);
    if (now >= kValidEpoch) {
        File metadata = LittleFS.open(metadataPath, FILE_WRITE, true);
        if (metadata) {
            StaticJsonDocument<128> doc;
            doc["updated_at"] = static_cast<long>(now);
            serializeJson(doc, metadata);
            metadata.close();
        }
    }
    error = "";
    return true;
}

bool WidgetCatalogService::listCitybusStops(const String& route,
                                            const String& direction,
                                            const String& serviceType,
                                            bool refresh,
                                            String& json,
                                            String& error) {
    std::vector<transitink::BusCatalogStop> freshStops;
    if (!refresh && readCitybusAggregateCache(route, direction, serviceType,
                                               false, freshStops, error)) {
        writeStopsJson(freshStops, json);
        error = "";
        return true;
    }
    std::vector<transitink::BusCatalogStop> staleStops;
    String staleError;
    readCitybusAggregateCache(route, direction, serviceType, false, staleStops, staleError);

    String routeStopJson;
    if (!citybus_.fetchRouteStopsJson(route, direction, routeStopJson, error)) {
        if (!staleStops.empty()) {
            writeStopsJson(staleStops, json);
            error = "";
            return true;
        }
        return false;
    }
    StringCatalogReader routeReader(routeStopJson);
    std::vector<transitink::BusCatalogStop> routeStops;
    std::string parserError;
    if (!transitink::parseBusRouteStops(routeReader, route.c_str(), direction.c_str(),
                                        serviceType.c_str(), routeStops, parserError)) {
        error = parserError.c_str();
        return false;
    }
    std::vector<std::string> stopIds;
    stopIds.reserve(routeStops.size());
    for (const auto& stop : routeStops) {
        stopIds.push_back(stop.stopId);
    }
    auto decision = transitink::resolveCitybusStopLabels(
        stopIds, {}, labelsFromStops(staleStops), {}, false, false);
    if (decision.resolution == transitink::CitybusStopResolution::Unavailable) {
        error = decision.error.c_str();
        return false;
    }

    std::vector<transitink::BusStopLabel> hydrated;
    const bool hydrationComplete = citybus_.fetchStopLabels(stopIds, hydrated, error);
    decision = transitink::resolveCitybusStopLabels(
        stopIds, {}, labelsFromStops(staleStops), hydrated, true, hydrationComplete);
    if (decision.resolution == transitink::CitybusStopResolution::StaleCache) {
        writeStopsJson(staleStops, json);
        error = "";
        return true;
    }
    if (decision.resolution != transitink::CitybusStopResolution::Hydrated ||
        !transitink::joinBusStopLabels(routeStops, decision.labels, parserError)) {
        error = decision.error.empty() ? parserError.c_str() : decision.error.c_str();
        return false;
    }
    String cacheError;
    writeCitybusAggregateCache(route, direction, serviceType, routeStops, cacheError);
    writeStopsJson(routeStops, json);
    error = "";
    return true;
}

bool WidgetCatalogService::listBusStops(transitink::BusOperator op,
                                        const String& route,
                                        const String& direction,
                                        const String& serviceType,
                                        bool refresh,
                                        String& json,
                                        String& error) {
    if (op == transitink::BusOperator::Tfl) {
        std::vector<transitink::BusCatalogStop> stops;
        if (!tfl_.fetchStops(route, direction, serviceType, stops, error)) {
            return false;
        }
        writeStopsJson(stops, json);
        error = "";
        return true;
    }
    const char* operatorId = transitink::busOperatorId(op);
    std::string validationError;
    auto noFetch = [](void*) { return true; };
    if (!transitink::runValidatedBusCatalogQuery(operatorId, route.c_str(), direction.c_str(),
                                                 serviceType.c_str(), noFetch, nullptr,
                                                 validationError)) {
        error = validationError.c_str();
        return false;
    }
    const bool citybus = op == transitink::BusOperator::Citybus;
    if (citybus) {
        if (serviceType != "1") {
            error = "城巴服務類型只支援 1";
            return false;
        }
        return listCitybusStops(route, direction, serviceType, refresh, json, error);
    }
    const String responseCache = kmbRouteStopCachePath(route, direction, serviceType);
    if (!refresh && readJsonCache(responseCache, json)) {
        error = "";
        return true;
    }
    const CacheKind kind = citybus ? CacheKind::CitybusStops : CacheKind::KmbStops;
    if (!ensureCache(kind, refresh, error)) {
        return false;
    }

    String routeStopJson;
    if (!kmb_.fetchRouteStopsJson(route, direction, serviceType, routeStopJson, error)) {
        return false;
    }
    StringCatalogReader routeReader(routeStopJson);
    std::vector<transitink::BusCatalogStop> stops;
    std::string parserError;
    if (!transitink::parseBusRouteStops(routeReader, route.c_str(), direction.c_str(),
                                        serviceType.c_str(), stops, parserError)) {
        error = parserError.c_str();
        return false;
    }
    std::vector<std::string> stopIds;
    stopIds.reserve(stops.size());
    for (const auto& stop : stops) {
        stopIds.push_back(stop.stopId);
    }
    File stopFile = LittleFS.open(pathsFor(static_cast<uint8_t>(kind)).data, FILE_READ);
    FileCatalogReader stopReader(stopFile);
    std::vector<transitink::BusStopLabel> labels;
    if (!transitink::parseMatchingBusStopLabels(stopReader, stopIds, labels, parserError)) {
        stopFile.close();
        error = parserError.c_str();
        return false;
    }
    stopFile.close();
    if (!transitink::joinBusStopLabels(stops, labels, parserError)) {
        error = parserError.c_str();
        return false;
    }

    writeStopsJson(stops, json);
    String cacheError;
    writeJsonCache(responseCache, json, cacheError);
    error = "";
    return true;
}

bool WidgetCatalogService::listGmbRoutes(bool refresh,
                                         String& json,
                                         String& error) {
    if (!refresh && readJsonCache(kGmbRoutesCachePath, json)) {
        error = "";
        return true;
    }
    const char* regions[] = {"HKI", "KLN", "NT"};
    std::set<std::string> routeCodes;
    for (const char* region : regions) {
        std::vector<std::string> fetched;
        if (!gmb_.fetchRouteCodes(region, fetched, error)) return false;
        routeCodes.insert(fetched.begin(), fetched.end());
    }
    if (routeCodes.empty()) {
        error = "專線小巴路線目錄不可為空";
        return false;
    }
    beginList(json);
    bool first = true;
    for (const auto& routeCode : routeCodes) {
        appendJsonObject(json, first, [&](JsonObject item) {
            item["id"] = routeCode.c_str();
            item["label_tc"] = routeCode.c_str();
            item["label_en"] = routeCode.c_str();
        });
    }
    endList(json);
    if (!writeJsonCache(kGmbRoutesCachePath, json, error)) return false;
    error = "";
    return true;
}

bool WidgetCatalogService::listGmbDirections(const String& routeCode,
                                             bool refresh,
                                             String& json,
                                             String& error) {
    String normalized = routeCode;
    normalized.trim();
    normalized.toUpperCase();
    if (!isSafeRouteCode(normalized)) {
        error = "專線小巴路線代號格式不正確";
        return false;
    }
    const String cachePath = gmbDirectionCachePath(normalized);
    if (!refresh && readJsonCache(cachePath, json)) {
        error = "";
        return true;
    }
    const char* regions[] = {"HKI", "KLN", "NT"};
    std::vector<transitink::GmbCatalogDirection> directions;
    String lastError;
    for (const char* region : regions) {
        std::vector<transitink::GmbCatalogDirection> fetched;
        String fetchError;
        if (gmb_.fetchDirections(region, normalized, fetched, fetchError)) {
            directions.insert(directions.end(), fetched.begin(), fetched.end());
        } else {
            lastError = fetchError;
        }
    }
    std::sort(directions.begin(), directions.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.region, lhs.routeId, lhs.routeSeq) <
               std::tie(rhs.region, rhs.routeId, rhs.routeSeq);
    });
    directions.erase(std::unique(directions.begin(), directions.end(),
                                 [](const auto& lhs, const auto& rhs) {
        return lhs.region == rhs.region && lhs.routeId == rhs.routeId &&
               lhs.routeSeq == rhs.routeSeq;
    }), directions.end());
    if (directions.empty()) {
        error = lastError.isEmpty() ? "找不到專線小巴路線方向" : lastError;
        return false;
    }
    beginList(json);
    bool first = true;
    for (const auto& direction : directions) {
        appendJsonObject(json, first, [&](JsonObject item) {
            const std::string id = direction.routeId + ":" + direction.routeSeq;
            const std::string label = direction.originLabelTc + " 往 " +
                                      direction.destinationLabelTc;
            const std::string labelEn =
                direction.originLabelEn.empty() ||
                        direction.destinationLabelEn.empty()
                    ? ""
                    : direction.originLabelEn + " to " +
                          direction.destinationLabelEn;
            item["id"] = id;
            item["label_tc"] = label;
            item["label_en"] = labelEn;
            item["region"] = direction.region.c_str();
            item["route_id"] = direction.routeId.c_str();
            item["route_seq"] = direction.routeSeq.c_str();
            item["origin_label_tc"] = direction.originLabelTc.c_str();
            item["destination_label_tc"] = direction.destinationLabelTc.c_str();
            item["origin_label_en"] = direction.originLabelEn.c_str();
            item["destination_label_en"] =
                direction.destinationLabelEn.c_str();
        });
    }
    endList(json);
    if (!writeJsonCache(cachePath, json, error)) return false;
    error = "";
    return true;
}

bool WidgetCatalogService::listGmbStops(const String& routeId,
                                        const String& routeSeq,
                                        bool refresh,
                                        String& json,
                                        String& error) {
    if (!isOfficialBusIdentifier(routeId.c_str()) ||
        !isOfficialBusIdentifier(routeSeq.c_str())) {
        error = "專線小巴方向設定不正確";
        return false;
    }
    const String cachePath = gmbStopCachePath(routeId, routeSeq);
    if (!refresh && readJsonCache(cachePath, json)) {
        error = "";
        return true;
    }
    std::vector<transitink::GmbCatalogStop> stops;
    if (!gmb_.fetchStops(routeId, routeSeq, stops, error)) return false;
    if (stops.empty()) {
        error = "專線小巴站點目錄不可為空";
        return false;
    }
    beginList(json);
    bool first = true;
    for (const auto& stop : stops) {
        appendJsonObject(json, first, [&](JsonObject item) {
            item["id"] = stop.stopSeq.c_str();
            item["label_tc"] = visibleStopLabel(stop.labelTc);
            item["label_en"] = visibleStopLabel(stop.labelEn);
            item["stop_id"] = stop.stopId.c_str();
            item["stop_seq"] = stop.stopSeq.c_str();
        });
    }
    endList(json);
    if (!writeJsonCache(cachePath, json, error)) return false;
    error = "";
    return true;
}

bool WidgetCatalogService::listRailLines(transitink::RailMode mode,
                                         String& json,
                                         String& error) {
    std::vector<transitink::StaticCatalogEntry> entries;
    if (mode == transitink::RailMode::LondonRail) {
        if (readJsonCache(kTflRailLinesCachePath, json)) {
            error = "";
            return true;
        }
        if (!tfl_.fetchRailLines(entries, error)) {
            return false;
        }
    } else {
        std::string projectionError;
        if (!transitink::listStaticRailLines(mode, entries,
                                             projectionError)) {
            error = projectionError.c_str();
            return false;
        }
    }
    beginList(json);
    bool first = true;
    for (const auto& entry : entries) {
        appendJsonObject(json, first, [&](JsonObject item) {
            item["id"] = entry.id.c_str();
            item["label_tc"] = entry.labelTc.c_str();
            item["label_en"] = entry.labelEn.c_str();
        });
    }
    endList(json);
    if (mode == transitink::RailMode::LondonRail &&
        !writeJsonCache(kTflRailLinesCachePath, json, error)) {
        return false;
    }
    error = "";
    return true;
}

bool WidgetCatalogService::listRailStations(transitink::RailMode mode,
                                            const String& lineOrRoute,
                                            String& json,
                                            String& error) {
    std::vector<transitink::StaticCatalogEntry> entries;
    if (mode == transitink::RailMode::LondonRail) {
        if (!isOfficialTflIdentifier(lineOrRoute.c_str())) {
            error = "倫敦鐵路路線代號格式不正確";
            return false;
        }
        const String cachePath = tflRailStationsCachePath(lineOrRoute);
        if (readJsonCache(cachePath, json)) {
            error = "";
            return true;
        }
        if (!tfl_.fetchRailStations(lineOrRoute, entries, error)) {
            return false;
        }
    } else {
        std::string projectionError;
        if (!transitink::listStaticRailStations(
                mode, lineOrRoute.c_str(), entries, projectionError)) {
            error = projectionError.c_str();
            return false;
        }
    }
    beginList(json);
    bool first = true;
    for (const auto& entry : entries) {
        appendJsonObject(json, first, [&](JsonObject item) {
            item["id"] = entry.id.c_str();
            item["label_tc"] = entry.labelTc.c_str();
            item["label_en"] = entry.labelEn.c_str();
        });
    }
    endList(json);
    if (mode == transitink::RailMode::LondonRail &&
        !writeJsonCache(tflRailStationsCachePath(lineOrRoute), json, error)) {
        return false;
    }
    error = "";
    return true;
}

bool WidgetCatalogService::listRailDirections(transitink::RailMode mode,
                                              const String& lineOrRoute,
                                              const String& station,
                                              String& json,
                                              String& error) {
    std::vector<transitink::StaticCatalogEntry> entries;
    if (mode == transitink::RailMode::LondonRail) {
        if (!isOfficialTflIdentifier(lineOrRoute.c_str()) ||
            !isOfficialTflIdentifier(station.c_str())) {
            error = "倫敦鐵路方向查詢參數不正確";
            return false;
        }
        entries = {
            {"inbound", "Inbound", "Inbound"},
            {"outbound", "Outbound", "Outbound"},
        };
    } else {
        std::string projectionError;
        if (!transitink::listStaticRailDirections(
                mode, lineOrRoute.c_str(), station.c_str(), entries,
                projectionError)) {
            error = projectionError.c_str();
            return false;
        }
    }
    beginList(json);
    bool first = true;
    for (const auto& entry : entries) {
        appendJsonObject(json, first, [&](JsonObject item) {
            item["id"] = entry.id.c_str();
            item["label_tc"] = entry.labelTc.c_str();
            item["label_en"] = entry.labelEn.c_str();
        });
    }
    endList(json);
    error = "";
    return true;
}

bool WidgetCatalogService::listJourneyLocations(String& json, String& error) {
    std::vector<transitink::StaticCatalogEntry> entries;
    std::string projectionError;
    if (!transitink::listStaticJourneyLocations(entries, projectionError)) {
        error = projectionError.c_str();
        return false;
    }
    beginList(json);
    bool first = true;
    for (const auto& entry : entries) {
        appendJsonObject(json, first, [&](JsonObject item) {
            item["id"] = entry.id.c_str();
            item["label_tc"] = entry.labelTc.c_str();
            item["label_en"] = entry.labelEn.c_str();
        });
    }
    endList(json);
    error = "";
    return true;
}

bool WidgetCatalogService::listJourneyDestinations(const String& locationId,
                                                   String& json,
                                                   String& error) {
    std::vector<transitink::StaticCatalogEntry> entries;
    std::string projectionError;
    if (!transitink::listStaticJourneyDestinations(
            locationId.c_str(), entries, projectionError)) {
        error = projectionError.c_str();
        return false;
    }
    beginList(json);
    bool first = true;
    for (const auto& entry : entries) {
        appendJsonObject(json, first, [&](JsonObject item) {
            item["id"] = entry.id.c_str();
            item["label_tc"] = entry.labelTc.c_str();
            item["label_en"] = entry.labelEn.c_str();
        });
    }
    endList(json);
    error = "";
    return true;
}

bool WidgetCatalogService::readBusRouteOverride(transitink::BusOperator op,
                                                const String& route,
                                                String& json,
                                                String& error) const {
    String normalized = route;
    normalized.trim();
    normalized.toUpperCase();
    if (!isSafeRouteCode(normalized)) {
        error = "巴士路線代號格式不正確";
        return false;
    }
    if (!readJsonCache(busOverrideCachePath(op, normalized), json)) {
        error = "not_found";
        return false;
    }
    error = "";
    return true;
}

bool WidgetCatalogService::readGmbRouteOverride(const String& routeCode,
                                                String& json,
                                                String& error) const {
    String normalized = routeCode;
    normalized.trim();
    normalized.toUpperCase();
    if (!isSafeRouteCode(normalized)) {
        error = "專線小巴路線代號格式不正確";
        return false;
    }
    if (!readJsonCache(gmbOverrideCachePath(normalized), json)) {
        error = "not_found";
        return false;
    }
    error = "";
    return true;
}

bool WidgetCatalogService::readUpdatedRouteIndex(String& json, String& error) const {
    if (!readJsonCache(kUpdatedRouteIndexPath, json)) {
        error = "not_found";
        return false;
    }
    error = "";
    return true;
}

bool WidgetCatalogService::refreshRouteIndex(String& json, String& error) {
    String kmbRoutes;
    String citybusRoutes;
    String gmbRoutes;
    if (!listBusRoutes(transitink::BusOperator::Kmb, true, kmbRoutes, error) ||
        !listBusRoutes(transitink::BusOperator::Citybus, true, citybusRoutes, error) ||
        !listGmbRoutes(true, gmbRoutes, error)) {
        return false;
    }

    String kmbArray;
    String citybusArray;
    String gmbArray;
    if (!extractDataArray(kmbRoutes, kmbArray, error) ||
        !extractDataArray(citybusRoutes, citybusArray, error) ||
        !extractDataArray(gmbRoutes, gmbArray, error)) {
        return false;
    }
    const time_t now = time(nullptr);
    lastUpdatedAt_ = now >= kValidEpoch ? now : lastUpdatedAt_;
    json = String("{\"updated_at\":") +
           String(static_cast<long long>(lastUpdatedAt_)) +
           ",\"bus\":{\"kmb\":" + kmbArray +
           ",\"ctb\":" + citybusArray + "},\"gmb\":" + gmbArray + "}";
    if (!writeJsonCache(kUpdatedRouteIndexPath, json, error)) {
        return false;
    }
    writeUpdateMetadata();
    error = "";
    return true;
}

bool WidgetCatalogService::refreshBusRoute(transitink::BusOperator op,
                                           const String& route,
                                           bool refreshRouteList,
                                           bool refreshSharedStops,
                                           String& json,
                                           String& error) {
    String normalized = route;
    normalized.trim();
    normalized.toUpperCase();
    if (!isSafeRouteCode(normalized)) {
        error = "巴士路線代號格式不正確";
        return false;
    }

    if (refreshRouteList && op != transitink::BusOperator::Tfl) {
        String ignoredRoutes;
        if (!listBusRoutes(op, true, ignoredRoutes, error)) {
            return false;
        }
    }
    String directionsJson;
    if (!listBusDirections(op, normalized, directionsJson, error)) {
        return false;
    }
    DynamicJsonDocument directionsDoc(12288);
    const DeserializationError parseError = deserializeJson(directionsDoc, directionsJson);
    JsonArrayConst directions = directionsDoc["data"].as<JsonArrayConst>();
    if (parseError || directions.isNull() || directions.size() == 0) {
        error = "官方服務找不到此巴士路線方向";
        return false;
    }

    String directionsArray;
    serializeJson(directions, directionsArray);
    json = String("{\"kind\":\"bus\",\"operator\":\"") +
           transitink::busOperatorId(op) + "\",\"route\":\"" + normalized +
           "\",\"directions\":" + directionsArray + ",\"stops\":{";
    bool first = true;
    bool refreshKmbStopLabels = refreshSharedStops;
    for (JsonObjectConst direction : directions) {
        const String directionId = direction["direction_id"] | "";
        const String serviceType = direction["service_type"] | "";
        String stopsJson;
        const bool forceRefresh = op == transitink::BusOperator::Citybus ||
                                  op == transitink::BusOperator::Tfl ||
                                  refreshKmbStopLabels;
        if (op != transitink::BusOperator::Citybus &&
            op != transitink::BusOperator::Tfl && !forceRefresh) {
            LittleFS.remove(kmbRouteStopCachePath(normalized, directionId, serviceType));
        }
        if (!listBusStops(op, normalized, directionId, serviceType,
                          forceRefresh, stopsJson, error)) {
            return false;
        }
        refreshKmbStopLabels = false;
        String stopsArray;
        if (!extractDataArray(stopsJson, stopsArray, error)) {
            return false;
        }
        if (!first) json += ',';
        json += "\"" + directionId + ":" + serviceType + "\":" + stopsArray;
        first = false;
        if (json.length() > transitink::kMaxCatalogResponseBytes) {
            error = "此巴士路線的站牌資料過大";
            return false;
        }
    }
    const time_t now = time(nullptr);
    lastUpdatedAt_ = now >= kValidEpoch ? now : lastUpdatedAt_;
    json += "},\"updated_at\":" + String(static_cast<long long>(lastUpdatedAt_)) + "}";
    if (!writeJsonCache(busOverrideCachePath(op, normalized), json, error)) {
        return false;
    }
    writeUpdateMetadata();
    error = "";
    return true;
}

bool WidgetCatalogService::refreshGmbRoute(const String& routeCode,
                                           String& json,
                                           String& error) {
    String normalized = routeCode;
    normalized.trim();
    normalized.toUpperCase();
    if (!isSafeRouteCode(normalized)) {
        error = "專線小巴路線代號格式不正確";
        return false;
    }
    String directionsJson;
    if (!listGmbDirections(normalized, true, directionsJson, error)) {
        return false;
    }
    DynamicJsonDocument directionsDoc(12288);
    const DeserializationError parseError = deserializeJson(directionsDoc, directionsJson);
    JsonArrayConst directions = directionsDoc["data"].as<JsonArrayConst>();
    if (parseError || directions.isNull() || directions.size() == 0) {
        error = "官方服務找不到此專線小巴路線方向";
        return false;
    }
    String directionsArray;
    serializeJson(directions, directionsArray);
    json = String("{\"kind\":\"gmb\",\"route\":\"") + normalized +
           "\",\"directions\":" + directionsArray + ",\"stops\":{";
    bool first = true;
    for (JsonObjectConst direction : directions) {
        const String routeId = direction["route_id"] | "";
        const String routeSeq = direction["route_seq"] | "";
        String stopsJson;
        if (!listGmbStops(routeId, routeSeq, true, stopsJson, error)) {
            return false;
        }
        String stopsArray;
        if (!extractDataArray(stopsJson, stopsArray, error)) {
            return false;
        }
        if (!first) json += ',';
        json += "\"" + routeId + ":" + routeSeq + "\":" + stopsArray;
        first = false;
        if (json.length() > transitink::kMaxCatalogResponseBytes) {
            error = "此專線小巴路線的站牌資料過大";
            return false;
        }
    }
    const time_t now = time(nullptr);
    lastUpdatedAt_ = now >= kValidEpoch ? now : lastUpdatedAt_;
    json += "},\"updated_at\":" + String(static_cast<long long>(lastUpdatedAt_)) + "}";
    if (!writeJsonCache(gmbOverrideCachePath(normalized), json, error)) {
        return false;
    }
    writeUpdateMetadata();
    error = "";
    return true;
}

bool WidgetCatalogService::appendStatus(String& configJson, String& error) const {
    DynamicJsonDocument doc(16384);
    const DeserializationError jsonError = deserializeJson(doc, configJson);
    if (jsonError || doc.overflowed() || !doc.is<JsonObject>()) {
        error = "設定 JSON 無法加入交通目錄狀態";
        return false;
    }
    const time_t now = time(nullptr);
    JsonObject catalog = doc.createNestedObject("catalog");
    std::size_t embeddedBytes = 0;
    for (std::size_t index = 0; index < transitink::kEmbeddedCatalogAssetCount; ++index) {
        embeddedBytes += transitink::kEmbeddedCatalogAssets[index].size;
    }
    catalog["source"] = "firmware_baseline";
    catalog["revision"] = transitink::kEmbeddedCatalogRevision;
    catalog["generated_at"] = transitink::kEmbeddedCatalogGeneratedAt;
    catalog["bytes"] = embeddedBytes;
    catalog["override_bytes"] = fsReady_ ? LittleFS.usedBytes() : 0;
    catalog["last_checked_at"] = static_cast<long long>(lastUpdatedAt_);
    catalog["stale"] = lastUpdatedAt_ > 0 && now >= kValidEpoch &&
                       now - lastUpdatedAt_ > 90L * 24L * 60L * 60L;
    catalog["update_available"] = fsReady_;
    configJson = "";
    if (serializeJson(doc, configJson) == 0) {
        error = "設定 JSON 重新編碼失敗";
        return false;
    }
    error = "";
    return true;
}
