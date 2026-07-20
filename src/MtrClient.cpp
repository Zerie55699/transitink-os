#include "MtrClient.h"
#include "TransitTlsTrust.h"

#include "TransitCatalog.h"
#include "TransitJsonParsers.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool MtrClient::fetchArrivals(
    const transitink::MtrWidgetConfig& config,
    std::vector<transitink::RailArrivalRecord>& records,
    int64_t& dataEpoch,
    String& error) {
    records.clear();
    dataEpoch = 0;
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

    WiFiClientSecure tls;
    transitink::configureVerifiedTls(tls);
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);
    const String url(requestUrl(config).c_str());
    if (!http.begin(tls, url)) {
        error = "無法連接港鐵資料服務";
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = String("港鐵資料服務回應錯誤：") + code;
        http.end();
        return false;
    }
    const String body = http.getString();
    http.end();

    std::string parserError;
    if (!parseMtrNextTrainJson(body.c_str(), config, records, dataEpoch,
                               parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}
