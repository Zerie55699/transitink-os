# Project structure

TransitInk OS keeps the standard PlatformIO source layout while separating
hardware-independent behaviour from device and network integration. This is
intentional: the pure C++ layer can be tested on a development machine without
an ESP32 toolchain or connected device.

## Directory map

```text
.
├── .github/workflows/           Continuous integration and release workflows
├── data/catalog/                Checked-in gzip transport catalog baseline
├── include/                     Firmware headers
│   ├── core/                    Hardware-independent domain interfaces
│   ├── hardware/                Board profiles and display-driver selection
│   └── providers/               Provider adapter interfaces
├── src/                         Production implementation
│   ├── core/                    Parsing, validation, scheduling, and snapshots
│   ├── hardware/                ESP32 buttons, wake support, and panel drivers
│   ├── providers/               Bus, rail, and journey-time provider adapters
│   └── main.cpp                 Device lifecycle and application composition
├── lib/yxml/                    Vendored XML parser and upstream licence
├── scripts/                     Developer and device-operation tools
├── installer/                   Static GitHub Pages installer source
├── test_host/                   Native C++ tests, fixtures, and test shims
├── test_native/                 PlatformIO Unity tests for production paths
├── tests/                       Python structural/integration tests
├── docs/                        Public technical documentation
├── platformio.ini               ESP32-S3 firmware environments
├── platformio.native.ini        Native PlatformIO test environments
├── partitions.csv               16 MiB flash partition layout
└── requirements-dev.txt         Pinned local Python tooling
```

## Production layers

### Application composition

`src/main.cpp` owns boot, Wi-Fi connection, wake/sleep behaviour, the scheduler,
display updates, configuration mode, and the top-level device lifecycle. It
constructs the concrete clients and providers but should not contain provider
parsing rules.

### Hardware profiles and adapters

`include/hardware/boards/` contains compile-time board profiles. A profile owns
display pins and controller type, battery measurement, button mapping and
polarity, and wake capabilities. `BoardSupport` exposes button and wake actions
to the application without leaking GPIO numbers into `main.cpp`.

`SelectedDisplayDriver` maps a profile's controller type to a driver under
`src/hardware/displays/`. `EInkDisplay` owns only the product renderer and frame
refresh policy. A board using the same controller only needs a new profile;
another controller also needs a driver implementing the selected driver's
`begin`, `show`, and `showPartialRegion` surface.

The current dashboard renderer requires an exact 400×300 canvas. Supporting a
different screen size requires a renderer/layout addition,
not board-specific conditionals in the existing renderer.

### Device and service integration

The top level of `src/` contains the Arduino-facing implementation:

- `EInkDisplay` and `HkGlyphFont` render the 400×300 user interface.
- `BatteryMonitor` consumes the selected board's battery profile.
- `ConfigStore`, `AppConfig`, `ConfigPortal`, `PortalConfigCodec`, and
  `TransitInkPortalPage` own persistent settings and the local web portal.
- `KmbClient`, `CitybusClient`, `GmbClient`, `MtrClient`, `LightRailClient`,
  `TflClient`, `JourneyTimeClient`, and `WeatherClient` perform remote data
  access.
- `ConfigPortal` streams versioned gzip bus, Green Minibus, MTR, and Light Rail
  assets directly from generated PROGMEM arrays without decompressing them in
  ESP32 RAM. `WidgetCatalogService` owns compact journey-time projections and
  the refreshed route index and per-route LittleFS stop overrides.

### Frontend transport catalog

`scripts/generate_transit_route_catalog.py` is the maintainer path for bulk
low-frequency official route and stop sources. It validates and deterministically
generates `index.json.gz`, four provider stop packs, `rail.json.gz`, a manifest,
and the C++ PROGMEM arrays under `src/generated/`. A normal firmware build reads
the tracked output and performs no catalog downloads.

The portal loads the embedded index and Hong Kong and London rail assets alongside
`/api/config`, loads a provider stop pack only when it is needed, then resolves
Hong Kong and London route, direction, and stop selections in the browser.
TfL is contacted only for live arrivals, explicit updates, or a route newer
than the bundled baseline; refreshed route data are cached in LittleFS.
Journey-time locations remain small API projections, and live ETA clients are
independent from the embedded catalog path.

