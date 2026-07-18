import hashlib
import importlib.util
import re
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read_text(path):
    return (ROOT / path).read_text(encoding="utf-8")


def cpp_function_body(source, signature):
    signature_start = source.index(signature)
    while True:
        after_signature = signature_start + len(signature)
        opening_brace = source.find("{", after_signature)
        semicolon = source.find(";", after_signature)
        if opening_brace >= 0 and (semicolon < 0 or opening_brace < semicolon):
            break
        signature_start = source.index(signature, after_signature)
    depth = 0
    quote = None
    escaped = False
    line_comment = False
    block_comment = False
    index = opening_brace
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if line_comment:
            line_comment = char != "\n"
        elif block_comment:
            if char == "*" and next_char == "/":
                block_comment = False
                index += 1
        elif quote is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
        elif char == "/" and next_char == "/":
            line_comment = True
            index += 1
        elif char == "/" and next_char == "*":
            block_comment = True
            index += 1
        elif char in ('"', "'"):
            quote = char
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[opening_brace + 1:index]
        index += 1
    raise AssertionError(f"Unclosed function body: {signature}")


def assert_fragments_in_order(test_case, body, *fragments):
    position = -1
    for fragment in fragments:
        next_position = body.find(fragment, position + 1)
        test_case.assertGreater(next_position, position, fragment)
        position = next_position


