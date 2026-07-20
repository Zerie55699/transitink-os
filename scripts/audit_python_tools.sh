#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# PlatformIO 6.1.19 constrains Starlette to <0.53 for its optional Home web UI.
# TransitInk's build, test, flash, and release commands never start that server.
# Keep these explicit exceptions until PlatformIO supports a fixed Starlette 1.x.
"$ROOT/.venv/bin/pip-audit" --local \
  --ignore-vuln PYSEC-2026-161 \
  --ignore-vuln PYSEC-2026-248 \
  --ignore-vuln PYSEC-2026-249 \
  --ignore-vuln PYSEC-2026-2280 \
  --ignore-vuln PYSEC-2026-2281
