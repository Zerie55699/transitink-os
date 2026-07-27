import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class SecurityHardeningTests(unittest.TestCase):
    def test_all_firmware_https_clients_verify_the_pinned_ca(self):
        sources = "\n".join(
            path.read_text(encoding="utf-8")
            for path in (ROOT / "src").rglob("*.cpp")
        )
        self.assertNotIn("setInsecure", sources)
        self.assertGreaterEqual(sources.count("configureVerifiedTls(tls)"), 13)
        trust = read("include/TransitTlsTrust.h")
        self.assertIn("Hongkong Post Root CA 3", trust)
        self.assertIn("5A:2F:C0:3F", trust)
        updater = read("src/FirmwareUpdateService.cpp")
        self.assertIn("configureFirmwareUpdateVerifiedTls(tls)", updater)
        self.assertIn("mbedtls_sha256_update", updater)
        self.assertIn("digestHex(digest) != manifest.sha256", updater)
        self.assertIn("esp_ota_get_next_update_partition", updater)
        self.assertIn("Update.begin(manifest.size, U_FLASH)", updater)
        self.assertNotIn("U_LITTLEFS", updater)
        self.assertNotIn("setInsecure", updater)
        setup = read("src/main.cpp").split("void setup()", 1)[1].split(
            "void loop()", 1
        )[0]
        self.assertGreater(
            setup.index("confirmRunningFirmware"),
            setup.index("hasUsableConfig"),
        )

    def test_release_platform_and_direct_libraries_are_exactly_pinned(self):
        config = read("platformio.ini")
        self.assertIn("/55.03.39/platform-espressif32.zip", config)
        self.assertIn("-DCORE_DEBUG_LEVEL=0", config)
        self.assertNotRegex(config, r"(?m)^\s+[^#\n]*@\^")

    def test_installer_executes_no_remote_scripts(self):
        page = read("installer/index.html")
        dialog = read("installer/esp-web-tools/no-port-dialog-zh.js")
        self.assertIn("Content-Security-Policy", page)
        self.assertNotRegex(page, r'<script[^>]+src="https?://')
        self.assertNotRegex(dialog, r'from\s+["\']https?://')

    def test_actions_are_commit_pinned_and_permissions_are_explicit(self):
        for workflow in (ROOT / ".github" / "workflows").glob("*.yml"):
            source = workflow.read_text(encoding="utf-8")
            for target in re.findall(r"uses:\s+[^\s@]+@([^\s#]+)", source):
                self.assertRegex(target, r"^[0-9a-f]{40}$", workflow.name)
        release = read(".github/workflows/release.yml")
        self.assertIn("attest-build-provenance@", release)
        self.assertIn("permissions: {}", release)

    def test_codeql_keeps_platformio_dependencies_outside_the_source_root(self):
        codeql = read(".github/workflows/codeql.yml")
        self.assertIn(
            "PLATFORMIO_CORE_DIR: ${{ runner.temp }}/platformio",
            codeql,
        )
        self.assertIn(
            "PLATFORMIO_WORKSPACE_DIR: ${{ runner.temp }}/transitink-pio",
            codeql,
        )
        self.assertIn("build-mode: manual", codeql)
        self.assertNotIn('PLATFORMIO_CORE_DIR="$PWD/.platformio"', codeql)

    def test_ci_scans_complete_history_with_checksum_verified_gitleaks(self):
        ci = read(".github/workflows/ci.yml")
        self.assertNotIn("gitleaks/gitleaks-action", ci)
        self.assertIn('GITLEAKS_VERSION: "8.30.1"', ci)
        self.assertIn(
            "551f6fc83ea457d62a0d98237cbad105af8d557003051f41f3e7ca7b3f2470eb",
            ci,
        )
        self.assertIn("sha256sum --check", ci)
        self.assertIn("gitleaks git", ci)
        self.assertIn("--log-opts=--all", ci)

    def test_backups_are_private_and_ssid_is_not_logged(self):
        backup = read("scripts/backup_flash.sh")
        self.assertIn("umask 077", backup)
        self.assertIn('chmod 600 "$OUT"', backup)
        self.assertNotIn("Connecting Wi-Fi SSID", read("src/main.cpp"))

    def test_device_portal_sends_browser_security_headers(self):
        portal = read("src/ConfigPortal.cpp")
        page = read("src/TransitInkPortalPage.cpp")
        self.assertIn("server_.client().localIP() == expectedIp", portal)
        self.assertIn("WiFi.enableSTA(false)", portal)
        self.assertIn('server_.header("Origin")', portal)
        self.assertIn('server_.header("X-TransitInk-Access")', portal)
        self.assertIn("isPortalAccessTokenAuthorized", portal)
        self.assertIn("function portalFetch", page)
        self.assertEqual(1, len(re.findall(r"\bfetch\(", page)))
        self.assertIn('"Content-Security-Policy"', portal)
        self.assertIn('"X-Content-Type-Options", "nosniff"', portal)
        self.assertIn('"X-Frame-Options", "DENY"', portal)
        self.assertIn('"Referrer-Policy", "no-referrer"', portal)


if __name__ == "__main__":
    unittest.main()
