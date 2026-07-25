#include "CitybusClient.h"
#include "TransitTlsTrust.h"

#include "TransitJsonParsers.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace {

constexpr const char* kCitybusBase =
    "https://rt.data.gov.hk/v2/transport/citybus";

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
    const transitink::BoundedBodyAccumulator& accumulator() const { return accumulator_; }
    const std::string& error() const { return error_; }

private:
    transitink::BoundedBodyAccumulator accumulator_;
    std::string error_;
};

}  // namespace

bool CitybusClient::httpGet(const String& url, String& body, String& error) {
    WiFiClientSecure tls;
    transitink::configureVerifiedTls(tls);
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法連接城巴資料服務";
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = String("城巴資料服務回應錯誤：") + code;
        http.end();
        return false;
    }
    body = http.getString();
    http.end();
    error = "";
    return true;
}

bool CitybusClient::httpGetBounded(const String& url,
                                   std::size_t limit,
                                   String& body,
                                   String& error) {
    WiFiClientSecure tls;
    transitink::configureVerifiedTls(tls);
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法連接城巴資料服務";
        return false;
    }
    http.addHeader("Accept-Encoding", "identity");
    const int code = http.GET();
    const int expectedLength = http.getSize();
    if (code != HTTP_CODE_OK || expectedLength > static_cast<int>(limit)) {
        error = code == HTTP_CODE_OK ? "城巴路線站牌回應過大"
                                     : String("城巴資料服務回應錯誤：") + code;
        http.end();
        return false;
    }
    BoundedBodyStream sink(limit);
    const int written = http.writeToStream(&sink);
    std::string completionError;
    const bool complete = sink.accumulator().complete(expectedLength, completionError);
    http.end();
    if (written < 0 || !sink.error().empty() || !complete) {
        const std::string& detail = !sink.error().empty() ? sink.error() : completionError;
        error = detail.empty() ? "城巴路線站牌回應不完整" : detail.c_str();
        return false;
    }
    body = sink.accumulator().body().c_str();
    error = "";
    return true;
}

bool CitybusClient::httpGetToFile(const String& url,
                                  fs::FS& fs,
                                  const char* path,
                                  String& error) {
    WiFiClientSecure tls;
    transitink::configureVerifiedTls(tls);
    HTTPClient http;
    http.setTimeout(20000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法連接城巴資料服務";
        return false;
    }
    http.addHeader("Accept-Encoding", "identity");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = String("城巴資料服務回應錯誤：") + code;
        http.end();
        return false;
    }

    File file = fs.open(path, FILE_WRITE, true);
    if (!file) {
        error = "無法寫入城巴目錄快取";
        http.end();
        return false;
    }
    const int expectedLength = http.getSize();
    if (expectedLength > static_cast<int>(transitink::kMaxCatalogDownloadBytes)) {
        error = "城巴目錄回應過大";
        file.close();
        http.end();
        return false;
    }
    auto* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    std::size_t copied = 0;
    unsigned long lastDataAt = millis();
    while (http.connected() &&
           (expectedLength < 0 || copied < static_cast<std::size_t>(expectedLength))) {
        std::size_t available = stream->available();
        if (available == 0) {
            if (millis() - lastDataAt > 20000) {
                break;
            }
            delay(10);
            continue;
        }
        const std::size_t toRead = available < sizeof(buffer) ? available : sizeof(buffer);
        const int readLength = stream->readBytes(buffer, toRead);
        if (readLength <= 0) {
            continue;
        }
        if (copied + static_cast<std::size_t>(readLength) >
            transitink::kMaxCatalogDownloadBytes) {
            error = "城巴目錄回應過大";
            file.close();
            http.end();
            return false;
        }
        if (file.write(buffer, readLength) != static_cast<std::size_t>(readLength)) {
            error = "城巴目錄快取寫入失敗";
            file.close();
            http.end();
            return false;
        }
        copied += static_cast<std::size_t>(readLength);
        lastDataAt = millis();
        delay(0);
    }
    file.flush();
    const std::size_t written = file.size();
    file.close();
    http.end();
    if (written == 0 ||
        (expectedLength > 0 && copied < static_cast<std::size_t>(expectedLength))) {
        error = "城巴目錄快取未完成";
        return false;
    }
    error = "";
    return true;
}

bool CitybusClient::fetchRoutesJson(String& body, String& error) {
    return httpGet(String(kCitybusBase) + "/route/CTB", body, error);
}

bool CitybusClient::fetchRoutesToFile(fs::FS& fs, const char* path, String& error) {
    return httpGetToFile(String(kCitybusBase) + "/route/CTB", fs, path, error);
}

bool CitybusClient::fetchStopsToFile(fs::FS& fs, const char* path, String& error) {
    return httpGetToFile(String(kCitybusBase) + "/stop", fs, path, error);
}

bool CitybusClient::fetchStopJson(const String& stopId,
                                  String& body,
                                  String& error) {
    if (!isOfficialBusIdentifier(stopId.c_str())) {
        error = "城巴巴士站編號格式不正確";
        return false;
    }
    return httpGet(String(kCitybusBase) + "/stop/" + stopId, body, error);
}

