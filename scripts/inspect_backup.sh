#!/usr/bin/env bash
set -euo pipefail

BIN="${1:?usage: scripts/inspect_backup.sh backup.bin}"

printf 'Size: '
wc -c "$BIN"
printf '\nPotential display or board strings:\n'
strings "$BIN" | rg -i 'zectrix|epd|e[-_ ]?ink|ssd1683|gxepd|waveshare|gpio|display' | head -c 6000
