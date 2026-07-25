import os
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / ".test-build"
TEST_BIN = BUILD_DIR / "test_core"
BOARD_PROFILE_TEST_BIN = BUILD_DIR / "test_board_profile"
WIDGET_CONFIG_TEST_BIN = BUILD_DIR / "test_widget_config_core"
WIDGET_CORE_TEST_BIN = BUILD_DIR / "test_widget_core"
DISPLAY_TEXT_CORE_TEST_BIN = BUILD_DIR / "test_display_text_core"
WIDGET_SCHEDULER_TEST_BIN = BUILD_DIR / "test_widget_scheduler"
WIDGET_PROVIDER_ROUTER_TEST_BIN = BUILD_DIR / "test_widget_provider_router"
TRANSIT_CATALOG_TEST_BIN = BUILD_DIR / "test_transit_catalog"
RAIL_PROVIDERS_TEST_BIN = BUILD_DIR / "test_rail_providers"
GMB_PROVIDER_TEST_BIN = BUILD_DIR / "test_gmb_provider"
JOURNEY_TIME_XML_TEST_BIN = BUILD_DIR / "test_journey_time_xml"
JOURNEY_TIME_PROVIDER_TEST_BIN = BUILD_DIR / "test_journey_time_provider"
JOURNEY_TIME_CLIENT_TEST_BIN = BUILD_DIR / "test_journey_time_client"
YXML_OBJECT = BUILD_DIR / "yxml.o"
YXML_CLIENT_OBJECT = BUILD_DIR / "yxml_client.o"
NATIVE_CONFIG = ROOT / "platformio.native.ini"


