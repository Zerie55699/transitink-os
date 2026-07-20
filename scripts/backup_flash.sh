#!/usr/bin/env bash
set -euo pipefail
umask 077

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${1:-${ESP32_PORT:-}}"
BAUD="${ESP32_BAUD:-460800}"
CHIP="${ESP32_CHIP:-esp32s3}"
PROFILE="${TRANSITINK_BOARD:-zectrix_note4}"
STAMP="$(date +%Y%m%d-%H%M%S)"
OUT="${2:-$ROOT/backups/$PROFILE-$STAMP.bin}"
CHUNK_SIZE_HEX="${ESP32_BACKUP_CHUNK_SIZE:-0x400000}"
FLASH_SIZE_HEX="${ESP32_FLASH_SIZE:-0x1000000}"
FLASH_SIZE=$((FLASH_SIZE_HEX))
CHUNK_SIZE=$((CHUNK_SIZE_HEX))

if [[ -z "$PORT" ]]; then
  printf 'usage: scripts/backup_flash.sh <serial-port> [output.bin]\n' >&2
  printf 'or set ESP32_PORT before running this script\n' >&2
  exit 2
fi

mkdir -p "$(dirname "$OUT")"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/esp32-flash-backup.XXXXXX")"
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

PARTS=()
OFFSET=0
while [ "$OFFSET" -lt "$FLASH_SIZE" ]; do
  REMAINING=$((FLASH_SIZE - OFFSET))
  THIS_SIZE="$CHUNK_SIZE"
  if [ "$THIS_SIZE" -gt "$REMAINING" ]; then
    THIS_SIZE="$REMAINING"
  fi
  PART="$TMP_DIR/part-$(printf '%08x' "$OFFSET").bin"
  PARTS+=("$PART")
  "$ROOT/.venv/bin/esptool" --chip "$CHIP" --port "$PORT" --baud "$BAUD" read-flash "$(printf '0x%x' "$OFFSET")" "$(printf '0x%x' "$THIS_SIZE")" "$PART"
  OFFSET=$((OFFSET + THIS_SIZE))
done

cat "${PARTS[@]}" > "$OUT"
chmod 600 "$OUT"

SIZE="$(wc -c < "$OUT" | tr -d ' ')"
if [ "$SIZE" != "$FLASH_SIZE" ]; then
  printf 'Backup size mismatch: got %s bytes, expected %s\n' "$SIZE" "$FLASH_SIZE" >&2
  exit 1
fi

printf 'Backup written: %s (%s bytes)\n' "$OUT" "$SIZE"
