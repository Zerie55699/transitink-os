# Corresponding source and rebuild information

TransitInk OS binary releases contain code linked with separately licensed
Arduino framework components. This document identifies the exact build inputs
and provides the normal route to rebuild or relink the firmware.

## TransitInk OS source

For a release tagged `vX.Y.Z`, use the source archive attached automatically to
the matching GitHub release or check out that exact tag from:

<https://github.com/Zerie55699/transitink-os>

The release tag must match `FIRMWARE_VERSION` in `include/ProductConfig.h`.

## Pinned framework source and binary libraries

- Platform package: pioarduino `platform-espressif32` 55.03.39, pinned by the
  URL in `platformio.ini`.
- Arduino core: 3.3.9, LGPL-2.1-or-later.
  Source: <https://github.com/espressif/arduino-esp32/tree/3.3.9>
- Arduino precompiled libraries: the 3.3.9 package published at
  <https://github.com/espressif/arduino-esp32/releases/download/3.3.9/esp32-core-3.3.9-libs.tar.xz>.
- Library build provenance for the ESP32-S3 package: `esp32-arduino-lib-builder`
  commit `43a8f6d`, ESP-IDF commit `735507283d`, Arduino commit `6cb835025`, and
  TinyUSB commit `5004a24b2`. The complete component version list is retained
  in `third_party/ARDUINO_ESP32_BUILD_VERSIONS.txt`.
- Library builder source:
  <https://github.com/espressif/esp32-arduino-lib-builder/tree/43a8f6d>
- ESP-IDF source:
  <https://github.com/pioarduino/esp-idf/tree/735507283d>

The full GNU Lesser General Public License 2.1 is retained at
`third_party/licenses/LGPL-2.1.txt` and is included with each binary release.

## Rebuild or relink

On macOS or Linux, check out the matching TransitInk OS tag and run:

```bash
scripts/install_tools.sh
PLATFORMIO_CORE_DIR="$PWD/.platformio" .venv/bin/platformio run -e zectrix_note4
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
  .venv/bin/python scripts/package_installer.py --expected-version X.Y.Z
```

PlatformIO downloads the pinned Arduino core, precompiled libraries, and the
declared source libraries. To use a modified Arduino core, replace the
`framework-arduinoespressif32` package in the project-local PlatformIO core with
your build of the 3.3.9-compatible source, then run the same build command. The
complete TransitInk application source is included and available for permitted
noncommercial purposes under the PolyForm Noncommercial License 1.0.0, so no
separate application object is required to produce a permitted modified
combined firmware image.

This information is provided to make the corresponding source and relinking
path available to binary recipients; each upstream component remains governed
by its own licence.
