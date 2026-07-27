import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ConfigStructureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "src/AppConfig.cpp").read_text()
        cls.header = (ROOT / "include/AppConfig.h").read_text()
        cls.store = (ROOT / "src/ConfigStore.cpp").read_text()

    def test_version_3_writer_and_legacy_readers_contract(self):
        source = self.source
        store = self.store
        self.assertIn('doc["schema_version"] = transitink::kConfigSchemaVersion', source)
        self.assertIn('doc.createNestedArray("widgets")', source)
        self.assertIn('doc["routes"].as<JsonArrayConst>()', source)
        self.assertNotIn('doc["refresh_seconds"] =', source)
        self.assertNotIn('doc["stop_name_tc"] =', source)
        self.assertNotIn('doc.createNestedArray("routes")', source)
        self.assertNotIn("TRANSITINK_LEGACY_COMPAT", self.header)
        self.assertIn('preferences_.begin("bus_eta", false)', store)
        self.assertIn('preferences_.putBytes(kConfigBlobKey', store)
        self.assertIn('preferences_.getString(kLegacyConfigStringKey', store)
        self.assertIn("preferences_.getBytesLength(kConfigBlobKey)", store)
        self.assertIn('preferences_.getBool("sleep_resume", false)', store)
        self.assertIn('preferences_.putBool("sleep_resume", pending)', store)
        self.assertIn("if (sleepResumePending() == pending)", store)

    def test_device_config_uses_versioned_widget_slots(self):
        self.assertIn("uint16_t schemaVersion = transitink::kConfigSchemaVersion", self.header)
        self.assertIn("transitink::WidgetSlots widgets", self.header)
        self.assertNotIn("String stopNameTc;", self.header)
        self.assertNotIn("uint16_t refreshSeconds", self.header)
        self.assertNotIn("std::vector<bus_eta::RouteSelection> routes", self.header)
        widget_header = (ROOT / "include/core/WidgetConfigCore.h").read_text()
        self.assertIn("kWidgetPageCount = 3", widget_header)
        self.assertIn("kWidgetsPerPage = 4", widget_header)
        self.assertIn("kConfigSchemaVersion = 3", widget_header)

    def test_legacy_reader_migrates_directly_to_widget_slots(self):
        source = self.source
        self.assertIn("transitink::migrateLegacyRoutes(legacyRoutes, legacyStopNameTc.c_str())", source)
        self.assertNotIn("projectLegacyCompatibility", source)

    def test_legacy_decode_accepts_old_fields_without_runtime_projection(self):
        source = self.source
        self.assertIn('String legacyStopNameTc = asString(doc["stop_name_tc"]);', source)
        self.assertIn('doc["refresh_seconds"] | ETA_REFRESH_DEFAULT_SECONDS', source)
        self.assertNotIn("parsed.stopNameTc", source)
        self.assertNotIn("parsed.refreshSeconds", source)
        self.assertNotIn("parsed.routes", source)

    def test_widgets_use_nested_active_payloads(self):
        source = self.source
        self.assertIn('item["type"] = transitink::widgetTypeId(widget.type)', source)
        self.assertIn('item.createNestedObject("bus")', source)
        self.assertIn('bus["operator"] = transitink::busOperatorId(widget.bus.operatorId)', source)
        self.assertIn('item.createNestedObject("mtr")', source)
        self.assertIn('mtr["mode"] = transitink::railModeId(widget.mtr.mode)', source)
        self.assertIn('item.createNestedObject("journey_time")', source)
        self.assertIn('item["bus"].as<JsonObjectConst>()', source)
        self.assertIn('item["mtr"].as<JsonObjectConst>()', source)
        self.assertIn('item["journey_time"].as<JsonObjectConst>()', source)

    def test_json_document_capacity_is_bounded(self):
        portal_codec = (ROOT / "src/PortalConfigCodec.cpp").read_text()
        self.assertEqual(
            self.source.count("DynamicJsonDocument doc(transitink::kConfigJsonCapacity)"), 2
        )
        self.assertEqual(
            portal_codec.count("DynamicJsonDocument doc(transitink::kConfigJsonCapacity)"), 2
        )
        self.assertNotIn("StaticJsonDocument<transitink::kConfigJsonCapacity>", self.source)
        self.assertNotIn("StaticJsonDocument<transitink::kConfigJsonCapacity>", portal_codec)
        self.assertIn("doc.overflowed()", self.source)

    def test_nested_save_path_keeps_device_config_candidates_off_loop_stack(self):
        portal_codec = (ROOT / "src/PortalConfigCodec.cpp").read_text()
        self.assertIn("std::unique_ptr<DeviceConfig> parsed", self.source)
        self.assertIn("std::unique_ptr<DeviceConfig> parsed", portal_codec)
        self.assertNotIn("DeviceConfig parsed;", self.source)
        self.assertNotIn("DeviceConfig parsed;", portal_codec)


