FROM gcc:14-bookworm AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY CMakeLists.txt ./
COPY VERSION ./
COPY apps ./apps
COPY cmake ./cmake
COPY include ./include
COPY scripts ./scripts
COPY tests ./tests

RUN BUILD_TESTING=OFF bash scripts/build-native.sh /out /workspace/build/native /workspace/build/native/install

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends libgcc-s1 libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /out/core /usr/local/bin/core

ENTRYPOINT ["/usr/local/bin/core"]
