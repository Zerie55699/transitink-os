#include "TflClient.h"

#include "TransitJsonParsers.h"
#include "TransitTlsTrust.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <string>

#include "core/CatalogCore.h"

namespace {

constexpr const char* kTflApiBase = "https://api.tfl.gov.uk";
constexpr std::size_t kMaxTflSequenceDownloadBytes = 1024 * 1024;
constexpr std::size_t kMaxTflFilteredSequenceBytes = 64 * 1024;
constexpr std::size_t kMaxTflFilteredRailStationsBytes = 32 * 1024;

class BoundedBodyStream final : public Stream {
public:
    explicit BoundedBodyStream(std::size_t limit) : accumulator_(limit) {}
    std::size_t write(uint8_t byte) override { return write(&byte, 1); }
    std::size_t write(const uint8_t* bytes, std::size_t length) override {
        if (!accumulator_.append(bytes, length, error_)) {
            return 0;
        }
        return length;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    const transitink::BoundedBodyAccumulator& accumulator() const {
        return accumulator_;
    }
    const std::string& error() const { return error_; }

private:
    transitink::BoundedBodyAccumulator accumulator_;
    std::string error_;
};

bool splitServiceType(const String& value, String& originator,
                      String& destination) {
    const int separator = value.indexOf('|');
    if (separator <= 0 || separator != value.lastIndexOf('|') ||
        separator >= static_cast<int>(value.length()) - 1) {
        return false;
    }
    originator = value.substring(0, separator);
    destination = value.substring(separator + 1);
    return isOfficialBusIdentifier(originator.c_str()) &&
           isOfficialBusIdentifier(destination.c_str());
}

}  // namespace

bool TflClient::httpGetBounded(const String& url,
                               std::size_t limit,
                               String& body,
                               String& error) {
    body = "";
    WiFiClientSecure tls;
    transitink::configureTflVerifiedTls(tls);
    HTTPClient http;
    http.setTimeout(15000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法建立 TfL HTTPS 連線";
        return false;
    }
    http.addHeader("Accept-Encoding", "identity");
    const int code = http.GET();
    const int expectedLength = http.getSize();
    if (code != HTTP_CODE_OK ||
        expectedLength > static_cast<int>(limit)) {
        error = code == HTTP_CODE_OK ? "TfL 回應過大"
                                     : String("TfL HTTP 錯誤: ") + code;
        http.end();
        return false;
    }

    BoundedBodyStream sink(limit);
    const int written = http.writeToStream(&sink);
    std::string completionError;
    const bool complete =
        sink.accumulator().complete(expectedLength, completionError);
    http.end();
    if (written < 0 || !sink.error().empty() || !complete) {
        const std::string& detail =
            !sink.error().empty() ? sink.error() : completionError;
        error = detail.empty() ? "TfL 回應不完整" : detail.c_str();
        return false;
    }
    body = sink.accumulator().body().c_str();
    error = "";
    return true;
}

bool TflClient::fetchFilteredRouteSequence(const String& url,
                                           String& body,
                                           String& error) {
    body = "";
    WiFiClientSecure tls;
    transitink::configureTflVerifiedTls(tls);
    HTTPClient http;
    // getStream() exposes HTTP/1.1 chunk framing. Request a connection-close
    // response so ArduinoJson receives the JSON body rather than a chunk size.
    http.useHTTP10(true);
    http.setTimeout(20000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法建立 TfL HTTPS 連線";
        return false;
    }
    http.addHeader("Accept-Encoding", "identity");
    const int code = http.GET();
    const int expectedLength = http.getSize();
    if (code != HTTP_CODE_OK ||
        expectedLength > static_cast<int>(kMaxTflSequenceDownloadBytes)) {
        error = code == HTTP_CODE_OK ? "TfL 路線站牌回應過大"
                                     : String("TfL HTTP 錯誤: ") + code;
        http.end();
        return false;
    }

    StaticJsonDocument<512> filter;
    filter["orderedLineRoutes"][0]["naptanIds"][0] = true;
    filter["stopPointSequences"][0]["stopPoint"][0]["id"] = true;
    filter["stopPointSequences"][0]["stopPoint"][0]["name"] = true;
    if (filter.overflowed()) {
        http.end();
        error = "TfL 路線站牌篩選設定錯誤";
        return false;
    }
    DynamicJsonDocument document(kMaxTflFilteredSequenceBytes);
    const DeserializationError jsonError = deserializeJson(
        document, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (jsonError) {
        error = String("TfL 路線站牌資料無法解析: ") + jsonError.c_str();
        return false;
    }
    if (document.overflowed()) {
        error = "TfL 路線站牌資料超出記憶體限制";
        return false;
    }
    serializeJson(document, body);
    if (body.isEmpty() ||
        body.length() > kMaxTflFilteredSequenceBytes) {
        body = "";
        error = "TfL 路線站牌資料過大";
        return false;
    }
    error = "";
    return true;
}

bool TflClient::fetchFilteredRailStations(const String& url,
                                          String& body,
                                          String& error) {
    body = "";
    WiFiClientSecure tls;
    transitink::configureTflVerifiedTls(tls);
    HTTPClient http;
    http.useHTTP10(true);
    http.setTimeout(20000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法建立 TfL HTTPS 連線";
        return false;
    }
    http.addHeader("Accept-Encoding", "identity");
    const int code = http.GET();
    const int expectedLength = http.getSize();
    if (code != HTTP_CODE_OK ||
        expectedLength > static_cast<int>(kMaxTflSequenceDownloadBytes)) {
        error = code == HTTP_CODE_OK ? "TfL 鐵路車站回應過大"
                                     : String("TfL HTTP 錯誤: ") + code;
        http.end();
        return false;
    }

    StaticJsonDocument<192> filter;
    filter[0]["id"] = true;
    filter[0]["commonName"] = true;
    DynamicJsonDocument document(kMaxTflFilteredRailStationsBytes);
    const DeserializationError jsonError = deserializeJson(
        document, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (jsonError) {
        error = String("TfL 鐵路車站資料無法解析: ") + jsonError.c_str();
        return false;
    }
    if (document.overflowed()) {
        error = "TfL 鐵路車站資料超出記憶體限制";
        return false;
    }
    serializeJson(document, body);
    if (body.isEmpty() ||
        body.length() > kMaxTflFilteredRailStationsBytes) {
        body = "";
        error = "TfL 鐵路車站資料過大";
        return false;
    }
    error = "";
    return true;
}

bool TflClient::fetchDirections(
    const String& route,
    std::vector<transitink::BusCatalogRoute>& directions,
    String& error) {
    directions.clear();
    if (!isOfficialBusIdentifier(route.c_str())) {
        error = "倫敦巴士路線代號格式不正確";
        return false;
    }
    String body;
    if (!httpGetBounded(String(kTflApiBase) + "/Line/" + route + "/Route",
                        transitink::kMaxCatalogResponseBytes, body, error)) {
        return false;
    }
    std::string parserError;
    if (!parseTflDirectionsJson(body.c_str(), route.c_str(), directions,
                                parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}

bool TflClient::fetchStops(
    const String& route,
    const String& direction,
    const String& serviceType,
    std::vector<transitink::BusCatalogStop>& stops,
    String& error) {
    stops.clear();
    String originator;
    String destination;
    if (!isOfficialBusIdentifier(route.c_str()) ||
        (direction != "inbound" && direction != "outbound") ||
        !splitServiceType(serviceType, originator, destination)) {
        error = "倫敦巴士站牌查詢參數不正確";
        return false;
    }
    String body;
    if (!fetchFilteredRouteSequence(
            String(kTflApiBase) + "/Line/" + route + "/Route/Sequence/" +
                direction,
            body, error)) {
        return false;
    }
    std::string parserError;
    if (!parseTflRouteSequenceJson(body.c_str(), route.c_str(),
                                   direction.c_str(), serviceType.c_str(),
                                   stops, parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}

bool TflClient::fetchEtaRecords(
    const transitink::BusWidgetConfig& config,
    int64_t nowEpoch,
    std::vector<transitink::BusEtaRecord>& records,
    String& error) {
    records.clear();
    if (config.operatorId != transitink::BusOperator::Tfl ||
        !isOfficialBusIdentifier(config.routeId) ||
        !isOfficialBusIdentifier(config.stopId) ||
        (config.directionId != "inbound" &&
         config.directionId != "outbound")) {
        error = "倫敦巴士設定不正確";
        return false;
    }
    String body;
    if (!httpGetBounded(String(kTflApiBase) + "/StopPoint/" +
                            config.stopId.c_str() + "/Arrivals",
                        transitink::kMaxCatalogResponseBytes, body, error)) {
        return false;
    }
    std::string parserError;
    if (!parseTflEtaJson(body.c_str(), config, nowEpoch, records,
                         parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}

bool TflClient::fetchRailLines(
    std::vector<transitink::StaticCatalogEntry>& lines,
    String& error) {
    lines.clear();
    String body;
    if (!httpGetBounded(
            String(kTflApiBase) +
                "/Line/Mode/tube,dlr,overground,elizabeth-line,tram",
            transitink::kMaxCatalogResponseBytes, body, error)) {
        return false;
    }
    std::string parserError;
    if (!parseTflRailLinesJson(body.c_str(), lines, parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}

bool TflClient::fetchRailStations(
    const String& line,
    std::vector<transitink::StaticCatalogEntry>& stations,
    String& error) {
    stations.clear();
    if (!isOfficialTflIdentifier(line.c_str())) {
        error = "倫敦鐵路路線代號格式不正確";
        return false;
    }
    String body;
    if (!fetchFilteredRailStations(
            String(kTflApiBase) + "/Line/" + line + "/StopPoints",
            body, error)) {
        return false;
    }
    std::string parserError;
    if (!parseTflRailStationsJson(body.c_str(), stations, parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}

bool TflClient::fetchRailArrivals(
    const transitink::MtrWidgetConfig& config,
    int64_t nowEpoch,
    std::vector<transitink::RailArrivalRecord>& records,
    String& error) {
    records.clear();
    if (config.mode != transitink::RailMode::LondonRail ||
        !isOfficialTflIdentifier(config.lineOrRouteId) ||
        !isOfficialTflIdentifier(config.stationId) ||
        (config.directionId != "inbound" &&
         config.directionId != "outbound")) {
        error = "倫敦鐵路設定不正確";
        return false;
    }
    String body;
    if (!httpGetBounded(String(kTflApiBase) + "/StopPoint/" +
                            config.stationId.c_str() + "/Arrivals",
                        transitink::kMaxCatalogResponseBytes, body, error)) {
        return false;
    }
    std::string parserError;
    if (!parseTflRailArrivalsJson(body.c_str(), config, nowEpoch, records,
                                  parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}
