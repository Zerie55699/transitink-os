#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include "AppConfig.h"
#include "BatteryMonitor.h"
#include "ConfigStore.h"
#include "FirmwareUpdateService.h"
#include "WidgetCatalogService.h"

class ConfigPortal {
public:
    ConfigPortal(DeviceConfig& config, ConfigStore& store, WidgetCatalogService& catalog);

    void begin(bool forceAp);
    void stop();
    void loop();
    bool isApMode() const { return apMode_; }
    bool isStarted() const { return serverStarted_; }
    const String& apPassword() const { return apPassword_; }
    String pageUrl() const;

private:
    void startAp();
    void registerRoutes();
    bool authorizePortalRequest(bool validateOrigin);
    void sendIndex();
    void sendConfig();
    void saveConfig();
    void scanWifiNetworks();
    void connectWifiForSetup();
    void sendSetupWifiStatus();
    void checkFirmwareUpdate();
    void installFirmwareUpdate();
    void listBusRoutes();
    void listBusDirections();
    void listBusStops();
    void listGmbRoutes();
    void listGmbDirections();
    void listGmbStops();
    void listRailLines();
    void listRailStations();
    void listRailDirections();
    void listJourneyLocations();
    void listJourneyDestinations();
    void serveEmbeddedCatalog(const char* assetPath);
    void readUpdatedRouteIndex();
    void refreshRouteIndex();
    void readRouteOverride();
    void refreshRoute();
    void sendCatalogResult(bool ok, const String& json, const String& error);
    void sendText(int code, const String& contentType, const String& body);
    IPAddress portalIp() const;

    DeviceConfig& config_;
    ConfigStore& store_;
    WidgetCatalogService& catalog_;
    BatteryMonitor batteryMonitor_;
    transitink::FirmwareUpdateService firmwareUpdate_;
    WebServer server_;
    DNSServer dns_;
    bool apMode_ = false;
    bool routesRegistered_ = false;
    bool serverStarted_ = false;
    String csrfToken_;
    String apPassword_;
    String accessToken_;
};
