#include "ConfigPortal.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <time.h>

#include "ProductConfig.h"
#include "PortalConfigCodec.h"
#include "TransitInkPortalPage.h"
#include "core/PortalRequestAuth.h"
#include "generated/TransitCatalogAssets.h"

namespace {

constexpr byte kDnsPort = 53;
const char* kRequestHeaders[] = {
    "Content-Type", "X-TransitInk-CSRF", "X-TransitInk-Access", "Origin"};

String generateCsrfToken() {
    char token[33];
    snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    return String(token);
}

String chipSuffix() {
    const uint64_t mac = ESP.getEfuseMac();
    char suffix[7];
    snprintf(suffix, sizeof(suffix), "%06X",
             static_cast<unsigned int>(mac & 0xFFFFFF));
    return String(suffix);
}

}  // namespace

ConfigPortal::ConfigPortal(DeviceConfig& config,
                           ConfigStore& store,
                           WidgetCatalogService& catalog)
    : config_(config), store_(store), catalog_(catalog), server_(80) {}

void ConfigPortal::begin(bool forceAp) {
    const bool catalogReady = catalog_.begin();
    batteryMonitor_.begin();
    Serial.print("Widget catalog storage: ");
    Serial.println(catalogReady ? "ready" : "failed");
    const bool useAp = forceAp || WiFi.status() != WL_CONNECTED;
    if (useAp && !apMode_) {
        startAp();
    } else if (!useAp) {
        apMode_ = false;
    }
    if (!routesRegistered_) {
        registerRoutes();
        routesRegistered_ = true;
    }
    if (!serverStarted_) {
        csrfToken_ = generateCsrfToken();
        accessToken_ = transitink::generatePortalApPassword(
                           esp_random(), esp_random(), esp_random()).c_str();
        server_.begin();
        serverStarted_ = true;
    }
}

void ConfigPortal::stop() {
    if (serverStarted_) {
        server_.stop();
        serverStarted_ = false;
    }
    if (apMode_) {
        dns_.stop();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        apMode_ = false;
    }
    apPassword_ = "";
    csrfToken_ = "";
    accessToken_ = "";
}

void ConfigPortal::loop() {
    if (!serverStarted_) {
        return;
    }
    if (apMode_) {
        dns_.processNextRequest();
    }
    server_.handleClient();
}

void ConfigPortal::startAp() {
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP);
    const String ssid = String(CONFIG_AP_PREFIX) + "-" + chipSuffix();
    apPassword_ = transitink::generatePortalApPassword(
                      esp_random(), esp_random(), esp_random()).c_str();
    WiFi.softAP(ssid.c_str(), apPassword_.c_str());
    dns_.start(kDnsPort, "*", WiFi.softAPIP());
    apMode_ = true;
}

IPAddress ConfigPortal::portalIp() const {
    return apMode_ ? WiFi.softAPIP() : WiFi.localIP();
}

String ConfigPortal::pageUrl() const {
    const String base = "http://" + portalIp().toString() + "/";
    return apMode_ ? base : base + accessToken_;
}