Catalog records may carry both `label_tc` and `label_en`. The portal and
electronic-paper view choose the label for `ui_locale`; if that field is empty,
they display the provider's available source-language label. Saved widget
settings retain both labels so changing language does not require selecting the
route or stop again. `WeatherClient` applies the same rule by requesting the
Hong Kong Observatory `tc` or `en` response for Hong Kong, or by using the
selected bilingual city label with the Open-Meteo UK Met Office model for the
United Kingdom.

Weather location and device time zone are independent settings. The time-zone
core maps the stable `Asia/Hong_Kong` and `Europe/London` IDs to POSIX time-zone
rules. The London rule includes GMT/BST transitions and is applied before
calculating the display clock, background maintenance, or daily scheduled wake.
Existing settings without a time-zone field default to Hong Kong. In the portal,
weather locations are grouped behind a Hong Kong or United Kingdom selector;
the stored location ID still determines the group, so existing settings need no
migration.

When a route or stop is absent, the user chooses "找不到站牌？更新所有路線" under
"設定". The device refreshes the complete KMB/Long Win, Citybus, and Green
Minibus route indexes. Routes already used by the configured widget drafts refresh
their directions and stops in the same operation; another route refreshes and
persists its detail the first time it is selected after the index update.
`WidgetCatalogService` validates the responses and atomically replaces both the
updated route index and route-specific LittleFS JSON overrides using temporary
and backup files. The portal overlays those files on the firmware baseline on
later visits. No catalog base URL, signing key, release server, or whole-catalog
first-run download is required.

### Domain core

`include/core/` and `src/core/` contain bounded parsing and business rules that
do not depend on display or Wi-Fi hardware. This includes widget configuration,
normalised snapshots, scheduling, portal request authentication, battery
status, and journey-time XML parsing.

New rules should go here when they can be expressed without Arduino APIs. This
keeps them fast to compile and easy to exercise with strict host warnings.

### Provider adapters

`src/providers/` converts bus, Green Minibus, Hong Kong rail, TfL rail, and
journey-time responses into common widget snapshots. `WidgetProviderRouter` is
the dispatch boundary used by `WidgetScheduler`; display code should consume
snapshots instead of calling an operator client directly.

## Test layers

The three test directories have distinct roles and should not be merged merely
for naming consistency:

- `test_host/` compiles pure C and C++ modules with the system compiler using
  `-Wall -Wextra -Werror`. It also owns bounded external-response fixtures.
- `test_native/` uses PlatformIO's native Unity runner for production code that
  needs small Arduino or Preferences compatibility shims.
- `tests/` contains Python tests that assert source boundaries, portal
  behaviour, and the firmware's structural contracts. `tests/test_core.py`
  also orchestrates the native C/C++ executables.

The single local verification entry point is:

```bash
python3 -m unittest discover -s tests -p "test_*.py" -q
```

The detailed board extension procedure is documented in
[Adding hardware](ADDING_HARDWARE.md).

## Generated and local-only content

These paths are deliberately excluded from Git:

- `.venv/`, `.platformio/`, and `.pio/` — local tools, packages, and builds
- `.catalog-cache/` — resumable official-source downloads used by the generator
- `.test-build/` — native test executables and objects
- `backups/` — full device flash images, which may contain credentials
- `dist/` — generated release assets and deployable installer package
- `.superpowers/` — local agent workspace

Tracked generated glyph and catalog C++ sources are required by the firmware
build. Their generators, deterministic inputs, gzip assets, manifests and
licensing boundaries live under `scripts/`, `data/catalog/`,
`THIRD_PARTY_NOTICES.md`, and `THIRD_PARTY_DATA.md`.

## Release and installer boundary

`installer/` contains only reviewed static source. `scripts/package_installer.py`
reads the single firmware version from `include/ProductConfig.h`, merges the
Zectrix Note 4 bootloader, partition table, Arduino boot application and
firmware at their verified offsets, then creates a relative ESP Web Tools
manifest. It also publishes the raw application image and a board-scoped OTA
manifest for settings-preserving updates. Both images have SHA-256 checksums and
release metadata under `dist/installer/`.

The release workflow rejects a tag that does not match `FIRMWARE_VERSION`. The
same generated first-install and OTA images are uploaded to the GitHub Release
and deployed through GitHub Pages, so neither update path can silently point at
different firmware from the release assets.
