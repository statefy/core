# core

`core` is a C++ finite automata library with a native CLI and a WebAssembly binding.

## Project layout

```text
.
|-- apps/
|   `-- core-cli/           # native executable entrypoint
|-- bindings/
|   `-- wasm/               # emscripten-facing API surface
|-- include/
|   `-- core/               # public library headers
|-- scripts/                # local and CI build entrypoints
`-- .github/workflows/      # branch-based CI/CD
```

## Build

Build the native CLI with CMake:

```bash
cmake -S . -B build/native -DCORE_BUILD_CLI=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --config Release --target core-cli
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

- Push to `dev`: build artifacts and publish `ghcr.io/statefy/core:dev`
- Push to `main`: build artifacts, publish `ghcr.io/statefy/core:latest`, and create a GitHub release
