# TransitInk OS web installer

This directory contains the dependency-free static installer source. Device
compatibility, product imagery and manifests are declared in `devices.json`; the
page uses that catalog without hard-coding the interface to one board. Add a
catalog entry, an image under `assets/`, and a matching generated manifest when
another hardware profile is ready for web installation.

`manifest.json` and the merged ESP32-S3 firmware image are generated into
`dist/installer/` by:

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
  .venv/bin/python scripts/package_installer.py
```

Do not commit generated `.bin` files. A `vX.Y.Z` tag runs the release workflow,
checks that the tag matches `FIRMWARE_VERSION`, publishes the merged image and
checksum as GitHub Release assets, and deploys this installer through GitHub
Pages.
