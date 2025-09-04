# syntax=docker/dockerfile:1

# ---------- Backend builder ----------
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

# Copy backend source
WORKDIR /src/backend
COPY backend/ /src/backend/

# Build backend and install into /opt/app
RUN cmake -S /src/backend -B /src/backend/build \
      -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_FEATURE_FLAGS=manifests \
      -DCMAKE_BUILD_TYPE=Release \
  && cmake --build /src/backend/build --config Release -- -j$(nproc) \
  && cmake --install /src/backend/build --prefix /opt/app

# ---------- Frontend builder ----------
FROM node:18 AS frontend-builder
WORKDIR /app/frontend
COPY frontend/package*.json ./
RUN npm ci --no-audit --no-fund
COPY frontend/ ./
# adjust to your frontend build command (npm run build is common)
RUN npm run build

# ---------- Runtime ----------
FROM ubuntu:22.04 AS runtime
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libsqlite3-0 && rm -rf /var/lib/apt/lists/*

# Copy backend installed artifacts to /app
WORKDIR /app
COPY --from=backend-builder /opt/app /app

# Copy frontend build output into /app/static
# (adjust path if your frontend build outputs to a different folder)
COPY --from=frontend-builder /app/frontend/dist /app/static

# Non-root user
RUN useradd -m appuser && chown -R appuser:appuser /app
USER appuser

EXPOSE 8080
CMD ["/app/bin/server"]