bool CitybusClient::fetchRouteStopsJson(const String& route,
                                        const String& direction,
                                        String& body,
                                        String& error) {
    if (!isOfficialBusIdentifier(route.c_str())) {
        error = "城巴路線編號格式不正確";
        return false;
    }
    std::string path;
    if (!mapCitybusDirectionPath(direction.c_str(), path)) {
        error = "城巴路線方向設定不正確";
        return false;
    }
    return httpGetBounded(String(kCitybusBase) + "/route-stop/CTB/" + route + "/" +
                              path.c_str(),
                          transitink::kMaxRouteStopResponseBytes, body, error);
}

bool CitybusClient::fetchStopLabels(
    const std::vector<std::string>& stopIds,
    std::vector<transitink::BusStopLabel>& labels,
    String& error) {
    labels.clear();
    if (stopIds.size() > transitink::kMaxCitybusRouteStops) {
        error = "城巴路線站數超出安全上限";
        return false;
    }

    WiFiClientSecure tls;
    transitink::configureVerifiedTls(tls);
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(true);
    bool complete = true;
    String lastError;
    for (const auto& stopId : stopIds) {
        if (!isOfficialBusIdentifier(stopId)) {
            complete = false;
            lastError = "城巴巴士站編號格式不正確";
            continue;
        }
        const String url = String(kCitybusBase) + "/stop/" + stopId.c_str();
        if (!http.begin(tls, url)) {
            complete = false;
            lastError = "無法連接城巴資料服務";
            continue;
        }
        const int code = http.GET();
        const int expectedLength = http.getSize();
        if (code != HTTP_CODE_OK || expectedLength > static_cast<int>(transitink::kMaxCatalogObjectBytes)) {
            complete = false;
            lastError = code == HTTP_CODE_OK ? "城巴站名回應過大" : "城巴站名服務回應錯誤";
            http.end();
            continue;
        }

        uint8_t body[transitink::kMaxCatalogObjectBytes + 1];
        std::size_t received = 0;
        unsigned long lastDataAt = millis();
        auto* stream = http.getStreamPtr();
        bool overflow = false;
        while (http.connected() &&
               (expectedLength < 0 || received < static_cast<std::size_t>(expectedLength))) {
            const std::size_t available = stream->available();
            if (available == 0) {
                if (millis() - lastDataAt > 10000) {
                    break;
                }
                delay(5);
                continue;
            }
            std::size_t toRead = available;
            if (toRead > transitink::kMaxCatalogObjectBytes - received) {
                overflow = true;
                break;
            }
            const int readLength = stream->readBytes(body + received, toRead);
            if (readLength > 0) {
                received += static_cast<std::size_t>(readLength);
                lastDataAt = millis();
            }
        }
        http.end();
        if (overflow || received == 0 ||
            (expectedLength > 0 && received < static_cast<std::size_t>(expectedLength))) {
            complete = false;
            lastError = overflow ? "城巴站名回應過大" : "城巴站名回應不完整";
            continue;
        }
        body[received] = '\0';
        StaticJsonDocument<transitink::kMaxCatalogObjectBytes> doc;
        const DeserializationError jsonError = deserializeJson(doc, body, received);
        JsonObjectConst data = doc["data"].as<JsonObjectConst>();
        if (jsonError || doc.overflowed() || data.isNull() ||
            !data["stop"].is<const char*>() || !data["name_tc"].is<const char*>()) {
            complete = false;
            lastError = "城巴站名 JSON 格式錯誤";
            continue;
        }
        const std::string returnedStop = data["stop"].as<const char*>();
        const std::string label = data["name_tc"].as<const char*>();
        const std::string labelEn =
            data["name_en"].is<const char*>()
                ? data["name_en"].as<const char*>()
                : "";
        if (returnedStop != stopId || label.empty() || label.size() > 96) {
            complete = false;
            lastError = "城巴站名資料不正確";
            continue;
        }
        labels.push_back({returnedStop, label, labelEn});
    }
    error = complete ? "" : lastError;
    return complete;
}

bool CitybusClient::fetchEtaRecords(
    const transitink::BusWidgetConfig& config,
    std::vector<transitink::BusEtaRecord>& records,
    String& error) {
    if (config.operatorId != transitink::BusOperator::Citybus) {
        records.clear();
        error = "城巴營辦商設定不正確";
        return false;
    }
    if (!isOfficialBusIdentifier(config.routeId)) {
        records.clear();
        error = "城巴路線編號格式不正確";
        return false;
    }
    if (!isOfficialBusIdentifier(config.stopId)) {
        records.clear();
        error = "城巴巴士站編號格式不正確";
        return false;
    }

    String body;
    if (!httpGet(String(kCitybusBase) + "/eta/CTB/" + config.stopId.c_str() + "/" +
                     config.routeId.c_str(),
                 body, error)) {
        records.clear();
        return false;
    }

    std::string parserError;
    if (!parseCitybusEtaJson(body.c_str(), config, records, parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}
