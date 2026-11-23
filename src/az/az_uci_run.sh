#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="python3"
if [ -x "$ROOT/.venv-az/bin/python3" ]; then
  PY="$ROOT/.venv-az/bin/python3"
fi
exec "$PY" "$ROOT/src/az/az_uci.py" "$@"
