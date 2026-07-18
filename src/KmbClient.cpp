#include "KmbClient.h"

#include "TransitJsonParsers.h"
#include "core/CatalogCore.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace {

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

String toArduino(const std::string& value) {
    return String(value.c_str());
}

std::string toStdString(JsonVariantConst value) {
    if (value.isNull()) {
        return "";
    }
    if (value.is<int>()) {
        return std::to_string(value.as<int>());
    }
    return std::string(value.as<const char*>());
}

}  // namespace

bool KmbClient::httpGet(const String& url, String& body, String& error) {
    WiFiClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法建立 HTTPS 連線";
        return false;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = String("HTTP 錯誤: ") + code;
        http.end();
        return false;
    }
    body = http.getString();
    http.end();
    return true;
}

bool KmbClient::httpGetBounded(const String& url,
                               std::size_t limit,
                               String& body,
                               String& error) {
    WiFiClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法建立 HTTPS 連線";
        return false;
    }
    http.addHeader("Accept-Encoding", "identity");
    const int code = http.GET();
    const int expectedLength = http.getSize();
    if (code != HTTP_CODE_OK || expectedLength > static_cast<int>(limit)) {
        error = code == HTTP_CODE_OK ? "路線站牌回應過大" : String("HTTP 錯誤: ") + code;
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
        error = detail.empty() ? "路線站牌回應不完整" : detail.c_str();
        return false;
    }
    body = sink.accumulator().body().c_str();
    error = "";
    return true;
}

bool KmbClient::fetchRoutesJson(String& body, String& error) {
    return httpGet(toArduino(bus_eta::kmbRoutesUrl()), body, error);
}

bool KmbClient::fetchRoutesToFile(fs::FS& fs, const char* path, String& error) {
    WiFiClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    http.setTimeout(20000);
    http.setReuse(false);
    if (!http.begin(tls, toArduino(bus_eta::kmbRoutesUrl()))) {
        error = "無法建立 HTTPS 連線";
        return false;
    }
    http.addHeader("Accept-Encoding", "identity");
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = String("HTTP 錯誤: ") + code;
        http.end();
        return false;
    }

    File file = fs.open(path, FILE_WRITE, true);
    if (!file) {
        error = "無法寫入路線快取";
        http.end();
        return false;
    }

    const int expectedLength = http.getSize();
    if (expectedLength > static_cast<int>(transitink::kMaxCatalogDownloadBytes)) {
        error = "路線目錄回應過大";
        file.close();
        http.end();
        return false;
    }
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t copied = 0;
    unsigned long lastDataAt = millis();
    while (http.connected() && (expectedLength < 0 || copied < static_cast<size_t>(expectedLength))) {
        size_t available = stream->available();
        if (available == 0) {
            if (millis() - lastDataAt > 20000) {
                break;
            }
            delay(10);
            continue;
        }

        size_t toRead = available;
        if (toRead > sizeof(buffer)) {
            toRead = sizeof(buffer);
        }
        int readLen = stream->readBytes(buffer, toRead);
        if (readLen <= 0) {
            if (millis() - lastDataAt > 20000) {
                break;
            }
            continue;
        }
        if (copied + static_cast<size_t>(readLen) > transitink::kMaxCatalogDownloadBytes) {
            error = "路線目錄回應過大";
            file.close();
            http.end();
            return false;
        }
        size_t wrote = file.write(buffer, readLen);
        if (wrote != static_cast<size_t>(readLen)) {
            error = "路線快取寫入失敗";
            file.close();
            http.end();
            return false;
        }
        copied += wrote;
        lastDataAt = millis();
        delay(0);
    }
    file.flush();
    size_t written = file.size();
    file.close();
    http.end();
    Serial.print("Route cache bytes: ");
    Serial.print(copied);
    Serial.print("/");
    Serial.print(expectedLength);
    Serial.print(" file=");
    Serial.println(written);
    if (written == 0 || (expectedLength > 0 && copied < static_cast<size_t>(expectedLength))) {
        error = String("路線快取未完成: ") + copied + "/" + expectedLength;
        return false;
    }
    return true;
}

