import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "package_installer", ROOT / "scripts" / "package_installer.py"
)
PACKAGE_INSTALLER = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(PACKAGE_INSTALLER)


class ReleasePackagingTests(unittest.TestCase):
    def test_firmware_version_comes_from_product_config(self):
        self.assertEqual("1.0.0", PACKAGE_INSTALLER.firmware_version())

    def test_manifest_uses_relative_versioned_firmware_asset(self):
        manifest = PACKAGE_INSTALLER.installer_manifest(
            "1.2.3", "transitink-zectrix-note4-v1.2.3.bin"
        )
        self.assertEqual("TransitInk OS Installer", manifest["name"])
        self.assertEqual("1.2.3", manifest["version"])
        self.assertTrue(manifest["new_install_prompt_erase"])
        build = manifest["builds"][0]
        self.assertEqual("ESP32-S3", build["chipFamily"])
        self.assertEqual(
            {
                "path": "firmware/transitink-zectrix-note4-v1.2.3.bin",
                "offset": 0,
            },
            build["parts"][0],
        )

    def test_version_parser_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / "ProductConfig.h"
            missing.write_text('#define FIRMWARE_VERSION "dev"\n', encoding="utf-8")
            with self.assertRaises(ValueError):
                PACKAGE_INSTALLER.firmware_version(missing)

    def test_installer_source_has_no_committed_manifest_or_binary(self):
        self.assertFalse((ROOT / "installer" / "manifest.json").exists())
        self.assertEqual([], list((ROOT / "installer").rglob("*.bin")))
        page = (ROOT / "installer" / "index.html").read_text(encoding="utf-8")
        self.assertIn('manifest="./manifest.json"', page)
        self.assertIn('fetch("./manifest.json"', page)

    def test_release_workflow_builds_one_tag_matched_pages_package(self):
        workflow = (ROOT / ".github" / "workflows" / "release.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn('      - "v*.*.*"', workflow)
        self.assertIn('--expected-version "${RELEASE_TAG#v}"', workflow)
        self.assertIn("dist/installer/firmware/*.bin", workflow)
        self.assertIn("actions/upload-pages-artifact@v3", workflow)
        self.assertIn("actions/deploy-pages@v4", workflow)
        self.assertNotIn("zectrix-note4-installer.git", workflow)


if __name__ == "__main__":
    unittest.main()
