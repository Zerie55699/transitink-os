import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class JourneyTimeClientStructureTests(unittest.TestCase):
    def test_streaming_transport_is_bounded_and_cleaned_up(self):
        source = (ROOT / "src/JourneyTimeClient.cpp").read_text(encoding="utf-8")
        header = (ROOT / "include/JourneyTimeClient.h").read_text(encoding="utf-8")

        self.assertIn(
            "https://resource.data.one.gov.hk/td/jss/Journeytimev2.xml", header
        )
        self.assertIn("kTimeoutMs = 10000", header)
        self.assertIn("kReadBufferBytes = 512", header)
        self.assertIn("kMaxResponseBytes = 65536", header)
        self.assertIn("std::array<uint8_t, JourneyTimeClient::kReadBufferBytes>", source)
        self.assertIn("http.setTimeout(JourneyTimeClient::kTimeoutMs)", source)
        self.assertIn("http.getStreamPtr()", source)
        self.assertNotIn("getString()", source)
        self.assertIn("lastProgressMs", source)
        self.assertIn(
            "millis() - lastProgressMs >= JourneyTimeClient::kTimeoutMs", source
        )
        self.assertLess(source.index("isJourneyTimePairValid"), source.index("http.begin"))
        self.assertIn("~HttpCleanup()", source)
        self.assertIn("http.end()", source)
        self.assertIn("tls.stop()", source)

    def test_vendored_yxml_is_pinned_and_real_client_host_test_is_wired(self):
        provenance = (ROOT / "lib/yxml/LICENSE").read_text(encoding="utf-8")
        orchestration = (ROOT / "tests/test_core.py").read_text(encoding="utf-8")

        self.assertIn("Pinned commit: d41923630fcf70c6e2181722d9d087dd1aa3b530", provenance)
        self.assertIn("test_journey_time_client", orchestration)
        self.assertIn("-Itest_host/journey_time_client_shims", orchestration)
        for path in (
            "test_host/test_transit_catalog.cpp",
            "test_host/test_journey_time_client.cpp",
            "test_host/journey_time_client_shims/JourneyTimeHttpFake.h",
            "test_host/journey_time_client_shims/Arduino.h",
            "test_host/journey_time_client_shims/WiFiClientSecure.h",
            "test_host/journey_time_client_shims/HTTPClient.h",
        ):
            self.assertTrue((ROOT / path).is_file(), path)


if __name__ == "__main__":
    unittest.main()
