# ---------- Backend builder ----------
FROM ubuntu:22.04 AS backend-builder
ARG DEBIAN_FRONTEND=noninteractive

# build deps (add more -dev packages if your CMake needs them)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl ninja-build pkg-config ca-certificates unzip wget \
    libssl-dev libsqlite3-dev && rm -rf /var/lib/apt/lists/*

# install vcpkg (optional; remove toolchain usage if you don't use vcpkg)
WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg
RUN /opt/vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Copy backend source into predictable path
WORKDIR /src/backend
COPY backend/ /src/backend/

# configure, build and install into /opt/app
# using -S/-B style so path expectations are explicit
RUN mkdir -p /src/backend/build && \
    cmake -S /src/backend -B /src/backend/build \
      -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_BUILD_TYPE=Release || ( \
        echo "==== CMake configure failed ====" && \
        test -f /src/backend/build/CMakeFiles/CMakeError.log && sed -n '1,500p' /src/backend/build/CMakeFiles/CMakeError.log || true && \
        false \
      )

RUN cmake --build /src/backend/build --config Release -- -j$(nproc) || ( \
        echo "==== Build failed ====" && false \
      )

# install(TARGETS ...) should place runtime binary under /opt/app (e.g. /opt/app/bin)
RUN cmake --install /src/backend/build --prefix /opt/app

# ---------- Frontend builder ----------
FROM node:18 AS frontend-builder
WORKDIR /app/frontend
COPY frontend/package*.json ./
RUN npm ci --no-audit --no-fund
COPY frontend/ ./
RUN npm run build

# ---------- Final runtime image ----------
FROM ubuntu:22.04 AS runtime
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libssl3 libsqlite3-0 && rm -rf /var/lib/apt/lists/*

# copy installed backend artifacts (from cmake --install)
COPY --from=backend-builder /opt/app /app

# copy frontend static files
COPY --from=frontend-builder /app/frontend/build /app/static

# create non-root user
RUN useradd -m myapp || true
USER myapp

# expose default (Heroku will provide $PORT). Your server must read PORT env var.
EXPOSE  ${PORT:-8080}

# default command - adjust path if your installed binary is different
# Common install location when using install(TARGETS server RUNTIME DESTINATION bin) -> /app/bin/server
CMD ["/app/bin/server"]
