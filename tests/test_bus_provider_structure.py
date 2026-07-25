import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class BusProviderStructureTests(unittest.TestCase):
    def source(self, relative_path):
        path = ROOT / relative_path
        self.assertTrue(path.exists(), f"missing Task 3 file: {relative_path}")
        return path.read_text()

    def test_citybus_v2_urls_and_ten_second_timeout(self):
        source = self.source("src/CitybusClient.cpp")
        self.assertIn("https://rt.data.gov.hk/v2/transport/citybus", source)
        self.assertIn('"/route/CTB"', source)
        self.assertIn('"/stop/"', source)
        self.assertIn('"/route-stop/CTB/"', source)
        self.assertIn('"/eta/CTB/"', source)
        self.assertIn("http.setTimeout(10000)", source)

    def test_citybus_validates_ids_before_constructing_request_paths(self):
        source = self.source("src/CitybusClient.cpp")
        stop = source.split("bool CitybusClient::fetchStopJson", 1)[1].split(
            "bool CitybusClient::fetchRouteStopsJson", 1
        )[0]
        route_stops = source.split("bool CitybusClient::fetchRouteStopsJson", 1)[
            1
        ].split("bool CitybusClient::fetchEtaRecords", 1)[0]
        eta = source.split("bool CitybusClient::fetchEtaRecords", 1)[1]

        self.assertIn("isOfficialBusIdentifier", stop)
        self.assertIn("isOfficialBusIdentifier", route_stops)
        self.assertIn("mapCitybusDirectionPath", route_stops)
        self.assertIn("isOfficialBusIdentifier", eta)
        self.assertLess(stop.index("isOfficialBusIdentifier"), stop.index("httpGet("))
        self.assertLess(
            route_stops.index("isOfficialBusIdentifier"),
            route_stops.index("httpGetBounded("),
        )
        self.assertLess(
            route_stops.index("mapCitybusDirectionPath"),
            route_stops.index("httpGetBounded("),
        )
        self.assertEqual(eta.count("isOfficialBusIdentifier"), 2)
        self.assertLess(eta.index("isOfficialBusIdentifier"), eta.index("httpGet("))
        self.assertIn("城巴路線編號格式不正確", source)
        self.assertIn("城巴巴士站編號格式不正確", source)
        self.assertIn("城巴路線方向設定不正確", source)
        self.assertNotIn('equalsIgnoreCase("inbound")', source)

    def test_eta_timeouts_keep_legacy_streaming_catalog_distinction(self):
        citybus = self.source("src/CitybusClient.cpp")
        kmb = self.source("src/KmbClient.cpp")
        new_kmb_overload = kmb[kmb.rfind("bool KmbClient::fetchEtaRecords(") :]
        citybus_short_get = citybus.split("bool CitybusClient::httpGet(", 1)[1].split(
            "bool CitybusClient::httpGetBounded", 1
        )[0]
        citybus_bounded_get = citybus.split("bool CitybusClient::httpGetBounded", 1)[1].split(
            "bool CitybusClient::httpGetToFile", 1
        )[0]
        citybus_catalog_get = citybus.split("bool CitybusClient::httpGetToFile", 1)[1].split(
            "bool CitybusClient::fetchRoutesJson", 1
        )[0]

        self.assertEqual(citybus_short_get.count("http.setTimeout(10000)"), 1)
        self.assertNotIn("http.setTimeout(20000)", citybus_short_get)
        self.assertEqual(citybus_bounded_get.count("http.setTimeout(10000)"), 1)
        self.assertIn("writeToStream", citybus_bounded_get)
        self.assertEqual(citybus_catalog_get.count("http.setTimeout(20000)"), 1)
        self.assertNotIn("getString()", citybus_catalog_get)
        self.assertEqual(kmb.count("http.setTimeout(10000)"), 2)
        self.assertEqual(kmb.count("http.setTimeout(20000)"), 2)
        self.assertIn("httpGet(", new_kmb_overload)

    def test_dispatch_is_explicit_and_delegates_to_shared_normalizer(self):
        source = self.source("src/providers/BusProvider.cpp")
        self.assertIn("case transitink::BusOperator::Kmb:", source)
        self.assertIn("case transitink::BusOperator::LongWin:", source)
        self.assertIn("case transitink::BusOperator::Citybus:", source)
        self.assertIn("case transitink::BusOperator::Tfl:", source)
        self.assertIn("kmb_.fetchEtaRecords(config.bus", source)
        self.assertIn("citybus_.fetchEtaRecords(config.bus", source)
        self.assertIn("tfl_.fetchEtaRecords(config.bus, nowEpoch", source)
        self.assertGreaterEqual(source.count("normalizeBusSnapshot("), 2)
        self.assertNotRegex(source, r"routeId[^\n]*(LongWin|LWB|lwb)")

    def test_tfl_uses_verified_tls_streaming_catalog_and_anonymous_api(self):
        header = self.source("include/TflClient.h")
        source = self.source("src/TflClient.cpp")
        trust = self.source("include/TransitTlsTrust.h")
        parser = self.source("src/TransitJsonParsers.cpp")
        catalog = self.source("src/WidgetCatalogService.cpp")
        portal = self.source("src/TransitInkPortalPage.cpp")

        self.assertIn("https://api.tfl.gov.uk", source)
        self.assertIn('"/Line/"', source)
        self.assertIn('"/Route/Sequence/"', source)
        self.assertIn('"/StopPoint/"', source)
        self.assertIn("configureTflVerifiedTls", source)
        self.assertIn("kGoogleTrustServicesRootR4", trust)
        self.assertNotIn("setInsecure", source)
        self.assertNotIn("app_key", header + source)
        self.assertIn("DeserializationOption::Filter(filter)", source)
        self.assertIn("http.useHTTP10(true)", source)
        self.assertIn('filter["orderedLineRoutes"]', source)
        self.assertIn('filter["stopPointSequences"]', source)
        self.assertIn("parseTflDirectionsJson", parser)
        self.assertIn("parseTflRouteSequenceJson", parser)
        self.assertIn("parseTflEtaJson", parser)
        self.assertIn("isValidJsonObject(json)", catalog)
        self.assertIn('item["id"] = id;', catalog)
        self.assertIn(
            'item["label_tc"] = visibleStopLabel(stop.labelTc);', catalog
        )
        self.assertNotIn(
            "visibleStopLabel(stop.labelTc).c_str()", catalog
        )
        self.assertIn("Powered by TfL Open Data", portal)
        self.assertIn("Data provided by Transport for London", portal)

    def test_kmb_overload_selects_exact_company_without_route_inference(self):
        header = self.source("include/KmbClient.h")
        source = self.source("src/KmbClient.cpp")
        parser_header = self.source("include/TransitJsonParsers.h")
        parser_source = self.source("src/TransitJsonParsers.cpp")
        self.assertIn("const transitink::BusWidgetConfig& config", header)
        self.assertIn("std::vector<transitink::BusEtaRecord>& records", header)
        self.assertIn("bool parseKmbEtaJson(", parser_header)
        self.assertIn("parseKmbEtaJson(", source)
        self.assertIn('item["co"]', parser_source)
        self.assertIn('"KMB"', parser_source)
        self.assertIn("config.operatorId", parser_source)
        self.assertNotRegex(source, r"routeId[^\n]*(LongWin|LWB|lwb)")

    def test_citybus_parser_is_native_and_transport_free(self):
        header = self.source("include/TransitJsonParsers.h")
        source = self.source("src/TransitJsonParsers.cpp")
        self.assertIn("bool parseCitybusEtaJson(", header)
        self.assertIn('item["co"]', source)
        self.assertIn('item["dir"]', source)
        self.assertIn('"rmk_tc"', source)
        self.assertIn('find("取消")', source)
        self.assertNotIn("#include <Arduino.h>", source)
        self.assertNotIn("#include <HTTPClient.h>", source)
        self.assertNotIn("WiFi", source)
        self.assertNotRegex(source, r"\bString\b")

if __name__ == "__main__":
    unittest.main()