void ConfigPortal::registerRoutes() {
    server_.collectHeaders(kRequestHeaders, 4);
    server_.on("/", HTTP_GET, [this]() { sendIndex(); });
    server_.on("/generate_204", HTTP_GET, [this]() { sendIndex(); });
    server_.on("/api/config", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) sendConfig(); });
    server_.on("/api/save", HTTP_POST,
               [this]() { if (authorizePortalRequest(true)) saveConfig(); });
    server_.on("/api/wifi", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) scanWifiNetworks(); });
    server_.on("/api/wifi/connect", HTTP_POST,
               [this]() { if (authorizePortalRequest(true)) connectWifiForSetup(); });
    server_.on("/api/wifi/status", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) sendSetupWifiStatus(); });
    server_.on("/api/firmware/update", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) checkFirmwareUpdate(); });
    server_.on("/api/firmware/update", HTTP_POST,
               [this]() { if (authorizePortalRequest(true)) installFirmwareUpdate(); });
    server_.on("/api/catalog/bus/routes", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listBusRoutes(); });
    server_.on("/api/catalog/bus/directions", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listBusDirections(); });
    server_.on("/api/catalog/bus/stops", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listBusStops(); });
    server_.on("/api/catalog/gmb/routes", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listGmbRoutes(); });
    server_.on("/api/catalog/gmb/directions", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listGmbDirections(); });
    server_.on("/api/catalog/gmb/stops", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listGmbStops(); });
    server_.on("/api/catalog/rail/lines", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listRailLines(); });
    server_.on("/api/catalog/rail/stations", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listRailStations(); });
    server_.on("/api/catalog/rail/directions", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listRailDirections(); });
    server_.on("/api/catalog/journey/locations", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listJourneyLocations(); });
    server_.on("/api/catalog/journey/destinations", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) listJourneyDestinations(); });
    server_.on("/assets/catalog/current/index.json", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) serveEmbeddedCatalog("index.json"); });
    server_.on("/assets/catalog/current/stops-kmb.json", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) serveEmbeddedCatalog("stops-kmb.json"); });
    server_.on("/assets/catalog/current/stops-ctb.json", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) serveEmbeddedCatalog("stops-ctb.json"); });
    server_.on("/assets/catalog/current/stops-gmb.json", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) serveEmbeddedCatalog("stops-gmb.json"); });
    server_.on("/assets/catalog/current/stops-tfl.json", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) serveEmbeddedCatalog("stops-tfl.json"); });
    server_.on("/assets/catalog/current/rail.json", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) serveEmbeddedCatalog("rail.json"); });
    server_.on("/api/catalog/route-index", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) readUpdatedRouteIndex(); });
    server_.on("/api/catalog/update", HTTP_POST,
               [this]() { if (authorizePortalRequest(true)) refreshRouteIndex(); });
    server_.on("/api/catalog/route-override", HTTP_GET,
               [this]() { if (authorizePortalRequest(false)) readRouteOverride(); });
    server_.on("/api/catalog/route-refresh", HTTP_POST,
               [this]() { if (authorizePortalRequest(true)) refreshRoute(); });
    server_.onNotFound([this]() { sendIndex(); });
}

bool ConfigPortal::authorizePortalRequest(bool validateOrigin) {
    const IPAddress expectedIp = portalIp();
    const String allowedHost = expectedIp.toString();
    const bool accessAllowed = apMode_ || transitink::isPortalAccessTokenAuthorized(
                                                server_.header("X-TransitInk-Access").c_str(),
                                                accessToken_.c_str());
    if (serverStarted_ && server_.client().localIP() == expectedIp && accessAllowed &&
        transitink::isPortalRequestSourceAllowed(
                       server_.hostHeader().c_str(),
                       server_.header("Origin").c_str(),
                       allowedHost.c_str(), validateOrigin)) {
        return true;
    }
    server_.sendHeader("Cache-Control", "no-store");
    sendText(403, "text/plain; charset=utf-8", "設定要求來源不正確");
    return false;
}

void ConfigPortal::sendText(int code,
                            const String& contentType,
                            const String& body) {
    server_.send(code, contentType, body);
}

void ConfigPortal::sendIndex() {
    const IPAddress expectedIp = portalIp();
    const String allowedHost = expectedIp.toString();
    if (!serverStarted_ || server_.client().localIP() != expectedIp) {
        server_.sendHeader("Cache-Control", "no-store");
        sendText(403, "text/plain; charset=utf-8", "設定頁只可經裝置 Wi-Fi 開啟");
        return;
    }
    if (!transitink::isPortalRequestSourceAllowed(
            server_.hostHeader().c_str(), "", allowedHost.c_str(), false)) {
        if (apMode_) {
            server_.sendHeader("Location", pageUrl());
            server_.sendHeader("Cache-Control", "no-store");
            sendText(302, "text/plain; charset=utf-8", "正在前往設定頁");
        } else {
            server_.sendHeader("Cache-Control", "no-store");
            sendText(403, "text/plain; charset=utf-8", "設定頁網址不正確");
        }
        return;
    }
    if (!apMode_) {
        String submittedToken = server_.uri();
        if (submittedToken.startsWith("/")) submittedToken.remove(0, 1);
        if (!transitink::isPortalAccessTokenAuthorized(
                submittedToken.c_str(), accessToken_.c_str())) {
            server_.sendHeader("Cache-Control", "no-store");
            sendText(403, "text/plain; charset=utf-8", "請使用裝置畫面的 QR code 開啟設定頁");
            return;
        }
    }
    server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server_.sendHeader("Pragma", "no-cache");
    server_.sendHeader(
        "Content-Security-Policy",
        "default-src 'self'; script-src 'unsafe-inline'; "
        "style-src 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; "
        "object-src 'none'; base-uri 'none'; form-action 'self'; "
        "frame-ancestors 'none'");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    server_.sendHeader("X-Frame-Options", "DENY");
    server_.sendHeader("Referrer-Policy", "no-referrer");
    server_.send_P(200, "text/html; charset=utf-8", kTransitInkPortalHtml);
}

