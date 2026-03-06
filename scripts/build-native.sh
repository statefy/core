#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="${1:-$ROOT_DIR/dist/linux}"
BUILD_DIR="${2:-$ROOT_DIR/build/native}"
INSTALL_DIR="${3:-$BUILD_DIR/install}"
CONFIG="${CONFIG:-Release}"
BUILD_TESTING="${BUILD_TESTING:-ON}"

cmake_args=(
  -S "$ROOT_DIR"
  -B "$BUILD_DIR"
  -DCORE_BUILD_CLI=ON
  -DBUILD_TESTING="$BUILD_TESTING"
  -DCMAKE_BUILD_TYPE="$CONFIG"
)

if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
  cmake_args+=(-G "$CMAKE_GENERATOR")
fi

mkdir -p "$OUT_DIR"

cmake "${cmake_args[@]}"

cmake --build "$BUILD_DIR" --config "$CONFIG" --target core-cli
cmake --install "$BUILD_DIR" --config "$CONFIG" --prefix "$INSTALL_DIR"

for candidate in \
  "$INSTALL_DIR/bin/core" \
  "$INSTALL_DIR/bin/core.exe"
do
  if [ -f "$candidate" ]; then
    cp "$candidate" "$OUT_DIR/$(basename "$candidate")"
    exit 0
  fi
done

echo "Failed to locate the installed core executable under $INSTALL_DIR/bin" >&2
exit 1
