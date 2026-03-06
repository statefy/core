FROM gcc:14-bookworm AS build

WORKDIR /workspace
COPY src ./src
COPY scripts ./scripts

RUN bash scripts/build-native.sh /out

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends libgcc-s1 libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /out/core /usr/local/bin/core

ENTRYPOINT ["/usr/local/bin/core"]