void ConfigPortal::serveEmbeddedCatalog(const char* assetPath) {
    for (std::size_t index = 0; index < transitink::kEmbeddedCatalogAssetCount; ++index) {
        const auto& asset = transitink::kEmbeddedCatalogAssets[index];
        if (strcmp(asset.path, assetPath) != 0) {
            continue;
        }
        server_.sendHeader("Cache-Control",
                           strcmp(assetPath, "index.json") == 0
                               ? "no-cache"
                               : "public, max-age=31536000, immutable");
        server_.sendHeader("Content-Encoding", "gzip");
        server_.send_P(200, "application/json; charset=utf-8",
                       reinterpret_cast<PGM_P>(asset.data), asset.size);
        return;
    }
    sendText(404, "text/plain; charset=utf-8", "找不到內建交通目錄資源");
}

void ConfigPortal::sendConfig() {
    server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server_.sendHeader("Pragma", "no-cache");
    const bus_eta::BatterySnapshot battery = batteryMonitor_.read();
    String json;
    String error;
    if (!encodePortalConfig(config_, battery, FIRMWARE_VERSION, csrfToken_, json, error)) {
        sendText(500, "text/plain; charset=utf-8", error);
        return;
    }
    if (!catalog_.appendStatus(json, error)) {
        sendText(500, "text/plain; charset=utf-8", error);
        return;
    }
    sendText(200, "application/json; charset=utf-8", json);
}

void ConfigPortal::checkFirmwareUpdate() {
    transitink::FirmwareUpdateManifest manifest;
    String error;
    if (!firmwareUpdate_.check(manifest, error)) {
        sendText(502, "text/plain; charset=utf-8", error);
        return;
    }
    StaticJsonDocument<384> response;
    response["current_version"] = FIRMWARE_VERSION;
    response["latest_version"] = manifest.version;
    response["update_available"] = manifest.updateAvailable;
    response["size"] = manifest.size;
    String json;
    serializeJson(response, json);
    server_.sendHeader("Cache-Control", "no-store");
    sendText(200, "application/json; charset=utf-8", json);
}

void ConfigPortal::installFirmwareUpdate() {
    if (!transitink::isPortalSaveAuthorized(
            server_.header("Content-Type").c_str(),
            server_.header("X-TransitInk-CSRF").c_str(),
            csrfToken_.c_str())) {
        sendText(403, "text/plain; charset=utf-8", "韌體更新要求驗證失敗");
        return;
    }
    StaticJsonDocument<128> request;
    if (server_.arg("plain").length() > 128) {
        sendText(400, "text/plain; charset=utf-8", "韌體更新要求格式不正確");
        return;
    }
    const DeserializationError parseError =
        deserializeJson(request, server_.arg("plain"));
    if (parseError || !request["version"].is<const char*>()) {
        sendText(400, "text/plain; charset=utf-8", "韌體更新要求格式不正確");
        return;
    }
    transitink::FirmwareUpdateManifest manifest;
    String error;
    if (!firmwareUpdate_.install(request["version"].as<const char*>(),
                                 manifest, error)) {
        sendText(502, "text/plain; charset=utf-8", error);
        return;
    }
    StaticJsonDocument<128> response;
    response["installed"] = true;
    response["version"] = manifest.version;
    String json;
    serializeJson(response, json);
    server_.sendHeader("Cache-Control", "no-store");
    sendText(200, "application/json; charset=utf-8", json);
    delay(600);
    ESP.restart();
}

