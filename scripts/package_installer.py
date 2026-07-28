#!/usr/bin/env python3
"""Build the versioned ESP Web Tools installer from a PlatformIO firmware build."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path
from typing import Optional


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BUILD_DIR = ROOT / ".pio" / "build" / "zectrix_note4"
DEFAULT_OUTPUT_DIR = ROOT / "dist" / "installer"
VERSION_PATTERN = re.compile(r'^#define FIRMWARE_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"$', re.MULTILINE)


def firmware_version(product_config: Path = ROOT / "include" / "ProductConfig.h") -> str:
    match = VERSION_PATTERN.search(product_config.read_text(encoding="utf-8"))
    if match is None:
        raise ValueError(f"FIRMWARE_VERSION not found in {product_config}")
    return match.group(1)


def installer_manifest(version: str, firmware_name: str) -> dict[str, object]:
    return {
        "name": "TransitInk OS Installer",
        "version": version,
        # This installer writes a merged factory image at offset 0. It is only
        # safe to present it as a full install, so erasing is mandatory and is
        # enforced by installer/esp-web-tools/full-erase-policy.js.
        "new_install_prompt_erase": False,
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "parts": [{"path": f"firmware/{firmware_name}", "offset": 0}],
            }
        ],
    }


def ota_manifest(
    version: str, firmware_name: str, size: int, digest: str
) -> dict[str, object]:
    return {
        "schema_version": 1,
        "product": "transitink-os",
        "board": "zectrix_note4",
        "version": version,
        "firmware": f"firmware/{firmware_name}",
        "size": size,
        "sha256": digest,
    }


def require_file(path: Path) -> Path:
    if not path.is_file():
        raise FileNotFoundError(f"required release input is missing: {path}")
    return path


def platformio_core_dir() -> Path:
    configured = os.environ.get("PLATFORMIO_CORE_DIR")
    if configured:
        path = Path(configured)
        return path if path.is_absolute() else ROOT / path
    return ROOT / ".platformio"


def merge_firmware(build_dir: Path, output_file: Path) -> None:
    core_dir = platformio_core_dir()
    factory_image = require_file(build_dir / "firmware.factory.bin")
    boot_app0 = require_file(
        core_dir
        / "packages"
        / "framework-arduinoespressif32"
        / "tools"
        / "partitions"
        / "boot_app0.bin"
    )
    inputs = (
        ("0x0000", require_file(build_dir / "bootloader.bin")),
        ("0x8000", require_file(build_dir / "partitions.bin")),
        ("0xe000", boot_app0),
        ("0x10000", require_file(build_dir / "firmware.bin")),
    )
    command = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        "esp32s3",
        "merge-bin",
        "--flash-mode",
        "keep",
        "--flash-size",
        "16MB",
        "--output",
        str(output_file),
    ]
    for offset, source in inputs:
        command.extend((offset, str(source)))
    subprocess.run(command, cwd=ROOT, check=True)
    if sha256(output_file) != sha256(factory_image):
        raise ValueError(
            "merged installer firmware differs from PlatformIO factory image"
        )


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_legal_material(output_dir: Path) -> Path:
    legal_dir = output_dir / "legal"
    licences_dir = legal_dir / "licenses"
    licences_dir.mkdir(parents=True)

    documents = {
        ROOT / "LICENSE": legal_dir / "LICENSE.txt",
        ROOT / "THIRD_PARTY_NOTICES.md": legal_dir / "THIRD_PARTY_NOTICES.md",
        ROOT / "THIRD_PARTY_DATA.md": legal_dir / "THIRD_PARTY_DATA.md",
        ROOT / "CORRESPONDING_SOURCE.md": legal_dir / "CORRESPONDING_SOURCE.md",
        ROOT / "installer" / "assets" / "SOURCE.md": legal_dir
        / "PRODUCT_IMAGE_SOURCE.md",
        ROOT / "installer" / "esp-web-tools" / "THIRD_PARTY_NOTICES.md": legal_dir
        / "ESP_WEB_TOOLS_THIRD_PARTY_NOTICES.md",
        ROOT / "third_party" / "fonts" / "noto-sans-cjk-hk" / "SOURCE.md": legal_dir
        / "NOTO_SANS_CJK_HK_SOURCE.md",
        ROOT
        / "third_party"
        / "fonts"
        / "noto-sans-cjk-hk"
        / "UPSTREAM-NOTICE.md": legal_dir / "NOTO_SANS_CJK_HK_NOTICE.md",
        ROOT / "third_party" / "fonts" / "noto-sans-cjk-hk" / "OFL.txt": licences_dir
        / "Noto-Sans-CJK-HK-OFL-1.1.txt",
        ROOT / "third_party" / "fonts" / "unifont" / "SOURCE.md": legal_dir
        / "GNU_UNIFONT_SOURCE.md",
        ROOT / "third_party" / "fonts" / "unifont" / "OFL-1.1.txt": licences_dir
        / "GNU-Unifont-OFL-1.1.txt",
        ROOT / "lib" / "yxml" / "LICENSE": licences_dir / "yxml-MIT.txt",
        ROOT / "installer" / "esp-web-tools" / "LICENSE": licences_dir
        / "ESP-Web-Tools-Apache-2.0.txt",
    }
    for source, destination in documents.items():
        shutil.copy2(require_file(source), destination)

    retained_licences_dir = ROOT / "third_party" / "licenses"
    retained_licences = require_file(retained_licences_dir / "README.md")
    shutil.copy2(retained_licences, licences_dir / "README.md")
    required_licences = (
        "Adafruit-BusIO-MIT.txt",
        "Adafruit-GFX-BSD-3-Clause.txt",
        "ArduinoJson-MIT.txt",
        "Improv-WiFi-Serial-SDK-Apache-2.0.txt",
        "LGPL-2.1.txt",
        "Lit-BSD-3-Clause.txt",
        "Material-Web-Apache-2.0.txt",
        "Pako-MIT.txt",
        "QRCode-MIT.txt",
        "Slate-MIT.txt",
        "VibecodingVoice-MIT.txt",
        "atob-lite-MIT.txt",
        "esptool-js-Apache-2.0.txt",
        "tslib-0BSD.txt",
    )
    for filename in required_licences:
        source = require_file(retained_licences_dir / filename)
        shutil.copy2(source, licences_dir / source.name)

    framework_versions = require_file(
        ROOT / "third_party" / "ARDUINO_ESP32_BUILD_VERSIONS.txt"
    )
    shutil.copy2(framework_versions, legal_dir / "ARDUINO_ESP32_BUILD_VERSIONS.txt")
    return legal_dir


def create_firmware_bundle(
    output_dir: Path,
    firmware_path: Path,
    ota_firmware_path: Path,
    version: str,
    legal_dir: Path,
) -> Path:
    bundle_path = output_dir / f"transitink-zectrix-note4-v{version}.zip"
    with zipfile.ZipFile(bundle_path, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
        bundle.write(firmware_path, firmware_path.name)
        bundle.write(ota_firmware_path, ota_firmware_path.name)
        for filename in (
            "manifest.json",
            "ota-manifest.json",
            "SHA256SUMS.txt",
            "release-metadata.json",
        ):
            source = require_file(output_dir / filename)
            bundle.write(source, source.name)
        for source in sorted(legal_dir.rglob("*")):
            if source.is_file():
                bundle.write(source, source.relative_to(output_dir).as_posix())
    return bundle_path


def package_installer(
    build_dir: Path, output_dir: Path, expected_version: Optional[str]
) -> Path:
    version = firmware_version()
    if expected_version is not None and version != expected_version:
        raise ValueError(
            f"release version mismatch: tag expects {expected_version}, firmware is {version}"
        )

    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)
    for filename in ("index.html", "styles.css", "app.js", "devices.json", ".nojekyll"):
        shutil.copy2(ROOT / "installer" / filename, output_dir / filename)
    shutil.copytree(
        ROOT / "installer" / "esp-web-tools",
        output_dir / "esp-web-tools",
    )
    shutil.copytree(
        ROOT / "installer" / "assets",
        output_dir / "assets",
    )
    legal_dir = copy_legal_material(output_dir)

    firmware_dir = output_dir / "firmware"
    firmware_dir.mkdir()
    firmware_name = f"transitink-zectrix-note4-v{version}.bin"
    firmware_path = firmware_dir / firmware_name
    merge_firmware(build_dir, firmware_path)
    ota_firmware_name = f"transitink-zectrix-note4-ota-v{version}.bin"
    ota_firmware_path = firmware_dir / ota_firmware_name
    shutil.copy2(require_file(build_dir / "firmware.bin"), ota_firmware_path)

    manifest = installer_manifest(version, firmware_name)
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    firmware_digest = sha256(firmware_path)
    ota_firmware_digest = sha256(ota_firmware_path)
    (output_dir / "ota-manifest.json").write_text(
        json.dumps(
            ota_manifest(
                version,
                ota_firmware_name,
                ota_firmware_path.stat().st_size,
                ota_firmware_digest,
            ),
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    (output_dir / "SHA256SUMS.txt").write_text(
        f"{firmware_digest}  {firmware_name}\n"
        f"{ota_firmware_digest}  {ota_firmware_name}\n",
        encoding="utf-8",
    )
    (output_dir / "release-metadata.json").write_text(
        json.dumps(
            {
                "version": version,
                "board": "zectrix_note4",
                "chip_family": "ESP32-S3",
                "firmware": firmware_name,
                "bundle": f"transitink-zectrix-note4-v{version}.zip",
                "size": firmware_path.stat().st_size,
                "sha256": firmware_digest,
                "ota_firmware": ota_firmware_name,
                "ota_size": ota_firmware_path.stat().st_size,
                "ota_sha256": ota_firmware_digest,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    create_firmware_bundle(
        output_dir, firmware_path, ota_firmware_path, version, legal_dir
    )
    return firmware_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=DEFAULT_BUILD_DIR)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--expected-version")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        firmware_path = package_installer(
            args.build_dir.resolve(), args.output_dir.resolve(), args.expected_version
        )
    except (FileNotFoundError, ValueError, subprocess.CalledProcessError) as error:
        print(f"release packaging failed: {error}", file=sys.stderr)
        return 1
    print(f"installer package: {firmware_path.parent.parent}")
    print(f"firmware: {firmware_path.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
