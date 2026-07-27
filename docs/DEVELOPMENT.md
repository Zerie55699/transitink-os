# Development

## Prerequisites

- macOS or Linux with Bash
- Python 3
- a C and C++ compiler with C++17 support
- USB access to an ESP32-S3 device for backup, flash, or serial verification

The checked-in helper creates a repository-local virtual environment and
installs the pinned PlatformIO and esptool versions:

```bash
scripts/install_tools.sh
scripts/audit_python_tools.sh
```

PlatformIO packages are kept under `.platformio/` so builds do not depend on a
developer's global PlatformIO state. The firmware platform is pinned to
pioarduino 55.03.39 (Arduino ESP32 3.3.9 / ESP-IDF 5.5.4), while all directly
declared libraries use exact versions.

## Test

Run all structure tests, host C++ tests, and native PlatformIO Unity tests:

```bash
python3 -m unittest discover -s tests -p "test_*.py" -q
```

The suite expects `.venv/bin/platformio` to exist, so run the setup script once
before the first test.

Run the same high-severity C/C++ static-analysis gate used by CI and releases:

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
  .venv/bin/platformio check -e zectrix_note4 --fail-on-defect high
```

## Build

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" .venv/bin/platformio run -e zectrix_note4
```

Each supported board has its own PlatformIO environment and compile-time board
profile. See [Adding hardware](ADDING_HARDWARE.md) before introducing a new one.

The main build outputs are written below `.pio/build/zectrix_note4/` and are not
committed.

## Back up, flash, and restore

Always back up a device before its first TransitInk flash. The backup is a full
16 MiB image and may contain Wi-Fi credentials or other device state, so it must
remain under the ignored `backups/` directory. The helper creates backup files
with owner-only permissions; preserve those permissions when copying a backup.

```bash
export ESP32_PORT=/dev/cu.usbmodemXXXX
scripts/backup_flash.sh "$ESP32_PORT"
scripts/flash_firmware.sh "$ESP32_PORT"
```

Restore only a verified 16 MiB image created from the same hardware profile:

```bash
scripts/restore_flash.sh backups/<backup>.bin "$ESP32_PORT"
```

The scripts also accept `ESP32_PORT`, `ESP32_BAUD`, `ESP32_CHIP`,
`ESP32_FLASH_SIZE`, `TRANSITINK_BOARD`, and `PLATFORMIO_ENV` environment
variables. A serial port must be passed explicitly or supplied through
`ESP32_PORT`; the remaining defaults preserve the Zectrix Note 4 workflow.

## Regenerate the Traditional Chinese glyph table

`scripts/generate_hk_glyph_font.py` scans production source plus the deterministic
seed in `scripts/hk_glyph_seed.txt`. It rasterises the pinned Noto Sans CJK HK
Regular 2.004 font with the pinned Pillow version from `requirements-dev.txt`.
The generator verifies the vendored OTF SHA-256 before producing output, so it
does not depend on an installed system font, CoreText, or macOS `sips`.

```bash
.venv/bin/python scripts/generate_hk_glyph_font.py
.venv/bin/python scripts/generate_hk_glyph_font.py --check
```

Do not hand-edit `src/generated/HkGlyphFontData.cpp`. The stable declarations in
`include/HkGlyphFont.h` and lookup code in `src/HkGlyphFont.cpp` remain normal
project source. The generated bitmap data and source font are distributed under
OFL-1.1; their exact provenance and licence are retained under
`third_party/fonts/noto-sans-cjk-hk/`.

For local experiments only, a different font may be selected explicitly:

```bash
.venv/bin/python scripts/generate_hk_glyph_font.py \
  --font /path/to/font-file --allow-unverified-font
```

Do not commit experimental output. Release builds and CI must use the vendored
font and pass the default `--check` command.

## Change boundaries

- Put hardware-independent validation, parsing, and scheduling in `src/core/`.
- Put GPIO maps and electrical capabilities in a board profile; put controller
  commands in a hardware driver.
- Keep `main.cpp` free of direct `pinMode`, `digitalRead`, and GPIO wake calls.
- Keep operator-specific network handling in clients and normalisation in
  `src/providers/`.
- Keep display code dependent on normalised snapshots, not raw remote payloads.
- Never commit Wi-Fi credentials, flash backups, serial logs with secrets, or
  local portal tokens.
- Add or update a host/native test for behavioural changes and a structure test
  when preserving an architectural boundary matters.

## Installer and release package

After a successful firmware build, generate the exact package used by GitHub
Pages and GitHub Releases:

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
  .venv/bin/python scripts/package_installer.py
```

The output under `dist/installer/` includes the versioned merged first-install
image, the raw application image used for settings-preserving A/B OTA,
`manifest.json`, `ota-manifest.json`, `SHA256SUMS.txt`, and machine-readable
release metadata. It is
ignored because releases are reproducibly generated from source. Do not copy
the nested `.git` directory from the ignored legacy installer clone.

Before publishing, update `FIRMWARE_VERSION` and `CHANGELOG.md`, review the
third-party notices and catalog attribution, run the checks above from a clean
checkout, scan both the working tree and Git history for secrets, and verify the
candidate on its supported hardware. Commit the reviewed tree and push a
matching `vX.Y.Z` tag. The release workflow tests and builds from the tag before
publishing any asset or deploying GitHub Pages.
