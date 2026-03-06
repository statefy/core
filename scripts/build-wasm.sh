#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-$ROOT_DIR/dist/wasm}"
EMXX="${EMXX:-em++}"

mkdir -p "$OUT_DIR"

"$EMXX" \
  -std=c++17 \
  -O3 \
  -I "$ROOT_DIR/include" \
  "$ROOT_DIR/bindings/wasm/wasm_api.cpp" \
  -o "$OUT_DIR/core_wasm.js" \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_ES6=1 \
  -s ENVIRONMENT=web,node \
  -s ALLOW_MEMORY_GROWTH=1 \
  --bind