class ProjectStructureTests(unittest.TestCase):
    def test_platformio_targets_zectrix_esp32s3(self):
        ini = read_text("platformio.ini")
        self.assertIn("[env:zectrix_note4]", ini)
        self.assertRegex(ini, r"platform\s*=\s*espressif32")
        self.assertRegex(ini, r"framework\s*=\s*arduino")
        self.assertRegex(ini, r"board_upload\.flash_size\s*=\s*16MB")
        self.assertRegex(ini, r"board_build\.arduino\.memory_type\s*=\s*qio_opi")
        self.assertIn("-DBOARD_HAS_PSRAM", ini)
        self.assertIn("bblanchon/ArduinoJson", ini)
        self.assertNotIn("U8g2_for_Adafruit_GFX", ini)
        self.assertNotIn("zinggjm/GxEPD2", ini)

    def test_board_config_uses_zectrix_reference_pins(self):
        selector = read_text("include/hardware/BoardProfile.h")
        profile = read_text("include/hardware/boards/ZectrixNote4.h")
        host_test = read_text("test_host/test_board_profile.cpp")
        self.assertIn("TRANSITINK_BOARD_ZECTRIX_NOTE4", selector)
        self.assertIn("inline constexpr BoardProfile kBoardProfile", profile)
        for assertion in (
            "display.dcPin == 10",
            "display.chipSelectPin == 11",
            "display.clockPin == 12",
            "display.mosiPin == 13",
            "display.resetPin == 9",
            "display.busyPin == 8",
            "display.powerPin == 6",
            "buttons.homePin == 0",
            "buttons.upPin == 39",
            "buttons.downPin == 18",
        ):
            self.assertIn(assertion, host_test)

    def test_transitink_branding_keeps_firmware_version_unchanged(self):
        config = read_text("include/ProductConfig.h")
        readme = read_text("README.md")
        display = read_text("src/EInkDisplay.cpp")
        portal = read_text("src/TransitInkPortalPage.cpp")
        self.assertIn('#define FIRMWARE_PRODUCT_NAME "TransitInk OS"', config)
        self.assertIn('#define FIRMWARE_SHORT_NAME "TransitInk"', config)
        self.assertIn("#define CONFIG_AP_PREFIX FIRMWARE_SHORT_NAME", config)
        self.assertIn('#define FIRMWARE_VERSION "1.0.0"', config)
        self.assertNotIn("Bus ETA Note 4", config + readme)
        self.assertNotIn("巴士 ETA", config + readme)
        self.assertTrue(readme.startswith("# TransitInk OS"))
        self.assertIn("drawText(18, 42, FIRMWARE_PRODUCT_NAME);", display)
        self.assertIn('#include "ProductConfig.h"', portal)
        self.assertEqual(2, portal.count("FIRMWARE_PRODUCT_NAME"))
        self.assertEqual(1, portal.count("FIRMWARE_SHORT_NAME"))
        self.assertNotIn("<strong>TransitInk</strong>", portal)
        self.assertNotIn(">TransitInk<", portal)
        self.assertNotIn("TransitInk OS", display + portal)

    def test_visible_status_copy_uses_shared_brand_constants(self):
        main = read_text("src/main.cpp")
        display = read_text("src/EInkDisplay.cpp")
        portal = read_text("src/TransitInkPortalPage.cpp")
        self.assertNotIn('String(FIRMWARE_SHORT_NAME) + " 未同步"', display)
        self.assertIn('String("連接此熱點\\n開啟 http://192.168.4.1/\\n完成 ") +', main)
        self.assertIn('FIRMWARE_SHORT_NAME + " 設定"', main)

        brand_literals = []
        for source in (main, display, portal):
            production_lines = "\n".join(
                line for line in source.splitlines()
                if not line.lstrip().startswith("#include")
            )
            for double_quoted, single_quoted in re.findall(
                r'"([^"\n]*)"|\'([^\'\n]*)\'', production_lines
            ):
                literal = double_quoted or single_quoted
                if "TransitInk" in literal:
                    brand_literals.append(literal)
        self.assertEqual(["X-TransitInk-CSRF"], brand_literals)

    def test_flash_scripts_keep_backup_restore_path(self):
        backup = read_text("scripts/backup_flash.sh")
        restore = read_text("scripts/restore_flash.sh")
        self.assertIn("read_flash", backup)
        self.assertIn('FLASH_SIZE_HEX="${ESP32_FLASH_SIZE:-0x1000000}"', backup)
        self.assertIn('CHIP="${ESP32_CHIP:-esp32s3}"', backup)
        self.assertIn("write_flash 0", restore)
        self.assertIn('FLASH_SIZE_HEX="${ESP32_FLASH_SIZE:-0x1000000}"', restore)

    def test_firmware_assembles_transitink_widget_runtime_without_legacy_eta(self):
        main = read_text("src/main.cpp")
        assert_fragments_in_order(
            self,
            main,
            "KmbClient kmbClient;",
            "CitybusClient citybusClient;",
            "GmbClient gmbClient;",
            "MtrClient mtrClient;",
            "LightRailClient lightRailClient;",
            "JourneyTimeClient journeyTimeClient;",
            "BusProvider busProvider(kmbClient, citybusClient);",
            "GmbProvider gmbProvider(gmbClient);",
            "MtrProvider mtrProvider(mtrClient);",
            "LightRailProvider lightRailProvider(lightRailClient);",
            "JourneyTimeProvider journeyTimeProvider(journeyTimeClient);",
            "WidgetProviderRouter widgetProviderRouter",
            "transitink::WidgetScheduler widgetScheduler(widgetProviderRouter);",
            "WidgetCatalogService widgetCatalogService(kmbClient, citybusClient, gmbClient);",
            "ConfigPortal configPortal(deviceConfig, configStore, widgetCatalogService);",
        )
        self.assertIn("configTzTime", main)
        self.assertIn("HKT-8", main)
        self.assertIn("waitForTimeSync", main)
        self.assertNotIn("正在連接 Wi-Fi", main)
        self.assertIn("Wi-Fi 未連接", main)
        self.assertIn("連接此熱點", main)
        self.assertNotIn("EtaController", main)
        self.assertNotIn("nextEtaRefreshMs", main)
        self.assertNotIn("deviceConfig.refreshSeconds", main)
        self.assertNotIn("deviceConfig.routes", main)
        self.assertNotIn('einkDisplay.showWifiStatus("已連接 " + WiFi.localIP().toString())', main)

    def test_home_button_is_sleep_wakeup_not_factory_reset(self):
        profile_test = read_text("test_host/test_board_profile.cpp")
        self.assertIn("buttons.homePin == 0", profile_test)
        main = read_text("src/main.cpp")
        support = read_text("src/hardware/BoardSupport.cpp")
        self.assertIn("transitink::hardware::configureHomeWakeup()", main)
        self.assertIn("transitink::hardware::homeButtonPressed()", main)
        self.assertIn("gpio_wakeup_enable", support)
        self.assertIn("rtc_gpio_deinit", support)
        self.assertIn("ESP_SLEEP_WAKEUP_GPIO", main)
        self.assertNotIn("pinMode(", main)
        self.assertNotIn("放開主頁鍵後重啟", main)

    def test_volume_buttons_long_press_factory_resets_device(self):
        profile_test = read_text("test_host/test_board_profile.cpp")
        self.assertIn("buttons.factoryResetUpPin == 39", profile_test)
        self.assertIn("buttons.factoryResetDownPin == 18", profile_test)
        self.assertIn("buttons.factoryResetHoldMs == 5000", profile_test)
        self.assertIn("buttons.configPin == 39", profile_test)
        self.assertIn("buttons.configDebounceMs == 30", profile_test)
        self.assertIn("buttons.configMaxClickMs == 1200", profile_test)

        main = read_text("src/main.cpp")
        self.assertIn("DualButtonHoldDetector", main)
        self.assertIn("kBoardProfile.buttons.configDebounceMs", main)
        self.assertIn("factoryResetUpButtonPressed()", main)
        self.assertIn("factoryResetDownButtonPressed()", main)
        self.assertIn("configButtonPressed()", main)
        self.assertIn("configStore.clear()", main)
        self.assertIn("WiFi.disconnect(true, true)", main)
        self.assertIn("LittleFS.format()", main)
        self.assertIn("已重設裝置", main)
        self.assertIn("放開音量鍵後重啟", main)
        self.assertIn("factoryResetPendingRestart", main)
        self.assertIn("ESP.restart()", main)

    def test_volume_up_click_opens_config_access_screen(self):
        main = read_text("src/main.cpp")
        self.assertIn("void showConfigAccessScreen()", main)
        self.assertIn("void returnToDashboard()", main)
        self.assertIn("void serviceConfigButton()", main)
        self.assertIn("Config button clicked", main)
        self.assertIn("Config button clicked: returning to dashboard", main)
        self.assertIn("configButtonDetector.update(configPressed, downPressed || factoryResetPendingRestart, millis())", main)
        self.assertIn("if (configAccessMode) {\n            returnToDashboard();\n        } else {\n            showConfigAccessScreen();\n        }", main)
        self.assertIn("configPortal.begin(WiFi.status() != WL_CONNECTED)", main)
        self.assertIn("裝置設定", main)
        self.assertIn("目前 Wi-Fi", main)
        self.assertIn("einkDisplay.showConfigMode(networkName, message, configUrl)", main)
        self.assertIn("configPortal.stop();", main)
        self.assertIn("configAccessMode = false", main)
        self.assertIn("refreshAllWidgetsNow();", main)
        self.assertIn("serviceConfigButton();", main)

    def test_config_portal_begin_is_idempotent(self):
        header = read_text("include/ConfigPortal.h")
        self.assertIn("void stop();", header)
        self.assertIn("bool routesRegistered_ = false", header)
        self.assertIn("bool serverStarted_ = false", header)
        self.assertIn("bool isStarted() const { return serverStarted_; }", header)

        portal = read_text("src/ConfigPortal.cpp")
        self.assertIn("if (!routesRegistered_)", portal)
        self.assertIn("if (!serverStarted_)", portal)
        self.assertRegex(portal, r"ConfigPortal::loop\(\)[\s\S]*if \(!serverStarted_\) \{[\s\S]*return;[\s\S]*\}")
        self.assertIn("void ConfigPortal::stop()", portal)
        self.assertIn("server_.stop();", portal)
        self.assertIn("dns_.stop();", portal)
        self.assertIn("WiFi.softAPdisconnect(true);", portal)
        self.assertIn("WiFi.mode(WIFI_STA);", portal)
        self.assertIn("serverStarted_ = false;", portal)
        self.assertIn("apMode_ = false;", portal)

    def test_config_portal_starts_only_for_missing_or_invalid_persisted_config(self):
        main = read_text("src/main.cpp")
        self.assertIn("Config portal deferred until button press", main)
        self.assertRegex(main, r"if \(!loaded \|\| !hasUsableConfig\(deviceConfig\)\) \{[\s\S]*configPortal\.begin\(true\);")
        self.assertIn("if (deviceConfig.sleepEnabled && sleepMaintenanceWake) {", main)
        self.assertIn("const bool homeWake = wakeCause == ESP_SLEEP_WAKEUP_GPIO", main)
        self.assertIn("wakeStartedAtMs = millis();", main)
        self.assertIn("refreshAllWidgetsNow();", main)
        self.assertNotIn("routesConfigured", main)
        self.assertNotIn("hasEnabledWidgets()", main)
        self.assertNotIn("enterSleepMode(\"idle boot\")", main)
        self.assertNotIn("configPortal.begin(forceAp);", main)

    def test_button_mapper_diagnostic_firmware_maps_four_physical_buttons(self):
        ini = read_text("platformio.ini")
        self.assertIn("[env:button_mapper]", ini)
        self.assertIn("-D BUTTON_MAPPER", ini)

        main = read_text("src/main.cpp")
        self.assertIn("#ifndef BUTTON_MAPPER", main)
        self.assertIn("#endif  // BUTTON_MAPPER", main)

        mapper = read_text("src/button_mapper.cpp")
        self.assertIn("#ifdef BUTTON_MAPPER", mapper)
        for label in ("針孔鍵", "音量上", "音量下", "Home Button"):
            self.assertIn(label, mapper)
        for gpio in ("buttons.homePin", "buttons.upPin", "buttons.downPin", "GPIO_NUM_1", "GPIO_NUM_2", "GPIO_NUM_5", "GPIO_NUM_7"):
            self.assertIn(gpio, mapper)
        self.assertIn("Preferences", mapper)
        self.assertIn("preferences.clear()", mapper)
        self.assertNotIn("pin_rst_wait", mapper)
        self.assertNotIn("resumePinholeIfNeeded", mapper)
        self.assertIn("esp_reset_reason()", mapper)
        self.assertIn("Press the requested button", mapper)

    def test_all_disabled_widgets_are_a_valid_dashboard_configuration(self):
        main = read_text("src/main.cpp")
        config = read_text("src/AppConfig.cpp")
        self.assertIn("return areWidgetsValid(config);", config)
        self.assertNotIn("hasEnabledWidgets()", main)
        self.assertNotIn("尚未選擇路線", main)
        self.assertNotIn("no routes configured", main)

    def test_eink_setup_screen_has_polished_copy_and_status_icons(self):
        header = read_text("include/EInkDisplay.h")
        self.assertIn("showConfigMode(const String& ssid, const String& url, const String& qrUrl = \"\")", header)

        display = read_text("src/EInkDisplay.cpp")
        self.assertIn("drawStatusBar", display)
        self.assertIn("drawWifiIcon", display)
        self.assertIn("drawBatteryIcon", display)
        self.assertIn('#include "BatteryMonitor.h"', display)
        self.assertIn("batteryMonitor.begin()", display)
        self.assertIn("batteryMonitor.read()", display)
        self.assertIn("status.percent", display)
        self.assertIn("status.charging", display)
        self.assertIn("status.full", display)
        self.assertIn("drawChargingBolt", display)
        self.assertIn("String batteryStatusText()", display)
        self.assertIn('"電量："', display)
        self.assertIn("充電中", display)
        self.assertIn("FIRMWARE_VERSION", display)
        self.assertIn('"版本："', display)
        self.assertRegex(display, r"showConfigMode[\s\S]*drawText\(18, 124, String\(\"版本：\"\) \+ FIRMWARE_VERSION\)")
        self.assertRegex(display, r"showConfigMode[\s\S]*drawText\(18, 100, batteryStatusText\(\)\)")
        self.assertIn("#include <qrcode.h>", display)
        self.assertIn("drawQrCode", display)
        self.assertIn("qrcode_initText", display)
        self.assertIn("qrcode_getModule", display)
        self.assertIn('String("設定 ") + FIRMWARE_PRODUCT_NAME', display)
        self.assertIn("網絡：", display)
        self.assertIn("完成後按「儲存並重啟」", display)
        self.assertNotIn("需要設定", display)

        config = read_text("include/ProductConfig.h")
        profile_test = read_text("test_host/test_board_profile.cpp")
        self.assertIn('#define FIRMWARE_VERSION "1.0.0"', config)
        self.assertIn("battery.adcPin == 4", profile_test)
        self.assertIn("battery.sensePowerPin == 17", profile_test)
        self.assertIn("battery.chargeDetectPin == 2", profile_test)
        self.assertIn("battery.chargeFullPin == 1", profile_test)

        monitor = read_text("src/BatteryMonitor.cpp")
        self.assertIn("analogReadMilliVolts(battery.adcPin)", monitor)
        self.assertIn("battery.voltageMultiplier", monitor)
        self.assertIn("battery.chargeDetectActiveLevel", monitor)
        self.assertIn("battery.chargeFullActiveLevel", monitor)
        self.assertIn("batterySnapshotFromSignals", monitor)

    def test_display_uses_four_equal_widget_lane_regions(self):
        header = read_text("include/EInkDisplay.h")
        display = read_text("src/EInkDisplay.cpp")

        for api in (
            "void showDashboard(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);",
            "void refreshWidgetLane(uint8_t slot, const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);",
            "void refreshClock(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);",
            "void refreshWeatherFooter(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);",
            "void showSleep(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);",
        ):
            self.assertIn(api, header)
        self.assertNotIn("EtaController", header)
        self.assertNotIn("TRANSITINK_LEGACY_COMPAT", header)

        self.assertIn("constexpr DisplayRegion kStatusRegion{0, 0, EINK_WIDTH, 42};", display)
        self.assertIn("constexpr DisplayRegion kLaneRegions[transitink::kWidgetSlotCount]", display)
        for rectangle in (
            "{0, 42, EINK_WIDTH, 57}",
            "{0, 99, EINK_WIDTH, 57}",
            "{0, 156, EINK_WIDTH, 57}",
            "{0, 213, EINK_WIDTH, 57}",
        ):
            self.assertIn(rectangle, display)
        self.assertIn("constexpr DisplayRegion kFooterRegion{0, 270, EINK_WIDTH, 30};", display)
        self.assertIn("kLaneRegions[slot]", display)
        self.assertIn("static_assert(transitink::kWidgetSlotCount == 4", display)
        self.assertIn("constexpr bool regionFitsPanel(const DisplayRegion& region)", display)
        for bound in (
            "region.x >= 0",
            "region.y >= 0",
            "region.w > 0",
            "region.h > 0",
            "region.x + region.w <= EINK_WIDTH",
            "region.y + region.h <= EINK_HEIGHT",
        ):
            self.assertIn(bound, display)
        self.assertEqual(display.count("static_assert(regionFitsPanel("), 6)

        dashboard = cpp_function_body(display, "void EInkDisplay::showDashboard(")
        assert_fragments_in_order(
            self,
            dashboard,
            "canvas.clear();",
            "drawClockAndStatusBar();",
            "shouldDrawLaneDivider(snapshots, slot)",
            "drawWidgetLane(slot, snapshots[slot], false, drawDivider);",
            "drawWeatherFooter(weather);",
            "fullRefresh();",
            "dashboardFrameActive = true;",
        )

        lane = cpp_function_body(display, "void EInkDisplay::refreshWidgetLane(")
        assert_fragments_in_order(
            self,
            lane,
            "if (slot >= transitink::kWidgetSlotCount)",
            "return;",
            "if (!dashboardFrameActive || !previousFrameValid)",
            "showDashboard(snapshots, weather);",
            "return;",
            "std::memcpy(frameBuffer, previousFrameBuffer, sizeof(frameBuffer));",
            "clearRegion(region);",
            "shouldDrawLaneDivider(snapshots, slot)",
            "drawWidgetLane(slot, snapshots[slot], false, drawDivider);",
            "partialRefresh(region.x, region.y, region.w, region.h);",
        )

        clock = cpp_function_body(
            display,
            "void EInkDisplay::refreshClock(const transitink::WidgetSnapshotSet&",
        )
        assert_fragments_in_order(
            self,
            clock,
            "if (!dashboardFrameActive || !previousFrameValid)",
            "showDashboard(snapshots, weather);",
            "return;",
            "std::memcpy(frameBuffer, previousFrameBuffer, sizeof(frameBuffer));",
            "clearRegion(kStatusRegion);",
            "drawClockAndStatusBar();",
            "partialRefresh(kStatusRegion.x, kStatusRegion.y, kStatusRegion.w, kStatusRegion.h);",
        )

        footer = cpp_function_body(display, "void EInkDisplay::refreshWeatherFooter(")
        assert_fragments_in_order(
            self,
            footer,
            "if (!dashboardFrameActive || !previousFrameValid)",
            "showDashboard(snapshots, weather);",
            "return;",
            "std::memcpy(frameBuffer, previousFrameBuffer, sizeof(frameBuffer));",
            "clearRegion(kFooterRegion);",
            "drawWeatherFooter(weather);",
            "partialRefresh(kFooterRegion.x, kFooterRegion.y, kFooterRegion.w, kFooterRegion.h);",
        )

        sleep = cpp_function_body(
            display,
            "void EInkDisplay::showSleep(const transitink::WidgetSnapshotSet&",
        )
        assert_fragments_in_order(
            self,
            sleep,
            "canvas.clear();",
            "drawClockAndStatusBar();",
            "shouldDrawLaneDivider(snapshots, slot)",
            "drawWidgetLane(slot, snapshots[slot], true, drawDivider);",
            "drawWeatherFooter(weather);",
            "markNonDashboardFrame();",
            "fullRefresh();",
        )

        self.assertIn('"%02d/%02d %s %02d:%02d"', display)
        self.assertIn('"星期日", "星期一", "星期二", "星期三",', display)
        self.assertIn('"星期四", "星期五", "星期六"', display)
        self.assertIn("String currentClockText()", display)
        self.assertIn("drawText(12, 24, currentClockText())", display)
        self.assertIn("panel.showPartialRegion(frameBuffer, previousFrameBuffer, x, y, w, h)", display)
        self.assertIn("copyPartialRegionToPrevious(x, y, w, h)", display)
        self.assertIn("previousFrameValid", display)
        self.assertIn("kMaxPartialRefreshes", display)
        self.assertRegex(display, r"constexpr int kMaxPartialRefreshes\s*=\s*8;")
        self.assertIn("partialRefreshCount", display)
        self.assertIn("std::memcpy(previousFrameBuffer, frameBuffer, sizeof(frameBuffer))", display)
        self.assertNotIn('"%02d:%02d 更新"', display)
        self.assertNotIn('return "未更新";', display)
    def test_dashboard_lanes_render_snapshot_state_and_type_aware_value_caps(self):
        display = read_text("src/EInkDisplay.cpp")
        lane_signature = "void drawWidgetLane(uint8_t slot,"
        self.assertIn(lane_signature, display)
        lane = cpp_function_body(display, lane_signature)
        self.assertIn("snapshot.type == transitink::WidgetType::Disabled", lane)
        disabled = lane.index("snapshot.type == transitink::WidgetType::Disabled")
        self.assertLess(disabled, lane.index("return;", disabled))
        self.assertNotIn("drawLaneDivider", lane[disabled:lane.index("return;", disabled)])
        self.assertIn("drawLaneDivider(region, drawDivider)", lane)
        divider_helper = cpp_function_body(display, "bool shouldDrawLaneDivider(")
        self.assertIn("snapshots[slot].type == transitink::WidgetType::Disabled", divider_helper)
        self.assertIn("snapshots[index].type != transitink::WidgetType::Disabled", divider_helper)
        self.assertIn("snapshot.state == transitink::WidgetState::Empty", lane)
        self.assertIn("snapshot.state == transitink::WidgetState::Error", lane)
        self.assertIn("snapshot.providerMessage", lane)
        self.assertIn("snapshot.freshness == transitink::Freshness::Stale", lane)
        self.assertIn('"資料已逾期"', lane)
        self.assertIn('"暫未能取得資料"', lane)
        self.assertIn("snapshot.type == transitink::WidgetType::JourneyTime ? 1U : 2U", lane)
        self.assertIn("std::min(snapshot.valueCount, valueLimit)", lane)
        shown_values = lane.index("const std::size_t shownValueCount")
        stale_empty = lane.index("snapshot.freshness == transitink::Freshness::Stale")
        self.assertLess(lane.index('"暫未能取得資料"', stale_empty), shown_values)
        fresh_empty_error = lane.index("snapshot.freshness == transitink::Freshness::Fresh")
        self.assertLess(lane.index("snapshot.providerMessage", fresh_empty_error), shown_values)
        self.assertLess(lane.index("return;", fresh_empty_error), shown_values)
        self.assertIn("snapshot.values[valueIndex].text", lane)
        self.assertIn("snapshot.type == transitink::WidgetType::JourneyTime", lane)
        self.assertIn("snapshot.values[valueIndex].context", lane)
        self.assertGreater(lane.index('"資料已逾期"'), shown_values)
        self.assertIn('drawText(valueX, valueY, "-")', lane)

    def test_display_truncates_utf8_by_measured_codepoints_and_uses_transitink_branding(self):
        display = read_text("src/EInkDisplay.cpp")
        self.assertTrue((ROOT / "include/core/DisplayTextCore.h").exists())
        self.assertTrue((ROOT / "src/core/DisplayTextCore.cpp").exists())
        core_header = read_text("include/core/DisplayTextCore.h")
        core = read_text("src/core/DisplayTextCore.cpp")
        self.assertIn("int measureTextWidth(const String& text)", display)
        self.assertIn("void drawTruncatedText(int x, int y, const String& text, int maxWidth)", display)
        self.assertRegex(display, r"measureTextWidth[\s\S]*decodeUtf8Codepoint[\s\S]*findHkGlyph")
        truncated_body = cpp_function_body(display, "void drawTruncatedText(")
        self.assertIn("transitink::planTruncatedUtf8", truncated_body)
        self.assertIn("drawText(x, y, String(plan.text.c_str()))", truncated_body)
        self.assertNotIn("substring(", truncated_body)
        self.assertIn("using CodepointWidth", core_header)
        self.assertIn("struct DisplayTextPlan", core_header)
        self.assertIn('"…"', core)
        for suffix in ('"..."', '".."', '"."', '""'):
            self.assertIn(suffix, core)
        self.assertIn("output.reserve", core)
        self.assertNotIn("std::function", core_header + core)
        self.assertIn("drawText(18, 42, FIRMWARE_PRODUCT_NAME)", display)
        self.assertIn('drawText(18, 38, String("設定 ") + FIRMWARE_PRODUCT_NAME)', display)
        self.assertNotIn('"設定 TransitInk"', display)
        self.assertNotIn("TRANSITINK_LEGACY_COMPAT", display)

    def test_partial_refresh_skips_noop_and_promotes_large_diff_to_full(self):
        display = read_text("src/EInkDisplay.cpp")
        self.assertIn("constexpr float kForceFullPartialDiffRatio = 0.30f;", display)
        self.assertIn("struct PartialDiffStats", display)
        self.assertIn("PartialDiffStats partialDiffStats(int x, int y, int w, int h)", display)
        self.assertIn("stats.changedBits == 0", display)
        self.assertIn('Serial.println("EPD partial skipped: unchanged")', display)
        self.assertIn("stats.ratio() >= kForceFullPartialDiffRatio", display)
        self.assertIn('Serial.println("EPD partial promoted to full: large diff")', display)
        self.assertRegex(display, r"refreshCanvasPartially[\s\S]*stats\.changedBits == 0[\s\S]*return;[\s\S]*stats\.ratio\(\) >= kForceFullPartialDiffRatio[\s\S]*flushCanvas\(\);[\s\S]*return;[\s\S]*panel\.showPartialRegion")

    def test_widget_refresh_helpers_force_all_and_service_only_one_due_slot(self):
        main = read_text("src/main.cpp")
        all_body = cpp_function_body(main, "void refreshAllWidgetsNow()")
        assert_fragments_in_order(
            self,
            all_body,
            "widgetScheduler.forceAllDue(nowMs);",
            "attempts < transitink::kWidgetSlotCount",
            "widgetScheduler.hasPendingDue(nowMs)",
            "widgetScheduler.serviceNextDue(nowMs, nowEpoch);",
            "einkDisplay.showDashboard(currentDisplaySnapshots(), weatherSnapshot);",
        )
        self.assertEqual(1, all_body.count("serviceNextDue("))
        one_body = cpp_function_body(main, "void serviceOneWidgetIfDue()")
        self.assertEqual(1, one_body.count("serviceNextDue("))
        assert_fragments_in_order(
            self,
            one_body,
            "const transitink::WidgetTickResult tick = widgetScheduler.serviceNextDue",
            "if (!tick.ran)",
            "einkDisplay.refreshWidgetLane(tick.slot, currentDisplaySnapshots(), weatherSnapshot);",
        )

    def test_clock_refreshes_on_minute_boundary_independent_of_eta(self):
        main = read_text("src/main.cpp")
        header = read_text("include/EInkDisplay.h")
        display = read_text("src/EInkDisplay.cpp")
        self.assertIn("unsigned long nextClockRefreshMs = 0", main)
        self.assertIn("uint32_t secondsUntilNextMinute(time_t now)", main)
        self.assertIn("60 - (now % 60)", main)
        self.assertIn("void scheduleNextClockRefresh()", main)
        self.assertIn("void refreshClockNow()", main)
        self.assertIn('Serial.println("Clock refresh start")', main)
        self.assertIn("einkDisplay.refreshClock(currentDisplaySnapshots(), weatherSnapshot);", main)
        self.assertIn("scheduleNextClockRefresh();", main)
        self.assertNotIn("EtaController", header)
        self.assertIn("constexpr DisplayRegion kStatusRegion{0, 0, EINK_WIDTH, 42};", display)
        self.assertIn("clearRegion(kStatusRegion);", display)
        self.assertIn("drawClockAndStatusBar()", display)
        self.assertIn("partialRefresh(kStatusRegion.x, kStatusRegion.y, kStatusRegion.w, kStatusRegion.h)", display)
        self.assertRegex(display, r"drawClockAndStatusBar\(\)[\s\S]*drawStatusBar\(\);[\s\S]*drawText\(12, 24, currentClockText\(\)\);")
        self.assertNotIn("showEta", main)
        self.assertRegex(main, r"loop\(\)[\s\S]*serviceOneWidgetIfDue\(\);")
        self.assertRegex(main, r"loop\(\)[\s\S]*millis\(\) >= nextClockRefreshMs[\s\S]*refreshClockNow\(\);")

    def test_eink_text_renderer_uses_only_pinned_custom_glyph_data(self):
        display = read_text("src/EInkDisplay.cpp")
        self.assertIn("decodeUtf8Codepoint", display)
        self.assertIn('#include "HkGlyphFont.h"', display)
        self.assertIn("drawCustomGlyph(cursorX, y, codepoint)", display)
        self.assertIn("if (width > 0) {", display)
        self.assertNotIn("U8g2_for_Adafruit_GFX", display)
        self.assertNotIn("u8g2_font_", display)
        self.assertNotIn("selectFontForGlyph", display)
        self.assertNotIn("encodeUtf8Codepoint", display)
        self.assertNotIn("codepoint = '?';", display)

        glyph_lookup = read_text("src/HkGlyphFont.cpp")
        glyphs = read_text("src/generated/HkGlyphFontData.cpp")
        self.assertIn("0x9418", glyphs)
        self.assertIn("SPDX-License-Identifier: OFL-1.1", glyphs)
        self.assertIn("findHkGlyph", glyph_lookup)
        self.assertNotIn("kHkGlyphs[]", glyph_lookup)

    def test_transitink_portal_has_three_tabs_and_four_accessible_widget_cards(self):
        page = read_text("src/TransitInkPortalPage.cpp")
        for label in ("Wi-Fi", "主頁小工具", "更新及省電"):
            self.assertIn(label, page)
        self.assertIn("label_tc:'港鐵'", page)
        self.assertNotIn("港鐵重鐵", page)
        self.assertIn('role="tablist"', page)
        self.assertIn('role="tab"', page)
        self.assertIn('aria-selected', page)
        self.assertIn('aria-expanded', page)
        self.assertIn('aria-live="polite"', page)
        self.assertIn(":focus-visible", page)
        self.assertIn("Array.from({length:4}", page)
        self.assertIn("let expandedSlot=0", page)
        for function in (
            "selectTab", "expandWidgetCard", "renderWidgetCards",
            "setWidgetType", "moveWidget", "ensureCatalogForSlot",
            "validateWidgetDrafts", "collectConfig",
        ):
            self.assertIn(f"function {function}", page)

    def test_widget_editor_stages_catalogs_clears_dependents_and_guards_stale_requests(self):
        page = read_text("src/TransitInkPortalPage.cpp")
        for endpoint in (
            "/assets/catalog/current/index.json",
            "/assets/catalog/current/stops-${provider}.json",
            "/assets/catalog/current/rail.json",
            "/api/catalog/route-index", "/api/catalog/update",
            "/api/catalog/route-override", "/api/catalog/route-refresh",
            "/api/catalog/journey/locations", "/api/catalog/journey/destinations",
        ):
            self.assertIn(endpoint, page)
        for online_catalog in (
            "/api/catalog/bus/routes", "/api/catalog/bus/directions",
            "/api/catalog/bus/stops", "/api/catalog/gmb/routes",
            "/api/catalog/gmb/directions", "/api/catalog/gmb/stops",
            "/api/catalog/rail/lines", "/api/catalog/rail/stations",
            "/api/catalog/rail/directions",
        ):
            self.assertNotIn(online_catalog, page)
        self.assertIn("requestVersion[slot]", page)
        self.assertIn("if(token!==requestVersion[slot])return", page)
        self.assertIn("clearBusAfterOperator", page)
        self.assertIn("clearBusAfterRoute", page)
        self.assertIn("clearRailAfterMode", page)
        self.assertIn("clearJourneyAfterLocation", page)
        self.assertIn("上移", page)
        self.assertIn("下移", page)
        self.assertIn("[catalogState[slot],catalogState[target]]=[catalogState[target],catalogState[slot]]", page)
        self.assertIn("[widgetErrors[slot],widgetErrors[target]]=[widgetErrors[target],widgetErrors[slot]]", page)
        self.assertIn("disabled", page)
        self.assertIn("bus_eta", page)
        self.assertIn("mtr_eta", page)
        self.assertIn("journey_time", page)

    def test_portal_is_password_safe_and_has_one_save_action(self):
        page = read_text("src/TransitInkPortalPage.cpp")
        codec = read_text("src/PortalConfigCodec.cpp")
        self.assertEqual(page.count("儲存並重新啟動"), 1)
        self.assertIn("留空會保留目前已儲存的密碼", page)
        self.assertIn("wifi_password_set", codec)
        self.assertNotIn('doc["wifi_password"] = config.wifiPassword', codec)
        self.assertNotIn("value=cfg.wifi_password", page)
        self.assertNotIn("refresh_seconds", page)
        self.assertNotIn("/api/routes", page)
        self.assertNotIn("/api/route-stops", page)

    def test_config_portal_shows_device_battery_percent(self):
        header = read_text("include/ConfigPortal.h")
        self.assertIn('#include "BatteryMonitor.h"', header)
        self.assertIn("BatteryMonitor batteryMonitor_", header)

        portal = read_text("src/ConfigPortal.cpp")
        page = read_text("src/TransitInkPortalPage.cpp")
        codec = read_text("src/PortalConfigCodec.cpp")
        self.assertIn("batteryMonitor_.begin()", portal)
        self.assertIn("batteryMonitor_.read()", portal)
        self.assertIn('JsonObject batteryJson = doc.createNestedObject("battery")', codec)
        self.assertIn('batteryJson["percent"] = battery.percent', codec)
        self.assertIn('id="battery_percent"', page)
        self.assertIn('id="firmware_version"', page)
        self.assertIn("function renderDeviceFacts", page)
        self.assertIn("電量", page)
        self.assertIn("版本", page)

    def test_weather_location_setting_and_eta_display(self):
        config = read_text("include/AppConfig.h")
        self.assertIn("String weatherLocationTc", config)

        config_impl = read_text("src/AppConfig.cpp")
        self.assertIn('parsed.weatherLocationTc = asString(doc["weather_location_tc"])', config_impl)
        self.assertIn('doc["weather_location_tc"] = config.weatherLocationTc', config_impl)

        page = read_text("src/TransitInkPortalPage.cpp")
        self.assertIn('id="weather_location"', page)
        self.assertIn("renderWeatherLocationOptions", page)
        self.assertIn("renderWeatherLocationOptions(cfg.weather_location_tc", page)
        self.assertIn("weather_location_tc:byId('weather_location').value", page)
        self.assertIn("天氣位置", page)

        weather_header = read_text("include/WeatherClient.h")
        self.assertIn("struct WeatherSnapshot", weather_header)
        self.assertIn("bool fetchCurrentWeather", weather_header)
        self.assertIn("String weatherDisplayText", weather_header)

        weather_client = read_text("src/WeatherClient.cpp")
        self.assertIn("data.weather.gov.hk/weatherAPI/opendata/weather.php?dataType=rhrread&lang=tc", weather_client)
        self.assertIn('item["place"]', weather_client)
        self.assertIn('item["value"]', weather_client)
        self.assertIn("weatherConditionText", weather_client)

        display_header = read_text("include/EInkDisplay.h")
        self.assertIn('#include "WeatherClient.h"', display_header)
        self.assertIn("const WeatherSnapshot& weather", display_header)

        display = read_text("src/EInkDisplay.cpp")
        self.assertIn("drawWeatherFooter", display)
        self.assertIn("weatherDisplayText(weather)", display)
        self.assertIn("drawTruncatedText(12, 291, weatherDisplayText(weather), 376)", display)
        footer = cpp_function_body(display, "void drawWeatherFooter(")
        self.assertNotIn("同步", footer)
        self.assertNotIn("fetchedAtEpoch", footer)

        main = read_text("src/main.cpp")
        self.assertIn("WeatherClient weatherClient", main)
        self.assertIn("WeatherSnapshot weatherSnapshot", main)
        self.assertIn("unsigned long nextWeatherRefreshMs = 0", main)
        self.assertIn("void refreshWeatherNow()", main)
        self.assertIn("weatherClient.fetchCurrentWeather(deviceConfig.weatherLocationTc, weatherSnapshot", main)
        self.assertIn("WEATHER_REFRESH_SECONDS", main)
        self.assertIn("refreshWeatherNow();", main)
        weather_body = cpp_function_body(main, "void refreshWeatherNow()")
        self.assertIn("einkDisplay.refreshWeatherFooter(currentDisplaySnapshots(), weatherSnapshot);", weather_body)
        self.assertNotIn("showDashboard", weather_body)

    def test_sleep_power_settings_are_persisted_and_exposed(self):
        config = read_text("include/ProductConfig.h")
        self.assertIn("#define SLEEP_ENABLED_DEFAULT 1", config)
        self.assertIn("#define SLEEP_WAKE_DEFAULT_MINUTES 5", config)
        self.assertIn("#define SLEEP_MAINTENANCE_DEFAULT_HOURS 12", config)

        app_config = read_text("include/AppConfig.h")
        self.assertIn("bool sleepEnabled = SLEEP_ENABLED_DEFAULT", app_config)
        self.assertIn("uint16_t wakeDurationMinutes = SLEEP_WAKE_DEFAULT_MINUTES", app_config)
        self.assertIn("uint16_t sleepMaintenanceHours = SLEEP_MAINTENANCE_DEFAULT_HOURS", app_config)

        config_impl = read_text("src/AppConfig.cpp")
        self.assertIn('parsed.sleepEnabled = doc["sleep_enabled"] | static_cast<bool>(SLEEP_ENABLED_DEFAULT)', config_impl)
        self.assertIn('parsed.wakeDurationMinutes = doc["wake_duration_minutes"] | SLEEP_WAKE_DEFAULT_MINUTES', config_impl)
        self.assertIn('parsed.sleepMaintenanceHours = doc["sleep_maintenance_hours"] | SLEEP_MAINTENANCE_DEFAULT_HOURS', config_impl)
        self.assertIn("if (parsed.wakeDurationMinutes < 1)", config_impl)
        self.assertIn("if (parsed.wakeDurationMinutes > 60)", config_impl)
        self.assertIn("if (parsed.sleepMaintenanceHours > 24)", config_impl)
        self.assertIn('doc["sleep_enabled"] = config.sleepEnabled', config_impl)
        self.assertIn('doc["wake_duration_minutes"] = config.wakeDurationMinutes', config_impl)
        self.assertIn('doc["sleep_maintenance_hours"] = config.sleepMaintenanceHours', config_impl)

        page = read_text("src/TransitInkPortalPage.cpp")
        self.assertIn('id="sleep_enabled"', page)
        self.assertIn('id="wake_duration_minutes"', page)
        self.assertIn('id="sleep_maintenance_hours"', page)
        self.assertIn("省電睡眠模式", page)
        self.assertIn("醒著時間", page)
        self.assertIn("睡眠中背景更新", page)
        self.assertIn("byId('sleep_enabled').checked=cfg.sleep_enabled!==false", page)
        self.assertIn("byId('wake_duration_minutes').value=cfg.wake_duration_minutes||5", page)
        self.assertIn("byId('sleep_maintenance_hours').value=cfg.sleep_maintenance_hours??12", page)
        self.assertIn("sleep_enabled:byId('sleep_enabled').checked", page)
        self.assertIn("wake_duration_minutes:Number(byId('wake_duration_minutes').value||5)", page)
        self.assertIn("sleep_maintenance_hours:Number(byId('sleep_maintenance_hours').value||0)", page)
        for cadence in (
            "<span>港鐵 ETA</span><strong>每 30 秒</strong>",
            "<span>巴士 ETA</span><strong>每 60 秒</strong>",
            "<span>行車時間</span><strong>每 120 秒</strong>",
        ):
            self.assertIn(cadence, page)

    def test_sleep_state_uses_light_sleep_gpio0_home_wake_and_stops_network_services(self):
        main = read_text("src/main.cpp")
        support = read_text("src/hardware/BoardSupport.cpp")
        self.assertIn("#include <esp_sleep.h>", main)
        self.assertIn("#include <esp_wifi.h>", main)
        self.assertIn('#include "driver/gpio.h"', support)
        self.assertIn('#include "driver/rtc_io.h"', support)
        self.assertIn("unsigned long wakeStartedAtMs = 0", main)
        self.assertIn("bool sleepMaintenanceWake = false", main)
        self.assertIn("void enterSleepMode(const char* reason)", main)
        self.assertIn("void configureLightSleepWakeup()", main)
        self.assertIn("void returnFromLightSleep()", main)
        self.assertIn("void performLightSleepMaintenance()", main)
        self.assertIn("RTC_NOINIT_ATTR uint32_t sleepResumeMarker", main)
        self.assertIn("RTC_NOINIT_ATTR uint32_t sleepResumeMarkerInverse", main)
        self.assertIn("void armSleepResumeMarker()", main)
        self.assertIn("bool consumeSleepResumeMarker()", main)
        self.assertIn("bus_eta::SleepSettings sleepSettingsFromConfig", main)
        self.assertIn("const bool sleepBlocked = configAccessMode || chargeSnapshot.powerPresent", main)
        self.assertRegex(
            main,
            r"bus_eta::shouldAutoSleep\(\s*sleepSettingsFromConfig\(\), wakeStartedAtMs, millis\(\), sleepBlocked\)",
        )
        self.assertIn("configPortal.stop();", main)
        self.assertIn("WiFi.disconnect(true, true)", main)
        self.assertIn("esp_wifi_stop()", main)
        self.assertIn("WiFi.mode(WIFI_OFF)", main)
        self.assertIn("einkDisplay.prepareForSleep()", main)
        self.assertIn("transitink::hardware::configureHomeWakeup()", main)
        self.assertIn("transitink::hardware::configureChargeWakeup()", main)
        self.assertIn("esp_sleep_enable_gpio_wakeup()", support)
        self.assertIn("esp_light_sleep_start()", main)
        self.assertNotIn("esp_deep_sleep_start()", main)
        self.assertNotIn("esp_sleep_enable_ext0_wakeup", main)
        self.assertIn("esp_sleep_enable_timer_wakeup", main)
        self.assertIn("sleepMaintenanceIntervalUs", main)
        self.assertIn("esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER", main)
        self.assertIn("esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO", main)
        self.assertIn("waitForHomeRelease();", main)
        self.assertIn("returnFromLightSleep();", main)
        self.assertIn("performLightSleepMaintenance();", main)
        self.assertIn("refreshWeatherNow();", main)
        self.assertIn("refreshAllWidgetsNow();", main)
        self.assertIn("enterSleepMode(\"maintenance complete\")", main)
        sleep_loop = cpp_function_body(main, "void enterSleepMode(const char* reason)")
        assert_fragments_in_order(
            self,
            sleep_loop,
            "armSleepResumeMarker();",
            "esp_light_sleep_start();",
            "clearSleepResumeMarker();",
        )
        setup_body = cpp_function_body(main, "void setup()")
        assert_fragments_in_order(
            self,
            setup_body,
            "const bool rtcResetWake = consumeSleepResumeMarker();",
            "const bool persistentResetWake = configStoreReady && configStore.sleepResumePending();",
            "const bool resetWake = rtcResetWake || persistentResetWake;",
            "const bool homeWake = wakeCause == ESP_SLEEP_WAKEUP_GPIO || resetWake;",
            "einkDisplay.begin(false);",
        )
        self.assertRegex(main, r"if \(deviceConfig\.sleepEnabled && sleepMaintenanceWake\) \{[\s\S]*performLightSleepMaintenance\(\);[\s\S]*enterSleepMode\(\"maintenance complete\"\)")
        maintenance = cpp_function_body(main, "void performLightSleepMaintenance()")
        assert_fragments_in_order(
            self,
            maintenance,
            "syncTimeAndWeatherBeforeDashboard(false);",
            "widgetScheduler.forceAllDue(nowMs);",
            "widgetScheduler.serviceNextDue(nowMs, nowEpoch);",
            "widgetScheduler.displaySnapshots(nowEpoch)",
            "einkDisplay.showSleep",
            "stopNetworkForSleep();",
        )
        self.assertEqual(1, maintenance.count("serviceNextDue("))
        self.assertGreater(
            maintenance.index("const uint32_t nowMs = millis();"),
            maintenance.index('weatherSnapshot.error = "Wi-Fi 未連接";'),
        )
        home_wake = cpp_function_body(main, "void returnFromLightSleep()")
        assert_fragments_in_order(
            self,
            home_wake,
            "connectWifi(deviceConfig)",
            "syncTimeAndWeatherBeforeDashboard(true);",
            "refreshAllWidgetsNow();",
            "clearPersistentSleepResumeMarker();",
        )

        battery = read_text("include/BatteryMonitor.h")
        self.assertIn("void shutdown();", battery)
        display = read_text("src/EInkDisplay.cpp")
        self.assertNotRegex(display, r"void EInkDisplay::prepareForSleep\(\) \{[\s\S]*batteryMonitor\.shutdown\(\);[\s\S]*\}")

    def test_sleep_screen_removes_stale_eta_minutes_without_bottom_right_icons(self):
        header = read_text("include/EInkDisplay.h")
        self.assertIn("void begin(bool showBootScreen = true);", header)
        self.assertIn("void showSleep(const transitink::WidgetSnapshotSet& snapshots, const WeatherSnapshot& weather);", header)
        self.assertNotIn("void showSleep(const DeviceConfig& config, const WeatherSnapshot& weather);", header)
        self.assertIn("void prepareForSleep();", header)

        display = read_text("src/EInkDisplay.cpp")
        self.assertNotIn("drawAwakeCatIcon", display)
        self.assertNotIn("drawSleepCatIcon", display)
        self.assertNotIn("drawCatIcon", display)
        self.assertNotIn('drawText(x + 2, y + 8, "Z")', display)
        self.assertNotIn("canvas.fillCircle(x + 32, y + 34, 18, kColorBlack)", display)
        self.assertIn("void drawWidgetLane(uint8_t slot, const transitink::WidgetSnapshot& snapshot, bool sleeping, bool drawDivider)", display)
        self.assertIn('drawText(valueX, valueY, "-")', display)
        self.assertNotIn("休眠中", display)
        self.assertIn("EInkDisplay::showSleep", display)
        snapshot_sleep_body = cpp_function_body(
            display,
            "void EInkDisplay::showSleep(const transitink::WidgetSnapshotSet&",
        )
        assert_fragments_in_order(
            self,
            snapshot_sleep_body,
            "drawWidgetLane(slot, snapshots[slot], true, drawDivider);",
            "drawWeatherFooter(weather);",
        )
        self.assertNotIn("drawEtaRows", snapshot_sleep_body)
        self.assertNotIn("TRANSITINK_LEGACY_COMPAT", display)
        self.assertNotIn("drawEtaRows", display)
        self.assertNotIn("drawSleepRows", display)
        self.assertNotIn("EInkDisplay::showEta", display)
        self.assertNotRegex(display, r"showEta[\s\S]*drawAwakeCatIcon")
        self.assertNotRegex(display, r"showSleep[\s\S]*drawSleepCatIcon")
        self.assertRegex(display, r"EInkDisplay::begin\(bool showBootScreen\)[\s\S]*if \(showBootScreen\) \{[\s\S]*showBoot\(\"啟動中\"\);")

        main = read_text("src/main.cpp")
        self.assertIn("const bool rtcResetWake = consumeSleepResumeMarker()", main)
        self.assertIn("const bool persistentResetWake = configStoreReady && configStore.sleepResumePending()", main)
        self.assertIn("const bool resetWake = rtcResetWake || persistentResetWake", main)
        self.assertIn("const bool homeWake = wakeCause == ESP_SLEEP_WAKEUP_GPIO || resetWake", main)
        setup_body = cpp_function_body(main, "void setup()")
        self.assertIn("einkDisplay.begin(false);", setup_body)
        self.assertNotIn("einkDisplay.showBoot", setup_body)
        self.assertNotIn("einkDisplay.showWifiStatus", setup_body)
        assert_fragments_in_order(
            self,
            setup_body,
            "const bool persistentResetWake = configStoreReady && configStore.sleepResumePending();",
            "einkDisplay.begin(false);",
            "syncTimeAndWeatherBeforeDashboard(homeWake);",
            "refreshAllWidgetsNow();",
            "if (resetWake) {",
            "clearPersistentSleepResumeMarker();",
        )
        sync_body = cpp_function_body(
            main,
            "void syncTimeAndWeatherBeforeDashboard(bool homeWake)",
        )
        self.assertNotIn("showWifiStatus", sync_body)
        self.assertIn(
            "if (homeWake) {\n"
            "        if (!hasValidTime()) {\n"
            "            waitForTimeSync(2000);\n"
            "        }\n"
            "        refreshWeatherNow();\n"
            "        return;\n"
            "    }",
            sync_body,
        )
        self.assertEqual(1, sync_body.count("return;"))
        self.assertIn("waitForTimeSync(hasValidTime() ? 1000 : 15000);", sync_body)

    def test_catalog_cache_ownership_is_outside_thin_config_portal(self):
        portal = read_text("src/ConfigPortal.cpp")
        store = read_text("src/WidgetCatalogService.cpp")
        self.assertNotIn("kRoutesCachePath", portal)
        self.assertNotIn("kStopsCachePath", portal)
        self.assertNotIn("proxyRoutes", portal)
        self.assertNotIn("proxyRouteStops", portal)
        self.assertIn("WidgetCatalogService::readJsonCache", store)
        self.assertIn("WidgetCatalogService::writeJsonCache", store)
        self.assertIn("WidgetCatalogService::refreshBusRoute", store)
        self.assertIn("WidgetCatalogService::refreshGmbRoute", store)
        self.assertIn("LittleFS.rename", store)
        self.assertNotIn("86400000UL", store)

        portal_header = read_text("include/ConfigPortal.h")
        self.assertIn("scanWifiNetworks", portal_header)

    def test_zectrix_display_uses_low_active_busy(self):
        display = read_text("src/EInkDisplay.cpp")
        driver = read_text("src/hardware/displays/Ssd1683DisplayDriver.cpp")
        profile_test = read_text("test_host/test_board_profile.cpp")
        self.assertIn("SelectedDisplayDriver panel", display)
        self.assertNotIn("sendCommand", display)
        self.assertIn("display.busyActiveLevel", driver)
        self.assertIn("sendCommand(0xE9)", driver)
        self.assertIn("sendCommand(0x10)", driver)
        self.assertIn("pack1bpp", driver)
        self.assertIn("Ssd1683DisplayDriver::showPartialRegion", driver)
        self.assertIn("displayPartialFrameRegion(current, previous, x, y, width, height)", driver)
        self.assertIn("packPartialTransition(previous[byte], current[byte]", driver)
        self.assertIn("packPartialTransition(current[byte], current[byte]", driver)
        self.assertIn("const bool dirtyRow", driver)
        self.assertIn("const bool dirtyByte", driver)
        self.assertIn("display.busyActiveLevel == 0", profile_test)

    def test_status_display_handles_multiline_messages(self):
        display = read_text("src/EInkDisplay.cpp")
        self.assertIn("drawMultilineText", display)
        self.assertIn("text.indexOf('\\n'", display)
        self.assertRegex(display, r"showWifiStatus[\s\S]*drawMultilineText")
        self.assertRegex(display, r"showConfigMode[\s\S]*drawMultilineText")

    def test_rail_clients_providers_and_catalog_keep_focused_contracts(self):
        mtr_header = read_text("include/MtrClient.h")
        mtr_client = read_text("src/MtrClient.cpp")
        light_header = read_text("include/LightRailClient.h")
        light_client = read_text("src/LightRailClient.cpp")
        mtr_provider = read_text("src/providers/MtrProvider.cpp")
        light_provider = read_text("src/providers/LightRailProvider.cpp")
        parser = read_text("src/TransitJsonParsers.cpp")
        catalog = read_text("src/TransitCatalog.cpp")
        native = read_text("platformio.native.ini")

        self.assertIn("https://rt.data.gov.hk/v1/transport/mtr/getSchedule.php?line=", mtr_header)
        self.assertIn("&sta=", mtr_header)
        self.assertIn("&lang=TC", mtr_header)
        self.assertIn("https://rt.data.gov.hk/v1/transport/mtr/lrt/getSchedule?station_id=", light_header)
        self.assertIn("&with_special=1", light_header)
        self.assertIn("http.setTimeout(10000)", mtr_client)
        self.assertIn("http.setTimeout(10000)", light_client)
        self.assertIn("findTransitCatalogDirection", mtr_client)
        self.assertIn("findTransitCatalogDirection", light_client)
        self.assertLess(mtr_client.index("findTransitCatalogDirection"),
                        mtr_client.index("WiFiClientSecure tls"))
        self.assertLess(light_client.index("findTransitCatalogDirection"),
                        light_client.index("WiFiClientSecure tls"))
        self.assertIn("RailMode::HeavyRail", mtr_provider)
        self.assertIn("RailMode::LightRail", light_provider)
        self.assertIn("normalizeRailSnapshot", mtr_provider)
        self.assertIn("normalizeRailSnapshot", light_provider)
        self.assertIn("lightRailDirectionIdForDestination", parser)
        self.assertNotIn("config.directionLabelTc", parser)
        self.assertNotIn("HTTPClient", catalog)
        self.assertIn("[env:native_app_config]", native)
        self.assertIn("[env:native_json]", native)
        self.assertIn("+<TransitCatalog.cpp>", native)

    def test_glyph_generator_uses_deterministic_repo_seed(self):
        generator = read_text("scripts/generate_hk_glyph_font.py")
        seed_path = ROOT / "scripts/hk_glyph_seed.txt"
        self.assertTrue(seed_path.exists(), "missing deterministic glyph seed")
        seed = seed_path.read_text(encoding="utf-8")
        self.assertIn("hk_glyph_seed.txt", generator)
        self.assertNotIn('default=Path("/tmp/kmb-stop.json")', generator)
        self.assertNotIn('default=Path("/tmp/kmb-route.json")', generator)
        self.assertIn('default=None', generator)
        self.assertIn("NotoSansCJKhk-Regular.otf", generator)
        self.assertIn("DEFAULT_FONT_SHA256", generator)
        self.assertIn('"--check"', generator)
        self.assertNotIn("/System/Library/Fonts", generator)
        self.assertNotIn("coretext", generator)
        self.assertNotIn("sips", generator)

        header, payload = seed.split("\n---\n", 1)
        self.assertTrue(header.isascii())
        characters = list(payload.rstrip("\n"))
        codepoints = [ord(character) for character in characters]
        self.assertEqual(codepoints, sorted(set(codepoints)))
        self.assertGreater(len(codepoints), 0)

    def test_glyph_font_has_pinned_ofl_provenance(self):
        script = ROOT / "scripts" / "generate_hk_glyph_font.py"
        spec = importlib.util.spec_from_file_location("generate_hk_glyph_font", script)
        generator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(generator)

        font_path = ROOT / "third_party/fonts/noto-sans-cjk-hk/NotoSansCJKhk-Regular.otf"
        licence_path = ROOT / "third_party/fonts/noto-sans-cjk-hk/OFL.txt"
        upstream_notice_path = ROOT / "third_party/fonts/noto-sans-cjk-hk/UPSTREAM-NOTICE.md"
        source_path = ROOT / "third_party/fonts/noto-sans-cjk-hk/SOURCE.md"
        notices_path = ROOT / "THIRD_PARTY_NOTICES.md"

        self.assertTrue(font_path.is_file())
        self.assertTrue(licence_path.is_file())
        self.assertTrue(upstream_notice_path.is_file())
        self.assertTrue(source_path.is_file())
        self.assertTrue(notices_path.is_file())
        self.assertEqual(
            hashlib.sha256(font_path.read_bytes()).hexdigest(),
            generator.DEFAULT_FONT_SHA256,
        )
        self.assertIn("SIL OPEN FONT LICENSE Version 1.1", licence_path.read_text())
        self.assertRegex(
            upstream_notice_path.read_text(),
            r"copyright\s+is held by Adobe",
        )
        self.assertIn(generator.DEFAULT_FONT_SHA256, source_path.read_text())
        self.assertIn("src/generated/HkGlyphFontData.cpp", notices_path.read_text())

    def test_glyph_generator_rejects_non_bmp_codepoints(self):
        script = ROOT / "scripts" / "generate_hk_glyph_font.py"
        spec = importlib.util.spec_from_file_location("generate_hk_glyph_font", script)
        generator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(generator)
        with tempfile.TemporaryDirectory() as directory:
            seed = Path(directory) / "seed.txt"
            seed.write_text("source=test\n---\n港🚊\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "U\\+1F68A"):
                generator.collect_chars([], seed)

    def test_route_stop_clients_use_bounded_decoded_streams(self):
        for path, signature in (
            ("src/CitybusClient.cpp", "bool CitybusClient::fetchRouteStopsJson"),
            ("src/KmbClient.cpp", "bool KmbClient::fetchRouteStopsJson"),
        ):
            source = read_text(path)
            method = source.split(signature, 1)[1].split("\n}", 1)[0]
            self.assertIn("httpGetBounded", method)
            self.assertIn("kMaxRouteStopResponseBytes", method)
            self.assertNotIn("httpGet(", method)
            self.assertIn("http.writeToStream(&sink)", source)
            self.assertIn("BoundedBodyAccumulator", source)

    def test_catalog_update_uses_official_clients_and_persistent_atomic_caches(self):
        store = read_text("src/WidgetCatalogService.cpp")
        self.assertIn("refreshRouteIndex", store)
        self.assertIn("kUpdatedRouteIndexPath", store)
        self.assertIn("listBusRoutes(transitink::BusOperator::Kmb, true", store)
        self.assertIn("listBusRoutes(transitink::BusOperator::Citybus, true", store)
        self.assertIn("listGmbRoutes(true", store)
        self.assertIn("listBusRoutes(op, true", store)
        self.assertIn("listBusStops(op, normalized", store)
        self.assertIn("listGmbDirections(normalized, true", store)
        self.assertIn("listGmbStops(routeId, routeSeq, true", store)
        self.assertIn("busOverrideCachePath", store)
        self.assertIn("gmbOverrideCachePath", store)
        self.assertIn("writeJsonCache", store)
        self.assertIn('path + ".tmp"', store)
        self.assertIn('path + ".bak"', store)
        self.assertIn("LittleFS.rename", store)
        self.assertNotIn("removeRouteDetailCaches", store)
        self.assertNotIn("verifyManifestSignature", store)

    def test_static_catalog_service_uses_tested_projection_core(self):
        service = read_text("src/WidgetCatalogService.cpp")
        for call in (
            "listStaticJourneyLocations", "listStaticJourneyDestinations",
        ):
            self.assertEqual(service.count(f"transitink::{call}("), 1)
        for direct_call in (
            "heavyRailCatalog", "lightRailCatalog", "journeyTimeCatalog",
            "findTransitCatalogGroup", "findTransitCatalogStation",
            "findJourneyTimeLocation", "findJourneyTimeDestination",
        ):
            self.assertNotIn(direct_call, service)
        self.assertIn("+<core/StaticCatalogCore.cpp>", read_text("platformio.native.ini"))


if __name__ == "__main__":
    unittest.main()
