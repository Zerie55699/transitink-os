# TransitInk OS web installer

This directory contains only the static installer source. `manifest.json` and
the merged ESP32-S3 firmware image are generated into `dist/installer/` by:

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
  .venv/bin/python scripts/package_installer.py
```

Do not commit generated `.bin` files. A `vX.Y.Z` tag runs the release workflow,
checks that the tag matches `FIRMWARE_VERSION`, publishes the merged image and
checksum as GitHub Release assets, and deploys this installer through GitHub
Pages.
