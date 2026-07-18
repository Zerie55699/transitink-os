# TransitInk OS

TransitInk OS is an ESP32-S3 firmware for a 400×300 e-paper transit dashboard,
developed for the Zectrix Note 4 hardware profile. It displays four independently
configured widgets and includes an on-device Traditional Chinese settings portal.

Supported widgets:

- Bus ETA for KMB, Long Win Bus, and Citybus
- Green Minibus ETA for Hong Kong Island, Kowloon, and the New Territories
- MTR ETA for heavy rail and Light Rail
- Transport Department journey-time indicators

TransitInk OS source code is licensed under the
[Apache License 2.0](LICENSE). Redistributable fonts, generated glyphs, vendored
components, and transport data retain the separate terms recorded in
[Third-party notices](THIRD_PARTY_NOTICES.md) and
[Third-party data](THIRD_PARTY_DATA.md).

## Quick start

The helper scripts are intended for macOS or Linux. Python 3 and a C/C++
toolchain are required.

```bash
scripts/install_tools.sh
.venv/bin/python scripts/generate_hk_glyph_font.py --check
.venv/bin/python scripts/generate_transit_route_catalog.py --check
python3 -m unittest discover -s tests -p "test_*.py" -q
PLATFORMIO_CORE_DIR="$PWD/.platformio" .venv/bin/platformio run -e zectrix_note4
```

Before replacing the firmware, set the serial port and create a full 16 MiB
backup:

```bash
export ESP32_PORT=/dev/cu.usbmodemXXXX
scripts/backup_flash.sh "$ESP32_PORT"
scripts/flash_firmware.sh "$ESP32_PORT"
```

If the new firmware does not work, restore the original image:

```bash
scripts/restore_flash.sh backups/<backup>.bin "$ESP32_PORT"
```

Flashing or restoring firmware can make a device temporarily unusable. Confirm
the board, flash size, and serial port before running either operation.

## First boot

When no valid settings exist, the device starts a `TransitInk-xxxx` Wi-Fi access
point. Connect to it, open `http://192.168.4.1/`, enter the Wi-Fi settings, and
configure four ordered widget slots. A slot may also be disabled.

TransitInk OS refreshes MTR every 30 seconds, bus and Green Minibus arrivals
every 60 seconds, and journey time every 120 seconds. Green Minibus setup uses
the official region, route, direction/variation, and stop identifiers. The
firmware migrates supported legacy route settings into the four-slot
configuration.

The settings portal reads its bus, Green Minibus, MTR, and Light Rail route and
stop directories from a versioned catalog embedded in the firmware. It remains
usable without Internet access; only ETA refreshes are live. Maintainers refresh
the catalog explicitly with:

```bash
.venv/bin/python scripts/generate_transit_route_catalog.py --refresh
```

Normal builds never contact a transport provider. A public release needs no
external catalog host: if a user cannot find a new route or stop, the
"更新及省電" page offers "找不到站牌？更新所有路線". The device refreshes the
complete KMB/Long Win, Citybus, and Green Minibus route indexes, then refreshes
the routes currently used by the four widget drafts. Other routes fetch their
stop detail once when first selected after an index update. Updated indexes and
route overrides are written atomically to LittleFS and reused on later visits. See
[Third-party data](THIRD_PARTY_DATA.md) for source attribution and
[Project structure](docs/PROJECT_STRUCTURE.md) for the storage boundary.

## Repository layout

```text
include/        C++ headers and board configuration
src/            firmware, clients, display, portal, and application entry point
src/core/       hardware-independent domain logic
src/generated/  generated OFL-licensed bitmap glyph data
data/catalog/   versioned gzip transport catalog and integrity metadata
src/hardware/   ESP32 board support and display-driver implementations
src/providers/  widget provider adapters
include/hardware/ compile-time board profiles and hardware interfaces
installer/      GitHub Pages installer source; manifest and binary are generated
lib/            vendored third-party source with its own licence
third_party/    redistributable assets with pinned provenance and licences
scripts/        setup, font generation, backup, flash, and restore tools
test_host/      native C++ behaviour tests and bounded API fixtures
test_native/    PlatformIO Unity tests and Arduino compatibility shims
tests/          Python structure tests and native-test orchestration
docs/           architecture, development, and historical design notes
```

See [Project structure](docs/PROJECT_STRUCTURE.md) for module boundaries and
[Development](docs/DEVELOPMENT.md) for the complete contributor workflow.
Third-party asset and generated-data terms are recorded in
[Third-party notices](THIRD_PARTY_NOTICES.md) and
[Third-party data](THIRD_PARTY_DATA.md).

## Hardware profiles

`zectrix_note4` is the default PlatformIO environment. Its selected profile is
`include/hardware/boards/ZectrixNote4.h`, using an SSD1683-style 400×300 panel.
Product, widget, network, and portal code do not contain this board's GPIO
mapping.

See [Adding hardware](docs/ADDING_HARDWARE.md) for the supported extension path.
Do not assume the Zectrix pinout or flash layout is safe for another board.

## Web installer and releases

The browser installer is maintained in [`installer/`](installer/) so its source,
firmware version, merged image and manifest are released together. Build a local
installer package after the firmware build with:

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
  .venv/bin/python scripts/package_installer.py
```

Generated Pages content is written to `dist/installer/` and is not committed.
Pushing a `vX.Y.Z` tag whose version matches `FIRMWARE_VERSION` publishes the
merged image and checksum as GitHub Release assets, then deploys the same package
through GitHub Pages. The canonical installer is
[https://wongshingg.github.io/transitink-os/](https://wongshingg.github.io/transitink-os/).

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a change. In
particular, keep credentials, flash backups, generated build output, and local
device state out of commits.
