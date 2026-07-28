import importlib.util
import hashlib
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "package_installer", ROOT / "scripts" / "package_installer.py"
)
PACKAGE_INSTALLER = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(PACKAGE_INSTALLER)

PAGES_SPEC = importlib.util.spec_from_file_location(
    "assemble_installer_pages", ROOT / "scripts" / "assemble_installer_pages.py"
)
ASSEMBLE_INSTALLER_PAGES = importlib.util.module_from_spec(PAGES_SPEC)
assert PAGES_SPEC.loader is not None
PAGES_SPEC.loader.exec_module(ASSEMBLE_INSTALLER_PAGES)


class ReleasePackagingTests(unittest.TestCase):
    def test_firmware_version_comes_from_product_config(self):
        self.assertEqual("1.1.3", PACKAGE_INSTALLER.firmware_version())

    def test_merge_preserves_flash_mode_and_matches_factory_image(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            build.mkdir()
            for filename in ("bootloader.bin", "partitions.bin", "firmware.bin"):
                (build / filename).write_bytes(b"input")
            (build / "firmware.factory.bin").write_bytes(b"factory")
            core = root / "core"
            boot_app0 = (
                core
                / "packages"
                / "framework-arduinoespressif32"
                / "tools"
                / "partitions"
                / "boot_app0.bin"
            )
            boot_app0.parent.mkdir(parents=True)
            boot_app0.write_bytes(b"input")
            output = root / "output.bin"
            commands = []

            def fake_run(command, cwd, check):
                commands.append(command)
                Path(command[command.index("--output") + 1]).write_bytes(b"factory")

            with mock.patch.object(
                PACKAGE_INSTALLER, "platformio_core_dir", return_value=core
            ), mock.patch.object(
                PACKAGE_INSTALLER.subprocess, "run", side_effect=fake_run
            ):
                PACKAGE_INSTALLER.merge_firmware(build, output)

            command = commands[0]
            self.assertEqual("keep", command[command.index("--flash-mode") + 1])
            self.assertNotIn("qio", command)

    def test_merge_rejects_output_that_differs_from_factory_image(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            build.mkdir()
            for filename in ("bootloader.bin", "partitions.bin", "firmware.bin"):
                (build / filename).write_bytes(b"input")
            (build / "firmware.factory.bin").write_bytes(b"factory")
            core = root / "core"
            boot_app0 = (
                core
                / "packages"
                / "framework-arduinoespressif32"
                / "tools"
                / "partitions"
                / "boot_app0.bin"
            )
            boot_app0.parent.mkdir(parents=True)
            boot_app0.write_bytes(b"input")
            output = root / "output.bin"

            def fake_run(command, cwd, check):
                Path(command[command.index("--output") + 1]).write_bytes(b"wrong")

            with mock.patch.object(
                PACKAGE_INSTALLER, "platformio_core_dir", return_value=core
            ), mock.patch.object(
                PACKAGE_INSTALLER.subprocess, "run", side_effect=fake_run
            ):
                with self.assertRaisesRegex(
                    ValueError, "differs from PlatformIO factory image"
                ):
                    PACKAGE_INSTALLER.merge_firmware(build, output)

    def test_manifest_uses_relative_versioned_firmware_asset(self):
        manifest = PACKAGE_INSTALLER.installer_manifest(
            "1.2.3", "transitink-zectrix-note4-v1.2.3.bin"
        )
        self.assertEqual("TransitInk OS Installer", manifest["name"])
        self.assertEqual("1.2.3", manifest["version"])
        self.assertFalse(manifest["new_install_prompt_erase"])
        build = manifest["builds"][0]
        self.assertEqual("ESP32-S3", build["chipFamily"])
        self.assertEqual(
            {
                "path": "firmware/transitink-zectrix-note4-v1.2.3.bin",
                "offset": 0,
            },
            build["parts"][0],
        )

    def test_ota_manifest_is_board_scoped_and_hash_pinned(self):
        digest = "a" * 64
        manifest = PACKAGE_INSTALLER.ota_manifest(
            "1.2.3",
            "transitink-zectrix-note4-ota-v1.2.3.bin",
            123456,
            digest,
        )
        self.assertEqual(1, manifest["schema_version"])
        self.assertEqual("transitink-os", manifest["product"])
        self.assertEqual("zectrix_note4", manifest["board"])
        self.assertEqual("1.2.3", manifest["version"])
        self.assertEqual(
            "firmware/transitink-zectrix-note4-ota-v1.2.3.bin",
            manifest["firmware"],
        )
        self.assertEqual(123456, manifest["size"])
        self.assertEqual(digest, manifest["sha256"])

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
        app = (ROOT / "installer" / "app.js").read_text(encoding="utf-8")
        self.assertIn('manifest="./manifest.json"', page)
        self.assertIn("刷機風險與備份責任", page)
        self.assertIn("本頁每次安裝都會先清除裝置資料", page)
        self.assertIn("本頁不提供保留設定的韌體更新", page)
        self.assertIn("安裝工具不會自動備份任何資料", page)
        self.assertIn("連接裝置並完整安裝", page)
        self.assertIn('id="install-paths"', page)
        self.assertIn('href="#install-paths">選擇安裝方式</a>', page)
        self.assertIn("首次安裝／清除安裝", page)
        self.assertIn("會清除裝置設定", page)
        self.assertIn("更新韌體並保留設定", page)
        self.assertIn("保留裝置設定", page)
        self.assertIn("如果設定頁沒有「檢查韌體更新」", page)
        self.assertIn('href="#firmware-update">查看更新步驟</a>', page)
        self.assertIn("更新並保留設定", page)
        self.assertIn("更新不在本頁刷寫", page)
        self.assertIn("風險與責任由使用者承擔", page)
        self.assertIn("與 Zectrix 沒有從屬或認可關係", page)
        self.assertIn('./legal/THIRD_PARTY_NOTICES.md', page)
        self.assertIn('./legal/THIRD_PARTY_DATA.md', page)
        self.assertIn('fetch("./devices.json"', app)
        self.assertIn('http-equiv="Content-Security-Policy"', page)
        self.assertIn(
            'src="./esp-web-tools/vendor/install-button.js"', page
        )
        self.assertIn(
            'src="./esp-web-tools/full-erase-policy.js?', page
        )
        self.assertNotIn("unpkg.com", page)
        no_port_dialog = (
            ROOT / "installer" / "esp-web-tools" / "no-port-dialog-zh.js"
        ).read_text(encoding="utf-8")
        install_button = (
            ROOT / "installer" / "esp-web-tools" / "vendor" / "install-button.js"
        ).read_text(encoding="utf-8")
        self.assertNotIn("unpkg.com", no_port_dialog)
        self.assertTrue(no_port_dialog.startswith("// Modified by TransitInk OS"))
        self.assertTrue(install_button.startswith("// Modified by TransitInk OS"))
        erase_policy = (
            ROOT / "installer" / "esp-web-tools" / "full-erase-policy.js"
        ).read_text(encoding="utf-8")
        self.assertIn(
            'import "./vendor/install-dialog-C5LjR_e6.js"', erase_policy
        )
        self.assertIn("startInstall.call(this, true)", erase_policy)

    def test_project_documents_non_endorsement_and_image_provenance(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        notices = (ROOT / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8")
        image_source = (ROOT / "installer" / "assets" / "SOURCE.md").read_text(
            encoding="utf-8"
        )
        for document in (readme, notices, image_source):
            self.assertIn("與 Zectrix 沒有從屬或認可關係", document)
        self.assertIn("1050ddeb3e6be7f435df4df760a7d13e5", image_source)
        self.assertIn("AI-assisted product visual", image_source)

    def test_project_license_is_noncommercial_and_source_available(self):
        licence = (ROOT / "LICENSE").read_text(encoding="utf-8")
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        contributing = (ROOT / "CONTRIBUTING.md").read_text(encoding="utf-8")
        corresponding_source = (ROOT / "CORRESPONDING_SOURCE.md").read_text(
            encoding="utf-8"
        )
        notices = (ROOT / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8")
        installer = (ROOT / "installer" / "index.html").read_text(
            encoding="utf-8"
        )

        self.assertIn("PolyForm Noncommercial License 1.0.0", licence)
        self.assertIn(
            "Required Notice: Copyright 2026 TransitInk OS contributors.",
            licence,
        )
        self.assertIn("Commercial use requires a", readme)
        for document in (readme, contributing, corresponding_source, notices):
            self.assertRegex(
                document, r"PolyForm\s+Noncommercial License 1\.0\.0"
            )
        for document in (readme, notices, installer):
            self.assertNotIn("open-source project", document)
            self.assertNotIn("獨立開源專案", document)
        self.assertIn("source-available project", readme)
        self.assertIn("PolyForm Noncommercial 1.0.0", installer)

    def test_device_catalog_is_versioned_and_extensible(self):
        catalog = json.loads(
            (ROOT / "installer" / "devices.json").read_text(encoding="utf-8")
        )
        self.assertEqual(1, catalog["schema_version"])
        self.assertEqual("zectrix_note4", catalog["devices"][0]["id"])
        self.assertEqual("./manifest.json", catalog["devices"][0]["manifest"])
        self.assertEqual(
            "./assets/zectrix-note4-product.png?v=1050ddeb",
            catalog["devices"][0]["image"],
        )
        self.assertTrue(catalog["devices"][0]["installable"])

    def test_package_copies_page_and_device_catalog_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "installer"
            build = Path(directory) / "build"
            build.mkdir()
            (build / "firmware.bin").write_bytes(b"ota firmware")

            def fake_merge(_build_dir, output_file):
                output_file.write_bytes(b"test firmware")

            with mock.patch.object(
                PACKAGE_INSTALLER, "merge_firmware", side_effect=fake_merge
            ):
                PACKAGE_INSTALLER.package_installer(
                    build, output, "1.1.3"
                )

            for filename in (
                "index.html",
                "styles.css",
                "app.js",
                "devices.json",
                "manifest.json",
                "ota-manifest.json",
                "firmware/transitink-zectrix-note4-ota-v1.1.3.bin",
                "assets/zectrix-note4-product.png",
                "esp-web-tools/LICENSE",
                "esp-web-tools/THIRD_PARTY_NOTICES.md",
                "esp-web-tools/vendor/install-button.js",
                "legal/LICENSE.txt",
                "legal/THIRD_PARTY_NOTICES.md",
                "legal/THIRD_PARTY_DATA.md",
                "legal/CORRESPONDING_SOURCE.md",
                "legal/PRODUCT_IMAGE_SOURCE.md",
                "legal/ARDUINO_ESP32_BUILD_VERSIONS.txt",
                "legal/licenses/LGPL-2.1.txt",
                "legal/licenses/Noto-Sans-CJK-HK-OFL-1.1.txt",
                "legal/GNU_UNIFONT_SOURCE.md",
                "legal/licenses/GNU-Unifont-OFL-1.1.txt",
                "legal/licenses/ArduinoJson-MIT.txt",
                "legal/licenses/Adafruit-GFX-BSD-3-Clause.txt",
                "legal/licenses/ESP-Web-Tools-Apache-2.0.txt",
                "legal/licenses/Improv-WiFi-Serial-SDK-Apache-2.0.txt",
                "legal/licenses/Lit-BSD-3-Clause.txt",
                "legal/licenses/Material-Web-Apache-2.0.txt",
                "legal/licenses/Pako-MIT.txt",
                "legal/licenses/QRCode-MIT.txt",
                "legal/licenses/Slate-MIT.txt",
                "legal/licenses/VibecodingVoice-MIT.txt",
                "legal/licenses/atob-lite-MIT.txt",
                "legal/licenses/esptool-js-Apache-2.0.txt",
                "legal/licenses/tslib-0BSD.txt",
            ):
                self.assertTrue((output / filename).is_file(), filename)

            bundle = output / "transitink-zectrix-note4-v1.1.3.zip"
            self.assertTrue(bundle.is_file())
            with zipfile.ZipFile(bundle) as archive:
                names = set(archive.namelist())
            self.assertIn("transitink-zectrix-note4-v1.1.3.bin", names)
            self.assertIn("transitink-zectrix-note4-ota-v1.1.3.bin", names)
            self.assertIn("ota-manifest.json", names)
            self.assertIn("SHA256SUMS.txt", names)
            self.assertIn("legal/LICENSE.txt", names)
            self.assertIn("legal/THIRD_PARTY_DATA.md", names)
            self.assertIn("legal/licenses/LGPL-2.1.txt", names)

    def test_release_workflow_builds_one_tag_matched_pages_package(self):
        workflow = (ROOT / ".github" / "workflows" / "release.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn('      - "v*.*.*"', workflow)
        self.assertIn('--expected-version "${RELEASE_TAG#v}"', workflow)
        self.assertIn("dist/installer/firmware/*.bin", workflow)
        self.assertIn("dist/installer/*.zip", workflow)
        self.assertIn("dist/installer/legal/THIRD_PARTY_NOTICES.md", workflow)
        self.assertIn("dist/installer/legal/THIRD_PARTY_DATA.md", workflow)
        self.assertIn("dist/installer/legal/CORRESPONDING_SOURCE.md", workflow)
        self.assertIn("actions/attest-build-provenance@", workflow)
        self.assertIn("GH_REPO: ${{ github.repository }}", workflow)
        self.assertIn("actions/upload-pages-artifact@fc324d35", workflow)
        self.assertIn("actions/deploy-pages@cd2ce8fc", workflow)
        self.assertNotIn("uses: actions/checkout@v", workflow)
        self.assertNotIn("zectrix-note4-installer.git", workflow)

    def test_installer_pages_use_latest_release_firmware(self):
        workflow = (
            ROOT / ".github" / "workflows" / "installer-pages.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("branches: [main]", workflow)
        self.assertIn('- "installer/**"', workflow)
        self.assertIn("gh release download", workflow)
        self.assertIn("scripts/assemble_installer_pages.py", workflow)
        self.assertIn("actions/upload-pages-artifact@fc324d35", workflow)
        self.assertIn("actions/deploy-pages@cd2ce8fc", workflow)
        self.assertNotIn("platformio run", workflow)
        self.assertNotIn("scripts/package_installer.py", workflow)

    def test_pages_assembler_overlays_ui_without_rebuilding_firmware(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            version = "1.1.0"
            firmware_name = f"transitink-zectrix-note4-v{version}.bin"
            bundle_name = f"transitink-zectrix-note4-v{version}.zip"
            firmware = b"released firmware"
            ota_firmware_name = (
                f"transitink-zectrix-note4-ota-v{version}.bin"
            )
            ota_firmware = b"released ota firmware"
            digest = hashlib.sha256(firmware).hexdigest()
            ota_digest = hashlib.sha256(ota_firmware).hexdigest()
            bundle = root / bundle_name
            manifest = PACKAGE_INSTALLER.installer_manifest(version, firmware_name)
            metadata = {
                "version": version,
                "board": "zectrix_note4",
                "chip_family": "ESP32-S3",
                "firmware": firmware_name,
                "bundle": bundle_name,
                "size": len(firmware),
                "sha256": digest,
                "ota_firmware": ota_firmware_name,
                "ota_size": len(ota_firmware),
                "ota_sha256": ota_digest,
            }
            ota_manifest = PACKAGE_INSTALLER.ota_manifest(
                version,
                ota_firmware_name,
                len(ota_firmware),
                ota_digest,
            )
            with zipfile.ZipFile(bundle, "w") as archive:
                archive.writestr(firmware_name, firmware)
                archive.writestr(ota_firmware_name, ota_firmware)
                archive.writestr(
                    "manifest.json",
                    json.dumps(manifest),
                )
                archive.writestr(
                    "ota-manifest.json",
                    json.dumps(ota_manifest),
                )
                archive.writestr(
                    "release-metadata.json",
                    json.dumps(metadata),
                )
                archive.writestr(
                    "SHA256SUMS.txt",
                    f"{digest}  {firmware_name}\n"
                    f"{ota_digest}  {ota_firmware_name}\n",
                )
                archive.writestr("legal/PRODUCT_IMAGE_SOURCE.md", "old")

            output = root / "site"
            ASSEMBLE_INSTALLER_PAGES.assemble_pages(bundle, output)

            self.assertEqual(firmware, (output / "firmware" / firmware_name).read_bytes())
            self.assertEqual(
                ota_firmware,
                (output / "firmware" / ota_firmware_name).read_bytes(),
            )
            self.assertEqual(
                ota_manifest,
                json.loads((output / "ota-manifest.json").read_text()),
            )
            self.assertFalse((output / firmware_name).exists())
            self.assertTrue((output / bundle_name).is_file())
            self.assertEqual(
                (ROOT / "installer" / "index.html").read_bytes(),
                (output / "index.html").read_bytes(),
            )
            self.assertEqual(
                (ROOT / "installer" / "assets" / "SOURCE.md").read_bytes(),
                (output / "legal" / "PRODUCT_IMAGE_SOURCE.md").read_bytes(),
            )

    def test_pages_assembler_accepts_previous_release_without_ota_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            version = "1.1.0"
            firmware_name = f"transitink-zectrix-note4-v{version}.bin"
            bundle_name = f"transitink-zectrix-note4-v{version}.zip"
            firmware = b"legacy released firmware"
            digest = hashlib.sha256(firmware).hexdigest()
            manifest = PACKAGE_INSTALLER.installer_manifest(version, firmware_name)
            metadata = {
                "version": version,
                "board": "zectrix_note4",
                "chip_family": "ESP32-S3",
                "firmware": firmware_name,
                "bundle": bundle_name,
                "size": len(firmware),
                "sha256": digest,
            }
            bundle = root / bundle_name
            with zipfile.ZipFile(bundle, "w") as archive:
                archive.writestr(firmware_name, firmware)
                archive.writestr("manifest.json", json.dumps(manifest))
                archive.writestr("release-metadata.json", json.dumps(metadata))
                archive.writestr(
                    "SHA256SUMS.txt", f"{digest}  {firmware_name}\n"
                )
                archive.writestr("legal/PRODUCT_IMAGE_SOURCE.md", "old")

            output = root / "site"
            ASSEMBLE_INSTALLER_PAGES.assemble_pages(bundle, output)

            self.assertEqual(
                firmware,
                (output / "firmware" / firmware_name).read_bytes(),
            )
            self.assertFalse((output / "ota-manifest.json").exists())


if __name__ == "__main__":
    unittest.main()
