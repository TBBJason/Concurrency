# syntax=docker/dockerfile:1

# ---------- Builder ----------
FROM ubuntu:22.04 AS backend-builder
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl pkg-config ninja-build \
    ca-certificates zip unzip tar libssl-dev libsqlite3-dev \
  && rm -rf /var/lib/apt/lists/*

# vcpkg
WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
 && /opt/vcpkg/bootstrap-vcpkg.sh
ENV VCPKG_ROOT=/opt/vcpkg
ENV VCPKG_DEFAULT_TRIPLET=x64-linux

# Warm up vcpkg (manifest mode) for better caching
WORKDIR /src/backend
COPY backend/vcpkg.json /src/backend/vcpkg.json
RUN cmake -S /src/backend -B /tmp/vcpkgwarmup \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_FEATURE_FLAGS=manifests \
  && rm -rf /tmp/vcpkgwarmup

# Build + install to a clean prefix
COPY backend/ /src/backend/
RUN cmake -S /src/backend -B /src/backend/build \
      -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_FEATURE_FLAGS=manifests \
      -DCMAKE_BUILD_TYPE=Release \
  && cmake --build /src/backend/build --config Release -- -j$(nproc) \
  && cmake --install /src/backend/build --prefix /opt/app

# ---------- Runtime ----------
FROM ubuntu:22.04 AS runtime
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libsqlite3-0 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=backend-builder /opt/app /app

# Non-root user
RUN useradd -m appuser && chown -R appuser:appuser /app
USER appuser

# Heroku provides $PORT (Expose is optional)
EXPOSE 8080

# This is what Heroku will run
CMD ["/app/bin/server"]
