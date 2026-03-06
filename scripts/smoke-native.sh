#!/usr/bin/env bash
set -euo pipefail

BINARY_PATH="${1:?usage: smoke-native.sh <binary-path>}"

if [[ ! -f "$BINARY_PATH" ]]; then
  echo "Native smoke check failed: missing binary at $BINARY_PATH" >&2
  exit 1
fi

output="$("$BINARY_PATH")"

grep -F '== DFA<char> (even number of 1s) ==' <<<"$output" >/dev/null
grep -F 'serialized snapshot (bytes=' <<<"$output" >/dev/null
grep -F 'nfa->dfa states=' <<<"$output" >/dev/null
grep -F 'token DFA snapshot bytes=' <<<"$output" >/dev/null

echo "native CLI smoke check passed for $BINARY_PATH"
