import gzip
import hashlib
import importlib.util
import json
import subprocess
import tempfile
import unittest
import urllib.error
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/generate_transit_route_catalog.py"


def load_generator():
    spec = importlib.util.spec_from_file_location("transit_catalog_generator", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class TransitCatalogGeneratorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.generator = load_generator()
        cls.manifest = json.loads(
            (ROOT / "data/catalog/catalog-manifest.json").read_text(encoding="utf-8")
        )

    def test_canonical_gzip_is_reproducible_and_accepts_utf8_bom(self):
        payload = {"站牌": ["海壩村", "鰂魚涌"], "revision": "abc12345"}
        self.assertEqual(
            self.generator.gzip_bytes(payload), self.generator.gzip_bytes(payload)
        )
        decoded = self.generator.parse_json_bytes(
            b"\xef\xbb\xbf" + self.generator.canonical_json(payload), "fixture"
        )
        self.assertEqual(payload, decoded)

    def test_invalid_utf8_empty_fields_bad_ids_and_non_increasing_stops_fail(self):
        with self.assertRaisesRegex(self.generator.CatalogError, "JSON"):
            self.generator.parse_json_bytes(b"\xff", "fixture")
        with self.assertRaises(self.generator.CatalogError):
            self.generator.require_text("", "route")
        with self.assertRaises(self.generator.CatalogError):
            self.generator.positive_sequence("0", "sequence")
        with self.assertRaisesRegex(self.generator.CatalogError, "嚴格遞增"):
            self.generator.normalized_stop_sequence(
                [
                    {"id": "A", "label_tc": "甲", "sequence": 1},
                    {"id": "B", "label_tc": "乙", "sequence": 1},
                ],
                "fixture ",
            )

    def test_exact_duplicate_stops_are_removed_and_internal_suffix_is_hidden(self):
        stop = {"id": "A", "label_tc": "海壩村", "sequence": 1}
        self.assertEqual(
            [stop], self.generator.normalized_stop_sequence([stop, dict(stop)], "fixture ")
        )
        self.assertEqual("海壩村", self.generator.normalized_label("海壩村 (TW515)"))

    def test_tfl_fixture_builds_offline_bus_and_rail_catalogues(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / "tfl-bus-sequences").mkdir()
            (cache / "tfl-rail-stations").mkdir()
            (cache / "tfl-bus-routes.json").write_text(
                json.dumps([{
                    "id": "24",
                    "created": "2026-07-01T00:00:00Z",
                    "modified": "2026-07-02T00:00:00Z",
                    "routeSections": [{
                        "direction": "inbound",
                        "originator": "490A",
                        "destination": "490B",
                        "originationName": "South End Green",
                        "destinationName": "Grosvenor Road",
                    }],
                }]),
                encoding="utf-8",
            )
            (cache / "tfl-bus-sequences" / "24.json").write_text(
                json.dumps({
                    "orderedLineRoutes": [{"naptanIds": ["490A", "490B"]}],
                    "stopPointSequences": [{"stopPoint": [
                        {"id": "490A", "name": "South End Green"},
                        {"id": "490B", "name": "Grosvenor Road"},
                    ]}],
                }),
                encoding="utf-8",
            )
            (cache / "tfl-rail-lines.json").write_text(
                json.dumps([{
                    "id": "victoria",
                    "name": "Victoria",
                    "created": "2026-07-01T00:00:00Z",
                    "modified": "2026-07-02T00:00:00Z",
                }]),
                encoding="utf-8",
            )
            (cache / "tfl-rail-stations" / "victoria.json").write_text(
                json.dumps([{
                    "id": "940GZZLUOXC",
                    "commonName": "Oxford Circus Underground Station",
                }]),
                encoding="utf-8",
            )

            bus, stops, rail, generated_at = self.generator.build_tfl(cache)

        direction = bus["routes"]["24"][0]
        self.assertEqual("inbound", direction["direction_id"])
        self.assertEqual("490A|490B", direction["service_type"])
        self.assertEqual(
            "490A",
            stops["routes"][direction["stop_key"]][0]["id"],
        )
        self.assertEqual("victoria", rail[0]["id"])
        self.assertEqual("940GZZLUOXC", rail[0]["stations"][0]["id"])
        self.assertEqual("2026-07-02T00:00:00Z", generated_at)

    def test_checked_in_release_matches_hashes_schema_revision_and_size_limits(self):
        total = 0
        revision = self.manifest["revision"]
        for filename in self.generator.ASSET_NAMES:
            content = (ROOT / "data/catalog" / filename).read_bytes()
            total += len(content)
            self.assertEqual(
                hashlib.sha256(content).hexdigest(),
                self.manifest["assets"][filename]["sha256"],
            )
            payload = json.loads(gzip.decompress(content).decode("utf-8"))
            self.assertEqual(1, payload["schema_version"])
            self.assertEqual(revision, payload["revision"])
        self.assertLessEqual(total, self.generator.MAX_RELEASE_BYTES)
        self.assertLessEqual(
            self.manifest["assets"]["index.json.gz"]["bytes"],
            self.generator.MAX_INDEX_GZIP_BYTES,
        )

    def test_every_selectable_direction_has_strict_stops_and_eta_ids(self):
        with gzip.open(ROOT / "data/catalog/index.json.gz", "rt", encoding="utf-8") as stream:
            index = json.load(stream)
        packs = {}
        for provider in ("kmb", "ctb", "gmb", "tfl"):
            with gzip.open(
                ROOT / "data/catalog" / f"stops-{provider}.json.gz",
                "rt",
                encoding="utf-8",
            ) as stream:
                packs[provider] = json.load(stream)["routes"]

        for provider, index_key in (
            ("kmb", "kmb_lwb"),
            ("ctb", "ctb"),
            ("tfl", "tfl"),
        ):
            for directions in index["bus"][index_key]["routes"].values():
                for direction in directions:
                    stops = packs[provider][direction["stop_key"]]
                    sequences = [stop["sequence"] for stop in stops]
                    self.assertEqual(sequences, sorted(set(sequences)))
                    self.assertTrue(all(stop["id"] and stop["label_tc"] for stop in stops))
                    self.assertTrue(all("label_en" in stop for stop in stops))
                    self.assertTrue(all("origin_label_en" in direction and
                                        "destination_label_en" in direction
                                        for direction in directions))

        for directions in index["gmb"]["routes"].values():
            for direction in directions:
                stops = packs["gmb"][direction["stop_key"]]
                self.assertTrue(direction["region"] and direction["route_id"])
                self.assertTrue(
                    all(stop["stop_id"] and stop["stop_seq"] and stop["label_tc"] for stop in stops)
                )
                self.assertTrue(all("label_en" in stop for stop in stops))

    def test_official_english_labels_and_empty_fallback_markers_are_bundled(self):
        with gzip.open(
            ROOT / "data/catalog/index.json.gz", "rt", encoding="utf-8"
        ) as stream:
            index = json.load(stream)
        route = index["bus"]["kmb_lwb"]["routes"]["40P"][0]
        self.assertEqual("KWUN TONG FERRY", route["origin_label_en"])
        self.assertEqual("TSUEN WAN (NINA TOWER)",
                         route["destination_label_en"])

        with gzip.open(
            ROOT / "data/catalog/rail.json.gz", "rt", encoding="utf-8"
        ) as stream:
            rail = json.load(stream)
        self.assertIn("london_rail", rail["modes"])
        self.assertTrue(rail["modes"]["london_rail"])
        airport_express = rail["modes"]["heavy_rail"][0]
        self.assertEqual("Airport Express", airport_express["label_en"])
        self.assertEqual("Hong Kong",
                         airport_express["stations"][0]["label_en"])

        east_rail = next(
            group for group in rail["modes"]["heavy_rail"]
            if group["id"] == "EAL"
        )
        racecourse = next(
            station for station in east_rail["stations"]
            if station["id"] == "RAC"
        )
        self.assertEqual("", racecourse["label_en"])

    def test_large_count_change_is_fail_closed_until_explicitly_confirmed(self):
        previous = {"counts": {"kmb_routes": 100}}
        current = {"counts": {"kmb_routes": 111}}
        with self.assertRaisesRegex(self.generator.CatalogError, "10%"):
            self.generator.validate_count_change(previous, current, False)
        self.generator.validate_count_change(previous, current, True)

    def test_generator_check_mode_needs_no_cache_or_network(self):
        with tempfile.TemporaryDirectory() as directory:
            completed = subprocess.run(
                [
                    str(ROOT / ".venv/bin/python"),
                    str(SCRIPT),
                    "--check",
                    "--cache-dir",
                    str(Path(directory) / "missing-cache"),
                ],
                cwd=ROOT,
                text=True,
                capture_output=True,
                timeout=30,
            )
        self.assertEqual(0, completed.returncode, completed.stderr)

    def test_source_timeout_retries_then_fails_closed(self):
        with mock.patch.object(
            self.generator.urllib.request,
            "urlopen",
            side_effect=urllib.error.URLError("timeout"),
        ) as request, mock.patch.object(self.generator.time, "sleep"):
            with self.assertRaisesRegex(self.generator.CatalogError, "下載失敗"):
                self.generator.request_bytes("https://example.invalid/catalog")
        self.assertEqual(4, request.call_count)

    def test_manifest_is_data_only_and_requires_no_signing_key(self):
        self.assertNotIn("signature_algorithm", self.manifest)
        source = SCRIPT.read_text(encoding="utf-8")
        self.assertNotIn("--signing-key", source)
        self.assertNotIn("sign_manifest", source)


if __name__ == "__main__":
    unittest.main()
