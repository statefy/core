#!/usr/bin/env bash
set -euo pipefail

JS_PATH="${1:?usage: smoke-wasm.sh <js-path> [wasm-path]}"
WASM_PATH="${2:-${JS_PATH%.js}.wasm}"

if [[ ! -f "$JS_PATH" ]]; then
  echo "WASM smoke check failed: missing JS bundle at $JS_PATH" >&2
  exit 1
fi

if [[ ! -f "$WASM_PATH" ]]; then
  echo "WASM smoke check failed: missing WASM binary at $WASM_PATH" >&2
  exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

cp "$JS_PATH" "$WORK_DIR/core_wasm.mjs"
cp "$WASM_PATH" "$WORK_DIR/core_wasm.wasm"

cat > "$WORK_DIR/run.mjs" <<'EOF'
import createCore from './core_wasm.mjs';

const module = await createCore();

const snapshot = {
  version: 1,
  nodes: [
    { id: 0, start: true, accepting: false },
    { id: 1, start: false, accepting: true }
  ],
  edges: [
    { from: 0, to: 1, epsilon: false, symbols: ['a'] }
  ]
};

const dfaResult = JSON.parse(
  module.dfaSimulateJson(JSON.stringify(snapshot), JSON.stringify(['a']))
);

if (!dfaResult.ok || !dfaResult.accepted || !dfaResult.complete || dfaResult.end_state !== 1) {
  throw new Error(`Unexpected DFA result: ${JSON.stringify(dfaResult)}`);
}

const nfaResult = JSON.parse(
  module.nfaSimulateJson(JSON.stringify(snapshot), JSON.stringify(['a']))
);

if (!nfaResult.ok || !nfaResult.accepted || !Array.isArray(nfaResult.end_states) || !nfaResult.end_states.includes(1)) {
  throw new Error(`Unexpected NFA result: ${JSON.stringify(nfaResult)}`);
}

console.log('WASM smoke check passed');
EOF

node "$WORK_DIR/run.mjs"
