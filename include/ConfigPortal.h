#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include "AppConfig.h"
#include "BatteryMonitor.h"
#include "ConfigStore.h"
#include "WidgetCatalogService.h"

class ConfigPortal {
public:
    ConfigPortal(DeviceConfig& config, ConfigStore& store, WidgetCatalogService& catalog);

    void begin(bool forceAp);
    void stop();
    void loop();
    bool isApMode() const { return apMode_; }
    bool isStarted() const { return serverStarted_; }

private:
    void startAp();
    void registerRoutes();
    void sendIndex();
    void sendConfig();
    void saveConfig();
    void scanWifiNetworks();
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

    DeviceConfig& config_;
    ConfigStore& store_;
    WidgetCatalogService& catalog_;
    BatteryMonitor batteryMonitor_;
    WebServer server_;
    DNSServer dns_;
    bool apMode_ = false;
    bool routesRegistered_ = false;
    bool serverStarted_ = false;
    String csrfToken_;
};
