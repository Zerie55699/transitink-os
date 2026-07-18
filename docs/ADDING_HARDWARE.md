# Adding hardware

TransitInk selects one board profile at compile time. A profile describes the
electrical facts needed by shared firmware; it does not contain widget, portal,
or transport-provider behaviour.

## Supported extension levels

1. **Same ESP32 family and SSD1683 panel:** add a board profile and PlatformIO
   environment.
2. **Different e-paper controller:** also add a display driver and register its
   `DisplayDriverKind` in `SelectedDisplayDriver.h`.
3. **Different screen layout:** also add or generalise the renderer. The current
   dashboard requires exactly 400×300 pixels.
4. **Non-ESP32 MCU/framework:** add a platform HAL for Wi-Fi, storage, sleep, and
   web serving. Board profiles alone do not remove Arduino/ESP32 dependencies.

## 1. Add a board profile

Create `include/hardware/boards/<BoardName>.h` with one `inline constexpr
BoardProfile kBoardProfile`. Declare:

- stable board id and display name;
- display controller, dimensions, SPI pins, power/busy levels, and clock;
- optional battery ADC, voltage multiplier, charge pins, and active levels;
- home/config/factory-reset buttons, input bias, active level, timings, and wake
  capability.

Use `kUnusedPin` for optional pins. Keep the profile free of Arduino headers so
it can be compiled by host tests.

## 2. Register the selector

Add a unique `TRANSITINK_BOARD_<NAME>` branch in
`include/hardware/BoardProfile.h`. Selection must remain fail-closed: a build
without a known board macro should stop with a compile error.

## 3. Add a PlatformIO environment

Add `[env:<board_id>]` to `platformio.ini`. Board, flash, PSRAM, partition, USB,
and upload settings belong in this environment rather than the shared `[env]`
section. Its build flags must select exactly one board profile.

Build it explicitly:

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" .venv/bin/platformio run -e <board_id>
```

For device scripts, set the matching values instead of relying on Zectrix
defaults:

```bash
export PLATFORMIO_ENV=<board_id>
export TRANSITINK_BOARD=<board_id>
export ESP32_CHIP=<esptool-chip>
export ESP32_FLASH_SIZE=<size-in-hex>
export ESP32_PORT=<serial-port>
```

## 4. Add a display driver when needed

A display driver exposes:

```cpp
void begin();
void show(const uint8_t* frame);
void showPartialRegion(const uint8_t* current,
                       const uint8_t* previous,
                       int x, int y, int width, int height);
```

Controller commands, pin I/O, busy polarity, SPI setup, and power sequencing stay
inside the driver. Rendering, glyphs, widget layout, and partial-refresh policy
stay in `EInkDisplay`.

## 5. Verify

- Add a host test for the profile's exact electrical configuration.
- Run the complete test suite.
- Build every affected PlatformIO environment.
- Back up the target device with the correct chip and flash-size settings.
- Flash and verify boot, display refresh, buttons, wake, battery, Wi-Fi portal,
  and restore before marking the board supported.
