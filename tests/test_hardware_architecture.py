import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read_text(path):
    return (ROOT / path).read_text(encoding="utf-8")


class HardwareArchitectureTests(unittest.TestCase):
    def test_board_selection_is_explicit_and_fail_closed(self):
        selector = read_text("include/hardware/BoardProfile.h")
        ini = read_text("platformio.ini")
        self.assertIn("TRANSITINK_BOARD_ZECTRIX_NOTE4", selector)
        self.assertIn("hardware/boards/ZectrixNote4.h", selector)
        self.assertIn('#error "No TransitInk board profile selected.', selector)
        self.assertIn("-DTRANSITINK_BOARD_ZECTRIX_NOTE4=1", ini)

    def test_product_config_is_separate_from_hardware_profile(self):
        product = read_text("include/ProductConfig.h")
        profile = read_text("include/hardware/boards/ZectrixNote4.h")
        self.assertIn('FIRMWARE_PRODUCT_NAME "TransitInk OS"', product)
        self.assertNotIn("FIRMWARE_PRODUCT_NAME", profile)

    def test_application_uses_board_support_not_gpio_numbers(self):
        main = read_text("src/main.cpp")
        support = read_text("src/hardware/BoardSupport.cpp")
        self.assertIn('#include "hardware/BoardSupport.h"', main)
        for action in (
            "configureButtons()",
            "configureHomeWakeup()",
            "disableHomeWakeup()",
            "homeButtonPressed()",
            "startButtonMonitoring()",
            "takeConfigClick()",
            "takeWidgetPageClick()",
            "takeFactoryResetHold()",
            "factoryResetUpButtonPressed()",
            "factoryResetDownButtonPressed()",
        ):
            self.assertIn(action, main)
        for direct_io in ("pinMode(", "digitalRead(", "gpio_wakeup_enable(", "rtc_gpio_deinit("):
            self.assertNotIn(direct_io, main)
            self.assertIn(direct_io, support)

    def test_renderer_is_separate_from_panel_controller(self):
        renderer = read_text("src/EInkDisplay.cpp")
        selector = read_text("include/hardware/SelectedDisplayDriver.h")
        driver = read_text("src/hardware/displays/Ssd1683DisplayDriver.cpp")
        self.assertIn("SelectedDisplayDriver panel", renderer)
        self.assertNotIn("sendCommand(", renderer)
        self.assertNotIn("SSD1683", renderer)
        self.assertIn("DisplayDriverFor<DisplayDriverKind::Ssd1683>", selector)
        self.assertIn("Ssd1683DisplayDriver::sendCommand", driver)
        self.assertIn("display.busyActiveLevel", driver)
        power_on = driver.split("void Ssd1683DisplayDriver::powerOn()", 1)[1]
        power_on = power_on.split("void Ssd1683DisplayDriver::powerOff()", 1)[0]
        self.assertLess(power_on.index("gpio_hold_dis(powerPin)"),
                        power_on.index("digitalWrite(display.powerPin"))
        self.assertLess(power_on.index("digitalWrite(display.powerPin"),
                        power_on.index("gpio_hold_en(powerPin)"))

    def test_battery_monitor_consumes_selected_profile(self):
        monitor = read_text("src/BatteryMonitor.cpp")
        self.assertIn("kBoardProfile.battery", monitor)
        self.assertIn("battery.sensePowerPin", monitor)
        self.assertIn("battery.chargeDetectActiveLevel", monitor)
        self.assertIn("battery.voltageMultiplier", monitor)
        self.assertIn("battery.chargeIndicatorPin", monitor)
        self.assertNotIn("BATTERY_ADC_PIN", monitor)

    def test_board_specific_platformio_settings_stay_in_board_environment(self):
        ini = read_text("platformio.ini")
        common, zectrix = ini.split("[env:zectrix_note4]", 1)
        self.assertNotIn("board =", common)
        self.assertNotIn("board_build.flash_size", common)
        self.assertIn("board = esp32-s3-devkitc-1", zectrix)
        self.assertIn("board_build.flash_size = 16MB", zectrix)

    def test_device_scripts_accept_profile_specific_settings(self):
        backup = read_text("scripts/backup_flash.sh")
        flash = read_text("scripts/flash_firmware.sh")
        restore = read_text("scripts/restore_flash.sh")
        self.assertIn("TRANSITINK_BOARD", backup)
        self.assertIn("ESP32_CHIP", backup)
        self.assertIn("ESP32_FLASH_SIZE", backup)
        self.assertIn("PLATFORMIO_ENV", flash)
        self.assertIn("ESP32_CHIP", restore)
        self.assertIn("ESP32_FLASH_SIZE", restore)
        self.assertIn('PORT="${1:-${ESP32_PORT:-}}"', backup)
        self.assertIn('PORT="${1:-${ESP32_PORT:-}}"', flash)
        self.assertIn('PORT="${2:-${ESP32_PORT:-}}"', restore)
        for script in (backup, flash, restore):
            self.assertNotIn("/dev/cu.", script)
            self.assertIn('if [[ -z "$PORT" ]]', script)


if __name__ == "__main__":
    unittest.main()
