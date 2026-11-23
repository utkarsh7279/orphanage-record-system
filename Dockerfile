FROM debian:bookworm-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git libsqlite3-dev libssl-dev ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libsqlite3-0 libssl3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/build/orphanage_system ./build/orphanage_system
COPY --from=build /app/database ./database
COPY --from=build /app/web ./web

RUN mkdir -p /data

CMD ["/bin/sh", "-c", "exec env ORPHANAGE_PORT=${PORT:-8080} ORPHANAGE_DB=${ORPHANAGE_DB:-/tmp/orphanage.db} ORPHANAGE_WEB_DIR=web ./build/orphanage_system"]