class PortalCatalogStructureTests(unittest.TestCase):
    def test_catalog_service_persists_updated_index_and_route_overrides(self):
        header = (ROOT / "include/WidgetCatalogService.h").read_text()
        source = (ROOT / "src/WidgetCatalogService.cpp").read_text()
        for method in (
            "listBusRoutes", "listBusStops", "listGmbRoutes", "listGmbStops",
            "listRailLines", "listJourneyLocations", "refreshBusRoute",
            "refreshGmbRoute", "readBusRouteOverride", "readGmbRouteOverride",
            "refreshRouteIndex", "readUpdatedRouteIndex",
        ):
            self.assertIn(method, header)
        self.assertIn("LittleFS", source)
        self.assertIn("if (usable && !refresh)", source)
        self.assertIn("readJsonCache", source)
        self.assertIn("writeJsonCache", source)
        self.assertNotIn("86400000UL", source)
        self.assertNotIn("TRANSITINK_CATALOG_BASE_URL", header + source)
        self.assertNotIn("verifyManifestSignature", source)

    def test_device_cache_hides_internal_stop_codes_before_frontend_output(self):
        source = (ROOT / "src/WidgetCatalogService.cpp").read_text()
        self.assertIn("visibleStopLabel", source)
        self.assertIn("hasLetter && hasDigit ? input.substr(0, open) : input", source)
        self.assertGreaterEqual(source.count('item["label_tc"] = visibleStopLabel'), 2)

    def test_device_clients_use_official_provider_sources(self):
        sources = "".join(
            (ROOT / path).read_text()
            for path in (
                "src/core/BusEtaCore.cpp",
                "src/CitybusClient.cpp",
                "src/GmbClient.cpp",
                "src/TflClient.cpp",
            )
        )
        self.assertIn("data.etabus.gov.hk", sources)
        self.assertIn("rt.data.gov.hk", sources)
        self.assertIn("data.etagmb.gov.hk", sources)
        self.assertIn("api.tfl.gov.uk", sources)

    def test_manual_refresh_writes_one_atomic_route_override(self):
        source = (ROOT / "src/WidgetCatalogService.cpp").read_text()
        bus = source.split("bool WidgetCatalogService::refreshBusRoute", 1)[1].split(
            "bool WidgetCatalogService::refreshGmbRoute", 1
        )[0]
        gmb = source.split("bool WidgetCatalogService::refreshGmbRoute", 1)[1].split(
            "bool WidgetCatalogService::appendStatus", 1
        )[0]
        self.assertIn("listBusRoutes(op, true", bus)
        self.assertIn("listBusStops(op, normalized", bus)
        self.assertIn("LittleFS.remove(kmbRouteStopCachePath", bus)
        self.assertIn("writeJsonCache(busOverrideCachePath", bus)
        self.assertIn("listGmbDirections(normalized, true", gmb)
        self.assertIn("listGmbStops(routeId, routeSeq, true", gmb)
        self.assertIn("writeJsonCache(gmbOverrideCachePath", gmb)
        self.assertNotIn("removeRouteDetailCaches", source)

    def test_gmb_direction_search_resolves_all_regions_and_caches_selection_ids(self):
        source = (ROOT / "src/WidgetCatalogService.cpp").read_text()
        method = source.split("bool WidgetCatalogService::listGmbDirections", 1)[1].split(
            "bool WidgetCatalogService::listGmbStops", 1
        )[0]
        self.assertIn('{"HKI", "KLN", "NT"}', method)
        self.assertIn('item["region"]', method)
        self.assertIn('item["route_id"]', method)
        self.assertIn('item["route_seq"]', method)
        self.assertIn("writeJsonCache(cachePath", method)


