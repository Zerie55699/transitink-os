#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${1:?usage: scripts/restore_flash.sh backup.bin [port]}"
PORT="${2:-${ESP32_PORT:-}}"
BAUD="${ESP32_BAUD:-460800}"
CHIP="${ESP32_CHIP:-esp32s3}"
FLASH_SIZE_HEX="${ESP32_FLASH_SIZE:-0x1000000}"
FLASH_SIZE=$((FLASH_SIZE_HEX))

if [[ -z "$PORT" ]]; then
  printf 'usage: scripts/restore_flash.sh <backup.bin> <serial-port>\n' >&2
  printf 'or set ESP32_PORT before running this script\n' >&2
  exit 2
fi

SIZE="$(wc -c < "$BIN" | tr -d ' ')"
if [ "$SIZE" != "$FLASH_SIZE" ]; then
  printf 'Refusing restore: %s is %s bytes, expected %s\n' "$BIN" "$SIZE" "$FLASH_SIZE" >&2
  exit 1
fi

"$ROOT/.venv/bin/esptool" --chip "$CHIP" --port "$PORT" --baud "$BAUD" write-flash 0 "$BIN"
