#!/usr/bin/env python3
"""Build the versioned ESP Web Tools installer from a PlatformIO firmware build."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
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
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32-S3",
                "parts": [{"path": f"firmware/{firmware_name}", "offset": 0}],
            }
        ],
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
    esptool = require_file(core_dir / "packages" / "tool-esptoolpy" / "esptool.py")
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
        str(esptool),
        "--chip",
        "esp32s3",
        "merge_bin",
        "--flash_mode",
        "qio",
        "--flash_size",
        "16MB",
        "--output",
        str(output_file),
    ]
    for offset, source in inputs:
        command.extend((offset, str(source)))
    subprocess.run(command, cwd=ROOT, check=True)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


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
    shutil.copy2(ROOT / "installer" / "index.html", output_dir / "index.html")
    shutil.copy2(ROOT / "installer" / ".nojekyll", output_dir / ".nojekyll")
    shutil.copytree(
        ROOT / "installer" / "esp-web-tools",
        output_dir / "esp-web-tools",
    )

    firmware_dir = output_dir / "firmware"
    firmware_dir.mkdir()
    firmware_name = f"transitink-zectrix-note4-v{version}.bin"
    firmware_path = firmware_dir / firmware_name
    merge_firmware(build_dir, firmware_path)

    manifest = installer_manifest(version, firmware_name)
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    firmware_digest = sha256(firmware_path)
    (output_dir / "SHA256SUMS.txt").write_text(
        f"{firmware_digest}  {firmware_name}\n",
        encoding="utf-8",
    )
    (output_dir / "release-metadata.json").write_text(
        json.dumps(
            {
                "version": version,
                "board": "zectrix_note4",
                "chip_family": "ESP32-S3",
                "firmware": firmware_name,
                "size": firmware_path.stat().st_size,
                "sha256": firmware_digest,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
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
