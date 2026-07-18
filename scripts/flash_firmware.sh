#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${1:-${ESP32_PORT:-}}"
ENVIRONMENT="${PLATFORMIO_ENV:-zectrix_note4}"

if [[ -z "$PORT" ]]; then
  printf 'usage: scripts/flash_firmware.sh <serial-port>\n' >&2
  printf 'or set ESP32_PORT before running this script\n' >&2
  exit 2
fi

cd "$ROOT"
PLATFORMIO_CORE_DIR="$ROOT/.platformio" .venv/bin/platformio run -e "$ENVIRONMENT" -t upload --upload-port "$PORT"
