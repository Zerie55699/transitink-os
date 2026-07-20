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
        for provider in ("kmb", "ctb", "gmb"):
            with gzip.open(
                ROOT / "data/catalog" / f"stops-{provider}.json.gz",
                "rt",
                encoding="utf-8",
            ) as stream:
                packs[provider] = json.load(stream)["routes"]

        for provider, index_key in (("kmb", "kmb_lwb"), ("ctb", "ctb")):
            for directions in index["bus"][index_key]["routes"].values():
                for direction in directions:
                    stops = packs[provider][direction["stop_key"]]
                    sequences = [stop["sequence"] for stop in stops]
                    self.assertEqual(sequences, sorted(set(sequences)))
                    self.assertTrue(all(stop["id"] and stop["label_tc"] for stop in stops))

        for directions in index["gmb"]["routes"].values():
            for direction in directions:
                stops = packs["gmb"][direction["stop_key"]]
                self.assertTrue(direction["region"] and direction["route_id"])
                self.assertTrue(
                    all(stop["stop_id"] and stop["stop_seq"] and stop["label_tc"] for stop in stops)
                )

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
