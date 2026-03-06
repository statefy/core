# core

`core` is a C++ finite automata library with a native CLI and a WebAssembly binding.

## Project layout

```text
.
|-- apps/
|   `-- core-cli/           # native executable entrypoint
|-- bindings/
|   `-- wasm/               # emscripten-facing API surface
|-- cmake/                  # package config templates
|-- include/
|   `-- core/               # public library headers
|-- tests/                  # native smoke/integration tests
|-- scripts/                # local and CI build entrypoints
|-- VERSION                 # release version source of truth
`-- .github/workflows/      # branch-based CI/CD
```

## Build

Build the native CLI with CMake:

```bash
cmake -S . -B build/native -DCORE_BUILD_CLI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release --target core-cli
ctest --test-dir build/native --build-config Release --output-on-failure
```

Or use the repository script:

```bash
bash scripts/build-native.sh
```

Build the WebAssembly bundle with Emscripten:

```bash
bash scripts/build-wasm.sh
```

## CI/CD

- Pull requests: build native binaries on Linux, macOS, and Windows; build WASM; run native tests and smoke checks
- Push to `dev`: build multi-OS release artifacts, build WASM, publish `ghcr.io/statefy/core:dev`, and attach container provenance/SBOM
- Push to `main`: publish versioned multi-OS artifacts, publish `ghcr.io/statefy/core:latest` and `ghcr.io/statefy/core:vX.Y.Z`, and create a GitHub release from `VERSION`
