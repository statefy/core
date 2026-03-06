#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-$ROOT_DIR/dist/linux}"
CXX="${CXX:-g++}"

mkdir -p "$OUT_DIR"

"$CXX" \
  -std=c++17 \
  -O3 \
  -Wall \
  -Wextra \
  -pedantic \
  "$ROOT_DIR/src/main.cpp" \
  -o "$OUT_DIR/core"