class CoreBehaviorTests(unittest.TestCase):
    def test_selected_board_profile_cpp_contract(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-DTRANSITINK_BOARD_ZECTRIX_NOTE4=1",
            "-Iinclude",
            "test_host/test_board_profile.cpp",
            "-o",
            str(BOARD_PROFILE_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(BOARD_PROFILE_TEST_BIN)], cwd=ROOT, check=True)

    def test_app_config_native_behaviors(self):
        env = os.environ.copy()
        env["PLATFORMIO_CORE_DIR"] = str(ROOT / ".platformio")
        cmd = [
            str(ROOT / ".venv/bin/platformio"),
            "test",
            "-c",
            str(NATIVE_CONFIG),
            "-e",
            "native_app_config",
            "-f",
            "test_app_config",
        ]
        subprocess.run(cmd, cwd=ROOT, env=env, check=True)

    def test_json_native_behaviors(self):
        env = os.environ.copy()
        env["PLATFORMIO_CORE_DIR"] = str(ROOT / ".platformio")
        for test_filter in ("test_transit_json_parsers", "test_widget_catalog"):
            cmd = [
                str(ROOT / ".venv/bin/platformio"),
                "test",
                "-c",
                str(NATIVE_CONFIG),
                "-e",
                "native_json",
                "-f",
                test_filter,
            ]
            subprocess.run(cmd, cwd=ROOT, env=env, check=True)

    def test_core_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/BusEtaCore.cpp",
            "src/core/BatteryStatus.cpp",
            "test_host/test_core.cpp",
            "-o",
            str(TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(TEST_BIN)], cwd=ROOT, check=True)

    def test_widget_config_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/WidgetConfigCore.cpp",
            "src/core/BusEtaCore.cpp",
            "test_host/test_widget_config_core.cpp",
            "-o",
            str(WIDGET_CONFIG_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(WIDGET_CONFIG_TEST_BIN)], cwd=ROOT, check=True)

    def test_widget_core_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/WidgetCore.cpp",
            "src/core/UiText.cpp",
            "src/core/WidgetConfigCore.cpp",
            "src/core/BusEtaCore.cpp",
            "test_host/test_widget_core.cpp",
            "-o",
            str(WIDGET_CORE_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(WIDGET_CORE_TEST_BIN)], cwd=ROOT, check=True)

    def test_display_text_core_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/DisplayTextCore.cpp",
            "test_host/test_display_text_core.cpp",
            "-o",
            str(DISPLAY_TEXT_CORE_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(DISPLAY_TEXT_CORE_TEST_BIN)], cwd=ROOT, check=True)

    def test_widget_scheduler_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "src/core/WidgetCore.cpp",
            "src/core/UiText.cpp",
            "src/core/WidgetScheduler.cpp",
            "src/core/WidgetConfigCore.cpp",
            "src/core/BusEtaCore.cpp",
            "test_host/test_widget_scheduler.cpp",
            "-o",
            str(WIDGET_SCHEDULER_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(WIDGET_SCHEDULER_TEST_BIN)], cwd=ROOT, check=True)

    def test_widget_provider_router_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Itest_host/provider_router_shims",
            "-Itest_native/shims",
            "-Iinclude",
            "src/core/WidgetCore.cpp",
            "src/core/UiText.cpp",
            "src/core/WidgetScheduler.cpp",
            "src/core/WidgetConfigCore.cpp",
            "src/core/BusEtaCore.cpp",
            "src/providers/WidgetProviderRouter.cpp",
            "test_host/test_widget_provider_router.cpp",
            "-o",
            str(WIDGET_PROVIDER_ROUTER_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(WIDGET_PROVIDER_ROUTER_TEST_BIN)], cwd=ROOT, check=True)

    def test_transit_catalog_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "-Itest_native/shims",
            "src/TransitCatalog.cpp",
            "test_host/test_transit_catalog.cpp",
            "-o",
            str(TRANSIT_CATALOG_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(TRANSIT_CATALOG_TEST_BIN)], cwd=ROOT, check=True)

    def test_gmb_provider_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "-Itest_native/shims",
            "src/core/WidgetCore.cpp",
            "src/core/UiText.cpp",
            "src/core/WidgetConfigCore.cpp",
            "src/core/BusEtaCore.cpp",
            "src/providers/GmbProvider.cpp",
            "test_host/test_gmb_provider.cpp",
            "-o",
            str(GMB_PROVIDER_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(GMB_PROVIDER_TEST_BIN)], cwd=ROOT, check=True)

    def test_rail_provider_invalid_config_precedence(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "-Itest_native/shims",
            "src/core/WidgetCore.cpp",
            "src/core/UiText.cpp",
            "src/core/WidgetConfigCore.cpp",
            "src/core/BusEtaCore.cpp",
            "src/TransitCatalog.cpp",
            "src/providers/MtrProvider.cpp",
            "src/providers/LightRailProvider.cpp",
            "src/providers/TflRailProvider.cpp",
            "test_host/test_rail_providers.cpp",
            "-o",
            str(RAIL_PROVIDERS_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(RAIL_PROVIDERS_TEST_BIN)], cwd=ROOT, check=True)

    def test_journey_time_xml_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        subprocess.run(
            [
                "cc",
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-Wno-unused-parameter",
                "-Ilib/yxml",
                "-c",
                "lib/yxml/yxml.c",
                "-o",
                str(YXML_OBJECT),
            ],
            cwd=ROOT,
            check=True,
        )
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "-Ilib/yxml",
            "src/core/JourneyTimeXmlParser.cpp",
            "src/core/WidgetCore.cpp",
            "src/core/UiText.cpp",
            "src/core/WidgetConfigCore.cpp",
            "src/core/BusEtaCore.cpp",
            "test_host/test_journey_time_xml.cpp",
            str(YXML_OBJECT),
            "-o",
            str(JOURNEY_TIME_XML_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(JOURNEY_TIME_XML_TEST_BIN)], cwd=ROOT, check=True)

    def test_journey_time_provider_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Iinclude",
            "-Itest_native/shims",
            "src/core/WidgetCore.cpp",
            "src/core/UiText.cpp",
            "src/core/WidgetConfigCore.cpp",
            "src/core/BusEtaCore.cpp",
            "src/TransitCatalog.cpp",
            "src/providers/JourneyTimeProvider.cpp",
            "test_host/test_journey_time_provider.cpp",
            "-o",
            str(JOURNEY_TIME_PROVIDER_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(JOURNEY_TIME_PROVIDER_TEST_BIN)], cwd=ROOT, check=True)

    def test_journey_time_client_cpp_behaviors(self):
        BUILD_DIR.mkdir(exist_ok=True)
        subprocess.run(
            [
                "cc",
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-Wno-unused-parameter",
                "-Ilib/yxml",
                "-c",
                "lib/yxml/yxml.c",
                "-o",
                str(YXML_CLIENT_OBJECT),
            ],
            cwd=ROOT,
            check=True,
        )
        cmd = [
            "c++",
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Itest_host/journey_time_client_shims",
            "-Iinclude",
            "-Ilib/yxml",
            "src/JourneyTimeClient.cpp",
            "src/core/JourneyTimeXmlParser.cpp",
            "src/TransitCatalog.cpp",
            "test_host/test_journey_time_client.cpp",
            str(YXML_CLIENT_OBJECT),
            "-o",
            str(JOURNEY_TIME_CLIENT_TEST_BIN),
        ]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(JOURNEY_TIME_CLIENT_TEST_BIN)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