class ThinConfigPortalStructureTests(unittest.TestCase):
    def test_portal_has_only_widget_catalog_constructor(self):
        header = (ROOT / "include/ConfigPortal.h").read_text()
        source = (ROOT / "src/ConfigPortal.cpp").read_text()
        self.assertIn("WidgetCatalogService& catalog", header)
        self.assertNotIn("CatalogAssetStore", header + source)
        self.assertNotIn("TRANSITINK_LEGACY_COMPAT", header + source)
        self.assertNotIn("ConfigPortal(DeviceConfig& config, ConfigStore& store, KmbClient& kmb)", header)
        self.assertNotIn("legacyCatalog", source)

    def test_portal_registers_exact_catalog_routes_and_no_raw_proxies(self):
        source = (ROOT / "src/ConfigPortal.cpp").read_text()
        endpoints = (
            "/api/catalog/bus/routes", "/api/catalog/bus/directions",
            "/api/catalog/bus/stops", "/api/catalog/gmb/routes",
            "/api/catalog/gmb/directions", "/api/catalog/gmb/stops",
            "/api/catalog/rail/lines", "/api/catalog/rail/stations",
            "/api/catalog/rail/directions", "/api/catalog/route-index",
            "/api/catalog/update", "/api/catalog/route-override",
            "/api/catalog/route-refresh",
            "/api/catalog/journey/locations", "/api/catalog/journey/destinations",
        )
        for endpoint in endpoints:
            self.assertEqual(source.count(f'server_.on("{endpoint}"'), 1)
        self.assertIn('server_.on("/api/wifi"', source)
        self.assertEqual(source.count('server_.on("/api/wifi/connect"'), 1)
        self.assertEqual(source.count('server_.on("/api/wifi/status"'), 1)
        self.assertEqual(source.count('server_.on("/api/firmware/update"'), 2)
        self.assertIn("firmwareUpdate_.check", source)
        self.assertIn("firmwareUpdate_.install", source)
        for asset in (
            "index", "stops-kmb", "stops-ctb", "stops-gmb",
            "stops-tfl", "rail",
        ):
            self.assertIn(f'/assets/catalog/current/{asset}.json', source)
        self.assertIn('server_.sendHeader("Content-Encoding", "gzip")', source)
        self.assertIn('strcmp(assetPath, "index.json") == 0', source)
        self.assertIn('"public, max-age=31536000, immutable"', source)
        for old in ('server_.on("/api/routes"', 'server_.on("/api/stops"',
                    'server_.on("/api/stop"', 'server_.on("/api/route-stops"'):
            self.assertNotIn(old, source)
        self.assertNotIn("proxyRoutes", source)
        self.assertNotIn("proxyStops", source)
        self.assertNotIn("proxyRouteStops", source)

    def test_first_setup_can_join_wifi_without_stopping_the_portal_ap(self):
        source = (ROOT / "src/ConfigPortal.cpp").read_text()
        connect = source.split(
            "void ConfigPortal::connectWifiForSetup()", 1
        )[1].split("void ConfigPortal::sendSetupWifiStatus()", 1)[0]
        status = source.split(
            "void ConfigPortal::sendSetupWifiStatus()", 1
        )[1].split("void ConfigPortal::sendCatalogResult", 1)[0]
        scan = source.split("void ConfigPortal::scanWifiNetworks()", 1)[1].split(
            "void ConfigPortal::connectWifiForSetup()", 1
        )[0]
        self.assertIn("isPortalSaveAuthorized", connect)
        self.assertLess(
            connect.index("isPortalSaveAuthorized"),
            connect.index('server_.arg("plain")'),
        )
        self.assertIn("WiFi.enableSTA(true)", connect)
        self.assertIn("WiFi.begin(ssid.c_str(), password.c_str())", connect)
        self.assertIn("configTzTime", connect)
        self.assertNotIn("WiFi.mode(WIFI_STA)", connect)
        self.assertNotIn("WiFi.softAPdisconnect", connect)
        self.assertIn("WiFi.status() == WL_CONNECTED", status)
        self.assertIn("time(nullptr)", status)
        self.assertIn('response["ready"] = timeReady', status)
        self.assertIn("keepStationEnabled", scan)
        self.assertIn("WiFi.getMode()", scan)
        self.assertIn("wifiMode == WIFI_AP_STA", scan)
        self.assertIn("apMode_ && !keepStationEnabled", scan)

    def test_portal_delegates_page_codec_and_atomic_save(self):
        source = (ROOT / "src/ConfigPortal.cpp").read_text()
        self.assertIn("kTransitInkPortalHtml", source)
        self.assertIn("encodePortalConfig", source)
        self.assertIn("savePortalConfig", source)
        self.assertIn("catalog_", source)
        self.assertNotIn("serializeDeviceConfigJson(config_)", source)
        self.assertNotIn("parseDeviceConfigJson(body, parsed", source)

    def test_portal_rejects_cross_site_save_before_reading_body(self):
        source = (ROOT / "src/ConfigPortal.cpp").read_text()
        page = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        codec = (ROOT / "src/PortalConfigCodec.cpp").read_text()
        save = source.split("void ConfigPortal::saveConfig()", 1)[1].split(
            "void ConfigPortal::scanWifiNetworks()", 1
        )[0]
        self.assertNotIn("Access-Control-Allow-Origin", source)
        self.assertIn("esp_random", source)
        self.assertIn("csrfToken_", source)
        self.assertIn("isPortalSaveAuthorized", save)
        self.assertLess(save.index("isPortalSaveAuthorized"), save.index('server_.arg("plain")'))
        self.assertLess(save.index("isPortalSaveAuthorized"), save.index("savePortalConfig"))
        self.assertIn('doc["csrf_token"]', codec)
        self.assertIn("cfg.csrf_token", page)
        self.assertIn("const csrfHeader='X-TransitInk-CSRF'", page)
        self.assertIn("[csrfHeader]:csrfToken", page)

    def test_config_get_disables_http_and_browser_caches_on_success_and_error(self):
        source = (ROOT / "src/ConfigPortal.cpp").read_text()
        page = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        send_config = source.split("void ConfigPortal::sendConfig()", 1)[1].split(
            "void ConfigPortal::saveConfig()", 1
        )[0]
        self.assertIn('server_.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate")', send_config)
        self.assertIn('server_.sendHeader("Pragma", "no-cache")', send_config)
        self.assertLess(send_config.index("sendHeader"), send_config.index("encodePortalConfig"))
        self.assertIn("api('/api/config',{cache:'no-store'})", page)


if __name__ == "__main__":
    unittest.main()
