# TransitInk OS web installer

This directory contains the dependency-free static installer source. Device
compatibility, product imagery and manifests are declared in `devices.json`; the
page uses that catalog without hard-coding the interface to one board. Add a
catalog entry, an image under `assets/`, and a matching generated manifest when
another hardware profile is ready for web installation.

`manifest.json` and the merged ESP32-S3 image provide first installation and
full erase/reinstallation. `ota-manifest.json` and the raw application image
provide the separate on-device update path that preserves NVS and LittleFS.
They are generated together into `dist/installer/` by:

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
  .venv/bin/python scripts/package_installer.py
```

Do not commit generated `.bin` files. A `vX.Y.Z` tag runs the release workflow,
checks that the tag matches `FIRMWARE_VERSION`, publishes both images and their
checksums as GitHub Release assets, and deploys the installer plus OTA manifest
through GitHub Pages.
