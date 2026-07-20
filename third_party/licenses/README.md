# Retained third-party licence texts

This directory contains verbatim licence files for components distributed in
TransitInk OS firmware or its browser installer. The project `LICENSE` applies
only to TransitInk-authored material; it does not replace these terms.

Firmware component versions are pinned in `platformio.ini`. Browser component
versions are recorded in `installer/esp-web-tools/THIRD_PARTY_NOTICES.md`.
Font and vendored-source licences remain beside their respective sources and
are copied into binary releases by `scripts/package_installer.py`.

`third_party/ARDUINO_ESP32_BUILD_VERSIONS.txt` is copied verbatim from the
ESP32-S3 framework package used by the pinned build and records the corresponding
framework, ESP-IDF, TinyUSB, and component versions.
