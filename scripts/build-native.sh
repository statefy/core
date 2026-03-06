#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-$ROOT_DIR/dist/linux}"
BUILD_DIR="${2:-$ROOT_DIR/build/native}"
CONFIG="${CONFIG:-Release}"

mkdir -p "$OUT_DIR"

cmake \
  -S "$ROOT_DIR" \
  -B "$BUILD_DIR" \
  -DCORE_BUILD_CLI=ON \
  -DCMAKE_BUILD_TYPE="$CONFIG"

cmake --build "$BUILD_DIR" --config "$CONFIG" --target core-cli
cmake --install "$BUILD_DIR" --config "$CONFIG" --prefix "$BUILD_DIR/install"

for candidate in \
  "$BUILD_DIR/install/bin/core" \
  "$BUILD_DIR/install/bin/core.exe"
do
  if [ -f "$candidate" ]; then
    cp "$candidate" "$OUT_DIR/$(basename "$candidate")"
    exit 0
  fi
done

echo "Failed to locate the installed core executable under $BUILD_DIR/install/bin" >&2
exit 1
