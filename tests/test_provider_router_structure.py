import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ProviderRouterStructureTests(unittest.TestCase):
    def source(self, relative_path):
        path = ROOT / relative_path
        self.assertTrue(path.exists(), f"missing Task 6 file: {relative_path}")
        return path.read_text()

    def test_router_owns_only_provider_references(self):
        header = self.source("include/providers/WidgetProviderRouter.h")
        for member in ("BusProvider& bus_", "GmbProvider& gmb_", "MtrProvider& mtr_", "LightRailProvider& lightRail_", "JourneyTimeProvider& journey_"):
            self.assertIn(member, header)
        self.assertNotIn("BusProvider bus_", header)
        self.assertNotIn("GmbProvider gmb_", header)
        self.assertNotIn("MtrProvider mtr_", header)
        self.assertNotIn("LightRailProvider lightRail_", header)
        self.assertNotIn("JourneyTimeProvider journey_", header)

    def test_router_only_validates_and_dispatches(self):
        source = self.source("src/providers/WidgetProviderRouter.cpp")
        self.assertIn("isWidgetConfigValid(config)", source)
        self.assertIn("slot >= transitink::kWidgetSlotCount", source)
        self.assertIn("bus_.fetch(slot, config, nowEpoch)", source)
        self.assertIn("gmb_.fetch(slot, config, nowEpoch)", source)
        self.assertIn("mtr_.fetch(slot, config, nowEpoch)", source)
        self.assertIn("lightRail_.fetch(slot, config, nowEpoch)", source)
        self.assertIn("journey_.fetch(slot, config, nowEpoch)", source)
        self.assertIn('snapshot.providerMessage = "設定不完整"', source)
        for forbidden in ("HTTPClient", "WiFi", "parse", "Catalog", "normalize"):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