void ConfigPortal::saveConfig() {
    if (!transitink::isPortalSaveAuthorized(server_.header("Content-Type").c_str(),
                                            server_.header("X-TransitInk-CSRF").c_str(),
                                            csrfToken_.c_str())) {
        sendText(403, "text/plain; charset=utf-8", "儲存要求驗證失敗");
        return;
    }
    String error;
    if (!savePortalConfig(server_.arg("plain"), config_, store_, error)) {
        sendText(400, "text/plain; charset=utf-8", error);
        return;
    }
    sendText(200, "text/plain; charset=utf-8", "設定已儲存，裝置正在重新啟動。");
    delay(400);
    ESP.restart();
}

void ConfigPortal::scanWifiNetworks() {
    const wifi_mode_t wifiMode = WiFi.getMode();
    const bool keepStationEnabled =
        wifiMode == WIFI_STA || wifiMode == WIFI_AP_STA;
    const int count = WiFi.scanNetworks(false, true);
    if (count < 0) {
        WiFi.scanDelete();
        if (apMode_ && !keepStationEnabled) WiFi.enableSTA(false);
        sendText(500, "text/plain; charset=utf-8", "Wi-Fi 掃描失敗");
        return;
    }
    DynamicJsonDocument doc(4096);
    JsonArray data = doc.createNestedArray("data");
    for (int index = 0; index < count && index < 24; ++index) {
        const String ssid = WiFi.SSID(index);
        if (ssid.isEmpty()) {
            continue;
        }
        bool seen = false;
        for (JsonObjectConst item : data) {
            if (ssid == (item["id"] | "")) {
                seen = true;
                break;
            }
        }
        if (seen) {
            continue;
        }
        JsonObject item = data.createNestedObject();
        item["id"] = ssid;
        item["label_tc"] = ssid;
        item["rssi"] = WiFi.RSSI(index);
        item["secure"] = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    if (apMode_ && !keepStationEnabled) WiFi.enableSTA(false);
    String json;
    serializeJson(doc, json);
    sendText(200, "application/json; charset=utf-8", json);
}

void ConfigPortal::connectWifiForSetup() {
    if (!transitink::isPortalSaveAuthorized(
            server_.header("Content-Type").c_str(),
            server_.header("X-TransitInk-CSRF").c_str(),
            csrfToken_.c_str())) {
        sendText(403, "text/plain; charset=utf-8", "Wi-Fi 連線要求驗證失敗");
        return;
    }
    if (!apMode_) {
        sendText(200, "application/json; charset=utf-8",
                 R"({"started":true})");
        return;
    }

    StaticJsonDocument<256> request;
    const DeserializationError parseError =
        deserializeJson(request, server_.arg("plain"));
    if (parseError || !request.is<JsonObject>() ||
        !request["wifi_ssid"].is<const char*>() ||
        (!request["wifi_password"].isNull() &&
         !request["wifi_password"].is<const char*>())) {
        sendText(400, "text/plain; charset=utf-8", "Wi-Fi 連線資料格式不正確");
        return;
    }

    const String ssid = request["wifi_ssid"].as<const char*>();
    String password = request["wifi_password"] | "";
    if (password.isEmpty() && ssid == config_.wifiSsid) {
        password = config_.wifiPassword;
    }
    if (ssid.isEmpty() || ssid.length() > transitink::kMaxWifiSsidBytes ||
        password.length() > transitink::kMaxWifiCredentialBytes) {
        sendText(400, "text/plain; charset=utf-8", "Wi-Fi 名稱或密碼格式不正確");
        return;
    }

    WiFi.enableSTA(true);
    WiFi.begin(ssid.c_str(), password.c_str());
    configTzTime(transitink::devicePosixTimeZone(config_.timeZone),
                 "pool.ntp.org", "time.cloudflare.com", "time.nist.gov");
    sendText(202, "application/json; charset=utf-8",
             R"({"started":true})");
}

void ConfigPortal::sendSetupWifiStatus() {
    constexpr time_t kMinimumValidEpoch = 1700000000;
    const bool connected = WiFi.status() == WL_CONNECTED;
    const bool timeReady = connected && time(nullptr) >= kMinimumValidEpoch;
    StaticJsonDocument<96> response;
    response["connected"] = connected;
    response["ready"] = timeReady;
    String json;
    serializeJson(response, json);
    sendText(200, "application/json; charset=utf-8", json);
}

void ConfigPortal::sendCatalogResult(bool ok,
                                     const String& json,
                                     const String& error) {
    sendText(ok ? 200 : 400,
             ok ? "application/json; charset=utf-8" : "text/plain; charset=utf-8",
             ok ? json : error);
}

void ConfigPortal::listBusRoutes() {
    transitink::BusOperator op;
    String json;
    String error;
    if (!parseCatalogBusOperator(server_.arg("operator"), op)) {
        sendText(400, "text/plain; charset=utf-8", "巴士營辦商設定不正確");
        return;
    }
    sendCatalogResult(catalog_.listBusRoutes(op, server_.arg("refresh") == "1", json, error),
                      json, error);
}

void ConfigPortal::listBusDirections() {
    transitink::BusOperator op;
    String json;
    String error;
    if (!parseCatalogBusOperator(server_.arg("operator"), op) ||
        server_.arg("route").isEmpty()) {
        sendText(400, "text/plain; charset=utf-8", "巴士路線查詢參數不正確");
        return;
    }
    sendCatalogResult(catalog_.listBusDirections(op, server_.arg("route"), json, error),
                      json, error);
}

void ConfigPortal::listBusStops() {
    transitink::BusOperator op;
    String json;
    String error;
    if (!parseCatalogBusOperator(server_.arg("operator"), op)) {
        sendText(400, "text/plain; charset=utf-8", "巴士營辦商設定不正確");
        return;
    }
    sendCatalogResult(catalog_.listBusStops(op, server_.arg("route"),
                                            server_.arg("direction"),
                                            server_.arg("service_type"),
                                            server_.arg("refresh") == "1", json, error),
                      json, error);
}

void ConfigPortal::listGmbRoutes() {
    String json;
    String error;
    sendCatalogResult(catalog_.listGmbRoutes(server_.arg("refresh") == "1",
                                             json, error),
                      json, error);
}

void ConfigPortal::listGmbDirections() {
    String json;
    String error;
    if (server_.arg("route").isEmpty()) {
        sendText(400, "text/plain; charset=utf-8", "專線小巴路線查詢參數不正確");
        return;
    }
    sendCatalogResult(catalog_.listGmbDirections(server_.arg("route"),
                                                 server_.arg("refresh") == "1",
                                                 json, error),
                      json, error);
}

void ConfigPortal::listGmbStops() {
    String json;
    String error;
    sendCatalogResult(catalog_.listGmbStops(server_.arg("route_id"),
                                            server_.arg("route_seq"),
                                            server_.arg("refresh") == "1",
                                            json, error),
                      json, error);
}

void ConfigPortal::listRailLines() {
    transitink::RailMode mode;
    String json;
    String error;
    if (!parseCatalogRailMode(server_.arg("mode"), mode)) {
        sendText(400, "text/plain; charset=utf-8", "鐵路類型設定不正確");
        return;
    }
    sendCatalogResult(catalog_.listRailLines(mode, json, error), json, error);
}

void ConfigPortal::listRailStations() {
    transitink::RailMode mode;
    String json;
    String error;
    if (!parseCatalogRailMode(server_.arg("mode"), mode)) {
        sendText(400, "text/plain; charset=utf-8", "鐵路類型設定不正確");
        return;
    }
    sendCatalogResult(catalog_.listRailStations(mode, server_.arg("line"), json, error),
                      json, error);
}

void ConfigPortal::listRailDirections() {
    transitink::RailMode mode;
    String json;
    String error;
    if (!parseCatalogRailMode(server_.arg("mode"), mode)) {
        sendText(400, "text/plain; charset=utf-8", "鐵路類型設定不正確");
        return;
    }
    sendCatalogResult(catalog_.listRailDirections(mode, server_.arg("line"),
                                                   server_.arg("station"), json, error),
                      json, error);
}

void ConfigPortal::listJourneyLocations() {
    String json;
    String error;
    sendCatalogResult(catalog_.listJourneyLocations(json, error), json, error);
}

void ConfigPortal::listJourneyDestinations() {
    String json;
    String error;
    sendCatalogResult(catalog_.listJourneyDestinations(server_.arg("location"), json, error),
                      json, error);
}

void ConfigPortal::readRouteOverride() {
    const String kind = server_.arg("kind");
    const String route = server_.arg("route");
    String json;
    String error;
    bool ok = false;
    if (kind == "bus") {
        transitink::BusOperator op;
        if (!parseCatalogBusOperator(server_.arg("operator"), op)) {
            sendText(400, "text/plain; charset=utf-8", "巴士營辦商設定不正確");
            return;
        }
        ok = catalog_.readBusRouteOverride(op, route, json, error);
    } else if (kind == "gmb") {
        ok = catalog_.readGmbRouteOverride(route, json, error);
    } else {
        sendText(400, "text/plain; charset=utf-8", "交通路線類型不正確");
        return;
    }
    if (!ok && error == "not_found") {
        sendText(404, "text/plain; charset=utf-8", "沒有此路線的本機更新");
        return;
    }
    sendCatalogResult(ok, json, error);
}

void ConfigPortal::readUpdatedRouteIndex() {
    String json;
    String error;
    if (!catalog_.readUpdatedRouteIndex(json, error)) {
        if (error == "not_found") {
            sendText(404, "text/plain; charset=utf-8", "尚未更新本機路線索引");
            return;
        }
        sendText(500, "text/plain; charset=utf-8", error);
        return;
    }
    sendText(200, "application/json; charset=utf-8", json);
}

void ConfigPortal::refreshRouteIndex() {
    if (!transitink::isPortalSaveAuthorized(server_.header("Content-Type").c_str(),
                                            server_.header("X-TransitInk-CSRF").c_str(),
                                            csrfToken_.c_str())) {
        sendText(403, "text/plain; charset=utf-8", "更新要求驗證失敗");
        return;
    }
    String json;
    String error;
    if (!catalog_.refreshRouteIndex(json, error)) {
        sendText(503, "text/plain; charset=utf-8", error);
        return;
    }
    sendText(200, "application/json; charset=utf-8", json);
}

void ConfigPortal::refreshRoute() {
    if (!transitink::isPortalSaveAuthorized(server_.header("Content-Type").c_str(),
                                            server_.header("X-TransitInk-CSRF").c_str(),
                                            csrfToken_.c_str())) {
        sendText(403, "text/plain; charset=utf-8", "更新要求驗證失敗");
        return;
    }
    StaticJsonDocument<512> request;
    const DeserializationError parseError = deserializeJson(request, server_.arg("plain"));
    if (parseError || !request.is<JsonObject>()) {
        sendText(400, "text/plain; charset=utf-8", "更新路線要求格式不正確");
        return;
    }
    const String kind = request["kind"] | "";
    const String route = request["route"] | "";
    const bool refreshRouteList = request["refresh_routes"] | true;
    const bool refreshSharedStops = request["refresh_shared_stops"] | true;
    String json;
    String error;
    bool ok = false;
    if (kind == "bus") {
        transitink::BusOperator op;
        if (!parseCatalogBusOperator(String(request["operator"] | ""), op)) {
            sendText(400, "text/plain; charset=utf-8", "巴士營辦商設定不正確");
            return;
        }
        ok = catalog_.refreshBusRoute(op, route, refreshRouteList,
                                      refreshSharedStops, json, error);
    } else if (kind == "gmb") {
        ok = catalog_.refreshGmbRoute(route, json, error);
    } else {
        sendText(400, "text/plain; charset=utf-8", "交通路線類型不正確");
        return;
    }
    if (!ok) {
        sendText(503, "text/plain; charset=utf-8", error);
        return;
    }
    sendText(200, "application/json; charset=utf-8", json);
}
