#!/usr/bin/env bash
set -euo pipefail

PLATFORM="${1:?usage: package-native-artifacts.sh <platform> <binary-dir> <install-dir> <output-dir>}"
BINARY_DIR="${2:?usage: package-native-artifacts.sh <platform> <binary-dir> <install-dir> <output-dir>}"
INSTALL_DIR="${3:?usage: package-native-artifacts.sh <platform> <binary-dir> <install-dir> <output-dir>}"
OUTPUT_DIR="${4:?usage: package-native-artifacts.sh <platform> <binary-dir> <install-dir> <output-dir>}"

mkdir -p "$OUTPUT_DIR"

binary_path=""
for candidate in \
  "$BINARY_DIR/core" \
  "$BINARY_DIR/core.exe"
do
  if [[ -f "$candidate" ]]; then
    binary_path="$candidate"
    break
  fi
done

if [[ -z "$binary_path" ]]; then
  echo "Packaging failed: no core binary found in $BINARY_DIR" >&2
  exit 1
fi

if [[ ! -d "$INSTALL_DIR" ]]; then
  echo "Packaging failed: install tree not found at $INSTALL_DIR" >&2
  exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

CLI_STAGE="$WORK_DIR/core-$PLATFORM-cli"
INSTALL_STAGE="$WORK_DIR/core-$PLATFORM-install"

mkdir -p "$CLI_STAGE"
cp "$binary_path" "$CLI_STAGE/"
cp -R "$INSTALL_DIR" "$INSTALL_STAGE"

tar -C "$WORK_DIR" -czf "$OUTPUT_DIR/core-$PLATFORM-cli.tar.gz" "$(basename "$CLI_STAGE")"
tar -C "$WORK_DIR" -czf "$OUTPUT_DIR/core-$PLATFORM-install.tar.gz" "$(basename "$INSTALL_STAGE")"
