#include "GmbClient.h"
#include "TransitTlsTrust.h"

#include "core/CatalogCore.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace {

constexpr const char* kBaseUrl = "https://data.etagmb.gov.hk";
constexpr std::size_t kCatalogResponseBytes = 65536;
constexpr std::size_t kEtaResponseBytes = 8192;

class BoundedBodyStream final : public Stream {
public:
    explicit BoundedBodyStream(std::size_t limit) : accumulator_(limit) {}
    std::size_t write(uint8_t byte) override { return write(&byte, 1); }
    std::size_t write(const uint8_t* bytes, std::size_t length) override {
        if (!accumulator_.append(bytes, length, error_)) return 0;
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

bool isDigits(const String& value) {
    if (value.length() == 0) return false;
    for (std::size_t index = 0; index < value.length(); ++index) {
        if (value[index] < '0' || value[index] > '9') return false;
    }
    return true;
}

}  // namespace

bool GmbClient::httpGetBounded(const String& url,
                               std::size_t limit,
                               String& body,
                               String& error) {
    body = "";
    WiFiClientSecure tls;
    transitink::configureVerifiedTls(tls);
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法建立專線小巴 HTTPS 連線";
        return false;
    }
    http.addHeader("Accept-Encoding", "identity");
    const int code = http.GET();
    const int expectedLength = http.getSize();
    if (code != HTTP_CODE_OK || expectedLength > static_cast<int>(limit)) {
        error = code == HTTP_CODE_OK ? "專線小巴回應過大"
                                     : String("HTTP 錯誤: ") + code;
        http.end();
        return false;
    }

    BoundedBodyStream sink(limit);
    const int written = http.writeToStream(&sink);
    std::string completionError;
    const bool complete = sink.accumulator().complete(expectedLength,
                                                        completionError);
    http.end();
    if (written < 0 || !sink.error().empty() || !complete) {
        const std::string& detail = !sink.error().empty() ? sink.error()
                                                          : completionError;
        error = detail.empty() ? "專線小巴回應不完整" : detail.c_str();
        return false;
    }
    body = sink.accumulator().body().c_str();
    error = "";
    return true;
}

bool GmbClient::fetchRouteCodes(const String& region,
                                std::vector<std::string>& routeCodes,
                                String& error) {
    if (!transitink::isGmbRegionId(region.c_str())) {
        routeCodes.clear();
        error = "專線小巴地區設定不正確";
        return false;
    }
    String body;
    if (!httpGetBounded(String(kBaseUrl) + "/route/" + region,
                        kCatalogResponseBytes, body, error)) {
        routeCodes.clear();
        return false;
    }
    std::string parserError;
    if (!parseGmbRouteCodesJson(body.c_str(), region.c_str(), routeCodes,
                                parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}

bool GmbClient::fetchDirections(
    const String& region,
    const String& routeCode,
    std::vector<transitink::GmbCatalogDirection>& directions,
    String& error) {
    if (!transitink::isGmbRegionId(region.c_str()) ||
        !isOfficialBusIdentifier(routeCode.c_str())) {
        directions.clear();
        error = "專線小巴路線設定不正確";
        return false;
    }
    String body;
    if (!httpGetBounded(String(kBaseUrl) + "/route/" + region + "/" +
                            routeCode,
                        kCatalogResponseBytes, body, error)) {
        directions.clear();
        return false;
    }
    std::string parserError;
    if (!parseGmbDirectionsJson(body.c_str(), region.c_str(), routeCode.c_str(),
                                directions, parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}

bool GmbClient::fetchStops(const String& routeId,
                           const String& routeSeq,
                           std::vector<transitink::GmbCatalogStop>& stops,
                           String& error) {
    if (!isDigits(routeId) || !isDigits(routeSeq)) {
        stops.clear();
        error = "專線小巴方向設定不正確";
        return false;
    }
    String body;
    if (!httpGetBounded(String(kBaseUrl) + "/route-stop/" + routeId + "/" +
                            routeSeq,
                        kCatalogResponseBytes, body, error)) {
        stops.clear();
        return false;
    }
    std::string parserError;
    if (!parseGmbStopsJson(body.c_str(), routeId.c_str(), routeSeq.c_str(),
                           stops, parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}

bool GmbClient::fetchEta(const transitink::GmbWidgetConfig& config,
                         transitink::GmbEtaPayload& payload,
                         String& error) {
    transitink::WidgetConfig widget;
    widget.type = transitink::WidgetType::GmbEta;
    widget.gmb = config;
    if (!transitink::isWidgetConfigValid(widget)) {
        payload = {};
        error = "專線小巴設定不完整";
        return false;
    }
    String body;
    const String url = String(kBaseUrl) + "/eta/route-stop/" +
                       config.routeId.c_str() + "/" +
                       config.routeSeq.c_str() + "/" +
                       config.stopSeq.c_str();
    if (!httpGetBounded(url, kEtaResponseBytes, body, error)) {
        payload = {};
        return false;
    }
    std::string parserError;
    if (!parseGmbEtaJson(body.c_str(), config, payload, parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}
