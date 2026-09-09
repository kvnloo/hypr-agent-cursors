#!/usr/bin/env bash
# nm-only check: required PLUGIN_*_FUNC_STR exports exist (no dlopen).
set -euo pipefail
SO="${1:?usage: $0 path/to.so}"
OUT="${2:-/dev/stdout}"
{
  echo "nm -D --defined-only $SO"
  nm -D --defined-only "$SO"
  echo
  missing=0
  for s in pluginAPIVersion pluginInit pluginExit; do
    if nm -D --defined-only "$SO" | grep -qE " T ${s}\$"; then
      echo "PASS: export $s"
    else
      echo "FAIL: missing export $s"
      missing=1
    fi
  done
  exit $missing
} | tee "$OUT"
