#include "LightRailClient.h"
#include "TransitTlsTrust.h"

#include "TransitCatalog.h"
#include "TransitJsonParsers.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool LightRailClient::fetchArrivals(
    const transitink::MtrWidgetConfig& config,
    int64_t nowEpoch,
    std::vector<transitink::RailArrivalRecord>& records,
    int64_t& dataEpoch,
    String& error) {
    records.clear();
    dataEpoch = 0;
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

    WiFiClientSecure tls;
    transitink::configureVerifiedTls(tls);
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);
    const String url(requestUrl(config).c_str());
    if (!http.begin(tls, url)) {
        error = "無法連接輕鐵資料服務";
        return false;
    }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = String("輕鐵資料服務回應錯誤：") + code;
        http.end();
        return false;
    }
    const String body = http.getString();
    http.end();

    std::string parserError;
    if (!parseLightRailJson(body.c_str(), config, nowEpoch, records, dataEpoch,
                            parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}
