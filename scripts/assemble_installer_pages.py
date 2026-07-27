#!/usr/bin/env python3
"""Assemble the current installer UI around the latest released firmware bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
import zipfile
from pathlib import Path, PurePosixPath
from typing import Optional


ROOT = Path(__file__).resolve().parents[1]
INSTALLER_SOURCE = ROOT / "installer"
VERSION_PATTERN = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
SHA256_LINE_PATTERN = re.compile(r"^([0-9a-f]{64})  ([^/]+)$")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_archive_path(filename: str) -> PurePosixPath:
    path = PurePosixPath(filename)
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise ValueError(f"unsafe release bundle path: {filename}")
    return path


def extract_release_bundle(bundle: Path, output_dir: Path) -> None:
    with zipfile.ZipFile(bundle) as archive:
        for member in archive.infolist():
            relative_path = validate_archive_path(member.filename)
            destination = output_dir.joinpath(*relative_path.parts)
            if member.is_dir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(member) as source, destination.open("wb") as target:
                shutil.copyfileobj(source, target)


def require_file(path: Path) -> Path:
    if not path.is_file():
        raise FileNotFoundError(f"required release input is missing: {path}")
    return path


def require_directory(path: Path) -> Path:
    if not path.is_dir():
        raise FileNotFoundError(f"required release input is missing: {path}")
    return path


def validate_release(output_dir: Path, bundle: Path) -> tuple[str, Optional[str]]:
    metadata = json.loads(
        require_file(output_dir / "release-metadata.json").read_text(encoding="utf-8")
    )
    manifest = json.loads(
        require_file(output_dir / "manifest.json").read_text(encoding="utf-8")
    )
    version = metadata.get("version")
    firmware_name = metadata.get("firmware")
    bundle_name = metadata.get("bundle")
    expected_digest = metadata.get("sha256")
    ota_fields = (
        metadata.get("ota_firmware"),
        metadata.get("ota_size"),
        metadata.get("ota_sha256"),
    )
    has_ota = all(value is not None for value in ota_fields)
    if any(value is not None for value in ota_fields) and not has_ota:
        raise ValueError("release bundle has incomplete OTA metadata")
    ota_firmware_name = ota_fields[0] if has_ota else None
    expected_ota_digest = ota_fields[2] if has_ota else None

    if not isinstance(version, str) or VERSION_PATTERN.fullmatch(version) is None:
        raise ValueError("release bundle has an invalid version")
    expected_firmware_name = f"transitink-zectrix-note4-v{version}.bin"
    expected_ota_firmware_name = (
        f"transitink-zectrix-note4-ota-v{version}.bin"
    )
    expected_bundle_name = f"transitink-zectrix-note4-v{version}.zip"
    if firmware_name != expected_firmware_name:
        raise ValueError("release bundle firmware name does not match its version")
    if has_ota and ota_firmware_name != expected_ota_firmware_name:
        raise ValueError("release bundle OTA firmware name does not match its version")
    if bundle_name != expected_bundle_name or bundle.name != expected_bundle_name:
        raise ValueError("release bundle filename does not match its version")
    if manifest.get("version") != version:
        raise ValueError("installer manifest version does not match the release")

    builds = manifest.get("builds")
    try:
        firmware_path = builds[0]["parts"][0]["path"]
    except (IndexError, KeyError, TypeError) as error:
        raise ValueError("installer manifest has an invalid firmware path") from error
    if firmware_path != f"firmware/{firmware_name}":
        raise ValueError("installer manifest firmware path does not match the release")

    firmware = require_file(output_dir / firmware_name)
    actual_digest = sha256(firmware)
    if actual_digest != expected_digest:
        raise ValueError("released firmware SHA-256 does not match its metadata")

    actual_ota_digest = None
    if has_ota:
        assert isinstance(ota_firmware_name, str)
        ota_firmware = require_file(output_dir / ota_firmware_name)
        actual_ota_digest = sha256(ota_firmware)
        if actual_ota_digest != expected_ota_digest:
            raise ValueError("released OTA firmware SHA-256 does not match its metadata")
        ota_manifest = json.loads(
            require_file(output_dir / "ota-manifest.json").read_text(encoding="utf-8")
        )
        expected_ota_manifest = {
            "schema_version": 1,
            "product": "transitink-os",
            "board": "zectrix_note4",
            "version": version,
            "firmware": f"firmware/{ota_firmware_name}",
            "size": ota_firmware.stat().st_size,
            "sha256": actual_ota_digest,
        }
        if ota_manifest != expected_ota_manifest:
            raise ValueError("released OTA manifest does not match its firmware")
    elif (output_dir / "ota-manifest.json").exists():
        raise ValueError("legacy release bundle contains unexpected OTA manifest")

    checksum_lines = require_file(output_dir / "SHA256SUMS.txt").read_text(
        encoding="utf-8"
    ).splitlines()
    checksums: dict[str, str] = {}
    for line in checksum_lines:
        checksum_match = SHA256_LINE_PATTERN.fullmatch(line)
        if checksum_match is None or checksum_match.group(2) in checksums:
            raise ValueError("released firmware SHA256SUMS entry is invalid")
        checksums[checksum_match.group(2)] = checksum_match.group(1)
    expected_checksums = {firmware_name: actual_digest}
    if has_ota:
        assert isinstance(ota_firmware_name, str)
        assert isinstance(actual_ota_digest, str)
        expected_checksums[ota_firmware_name] = actual_ota_digest
    if checksums != expected_checksums:
        raise ValueError("released firmware SHA256SUMS entries are invalid")
    return firmware_name, ota_firmware_name


def overlay_installer_source(output_dir: Path) -> None:
    for filename in ("index.html", "styles.css", "app.js", "devices.json", ".nojekyll"):
        shutil.copy2(require_file(INSTALLER_SOURCE / filename), output_dir / filename)
    for directory in ("esp-web-tools", "assets"):
        shutil.copytree(
            INSTALLER_SOURCE / directory,
            output_dir / directory,
            dirs_exist_ok=True,
        )
    shutil.copy2(
        require_file(INSTALLER_SOURCE / "assets" / "SOURCE.md"),
        require_directory(output_dir / "legal") / "PRODUCT_IMAGE_SOURCE.md",
    )


def assemble_pages(bundle: Path, output_dir: Path) -> Path:
    bundle = bundle.resolve()
    require_file(bundle)
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    extract_release_bundle(bundle, output_dir)
    firmware_name, ota_firmware_name = validate_release(output_dir, bundle)
    firmware_dir = output_dir / "firmware"
    firmware_dir.mkdir()
    shutil.move(output_dir / firmware_name, firmware_dir / firmware_name)
    if ota_firmware_name is not None:
        shutil.move(
            output_dir / ota_firmware_name,
            firmware_dir / ota_firmware_name,
        )
    shutil.copy2(bundle, output_dir / bundle.name)
    overlay_installer_source(output_dir)
    return output_dir


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-bundle", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist" / "installer")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        output_dir = assemble_pages(
            args.release_bundle,
            args.output_dir.resolve(),
        )
    except (FileNotFoundError, ValueError, zipfile.BadZipFile, json.JSONDecodeError) as error:
        print(f"installer Pages assembly failed: {error}", file=sys.stderr)
        return 1
    print(f"installer Pages package: {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