bool KmbClient::fetchStopsToFile(fs::FS& fs, const char* path, String& error) {
    WiFiClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    http.setTimeout(20000);
    http.setReuse(false);
    if (!http.begin(tls, toArduino(bus_eta::kmbStopsUrl()))) {
        error = "無法建立 HTTPS 連線";
        return false;
    }
    http.addHeader("Accept-Encoding", "identity");
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = String("HTTP 錯誤: ") + code;
        http.end();
        return false;
    }

    File file = fs.open(path, FILE_WRITE, true);
    if (!file) {
        error = "無法寫入站牌快取";
        http.end();
        return false;
    }

    const int expectedLength = http.getSize();
    if (expectedLength > static_cast<int>(transitink::kMaxCatalogDownloadBytes)) {
        error = "站牌目錄回應過大";
        file.close();
        http.end();
        return false;
    }
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t copied = 0;
    unsigned long lastDataAt = millis();
    while (http.connected() && (expectedLength < 0 || copied < static_cast<size_t>(expectedLength))) {
        size_t available = stream->available();
        if (available == 0) {
            if (millis() - lastDataAt > 20000) {
                break;
            }
            delay(10);
            continue;
        }

        size_t toRead = available;
        if (toRead > sizeof(buffer)) {
            toRead = sizeof(buffer);
        }
        int readLen = stream->readBytes(buffer, toRead);
        if (readLen <= 0) {
            if (millis() - lastDataAt > 20000) {
                break;
            }
            continue;
        }
        if (copied + static_cast<size_t>(readLen) > transitink::kMaxCatalogDownloadBytes) {
            error = "站牌目錄回應過大";
            file.close();
            http.end();
            return false;
        }
        size_t wrote = file.write(buffer, readLen);
        if (wrote != static_cast<size_t>(readLen)) {
            error = "站牌快取寫入失敗";
            file.close();
            http.end();
            return false;
        }
        copied += wrote;
        lastDataAt = millis();
        delay(0);
    }
    file.flush();
    size_t written = file.size();
    file.close();
    http.end();
    Serial.print("Stop cache bytes: ");
    Serial.print(copied);
    Serial.print("/");
    Serial.print(expectedLength);
    Serial.print(" file=");
    Serial.println(written);
    if (written == 0 || (expectedLength > 0 && copied < static_cast<size_t>(expectedLength))) {
        error = String("站牌快取未完成: ") + copied + "/" + expectedLength;
        return false;
    }
    return true;
}

bool KmbClient::fetchRouteJson(const String& route, const String& bound, const String& serviceType, String& body, String& error) {
    return httpGet(toArduino(bus_eta::kmbRouteUrl(route.c_str(), bound.c_str(), serviceType.c_str())), body, error);
}

bool KmbClient::fetchStopsJson(String& body, String& error) {
    return httpGet(toArduino(bus_eta::kmbStopsUrl()), body, error);
}

bool KmbClient::fetchStopJson(const String& stopId, String& body, String& error) {
    return httpGet(toArduino(bus_eta::kmbStopUrl(stopId.c_str())), body, error);
}

bool KmbClient::fetchRouteStopsJson(const String& route, const String& bound, const String& serviceType, String& body, String& error) {
    return httpGetBounded(
        toArduino(bus_eta::kmbRouteStopsUrl(route.c_str(), bound.c_str(), serviceType.c_str())),
        transitink::kMaxRouteStopResponseBytes, body, error);
}

bool KmbClient::fetchEtaRecords(const bus_eta::RouteSelection& selection, std::vector<bus_eta::EtaRecord>& records, String& error) {
    String body;
    if (!httpGet(toArduino(bus_eta::kmbEtaUrl(selection.stopId, selection.route, selection.serviceType)), body, error)) {
        return false;
    }

    DynamicJsonDocument doc(16384);
    DeserializationError jsonError = deserializeJson(doc, body);
    if (jsonError) {
        error = String("ETA JSON 錯誤: ") + jsonError.c_str();
        return false;
    }

    records.clear();
    for (JsonObjectConst item : doc["data"].as<JsonArrayConst>()) {
        bus_eta::EtaRecord record;
        record.route = toStdString(item["route"]);
        record.dir = toStdString(item["dir"]);
        record.serviceType = toStdString(item["service_type"]);
        record.etaSeq = item["eta_seq"] | 0;
        record.etaIso = toStdString(item["eta"]);
        record.destTc = toStdString(item["dest_tc"]);
        record.remarkTc = toStdString(item["rmk_tc"]);
        if (!record.etaIso.empty()) {
            records.push_back(record);
        }
    }
    return true;
}

bool KmbClient::fetchEtaRecords(const transitink::BusWidgetConfig& config,
                                std::vector<transitink::BusEtaRecord>& records,
                                String& error) {
    if (config.operatorId != transitink::BusOperator::Kmb &&
        config.operatorId != transitink::BusOperator::LongWin) {
        records.clear();
        error = "九巴及龍運營辦商設定不正確";
        return false;
    }

    String body;
    if (!httpGet(toArduino(bus_eta::kmbEtaUrl(config.stopId, config.routeId,
                                              config.serviceType)),
                 body, error)) {
        records.clear();
        return false;
    }

    std::string parserError;
    if (!parseKmbEtaJson(body.c_str(), config, records, parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}
