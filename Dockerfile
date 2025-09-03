FROM ubuntu:22.04 AS backend-builder
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl ninja-build pkg-config ca-certificates unzip wget zip tar \
    libssl-dev libsqlite3-dev && rm -rf /var/lib/apt/lists/*

WORKDIR /opt
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg
RUN /opt/vcpkg/bootstrap-vcpkg.sh
RUN /opt/vcpkg/vcpkg install boost-thread boost-system boost-filesystem --triplet x64-linux


ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Copy backend source into predictable path
WORKDIR /src/backend
COPY backend/ /src/backend/

# Configure, build and install
RUN mkdir -p /src/backend/build && \
    cmake -S /src/backend -B /src/backend/build \
      -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_BUILD_TYPE=Release
RUN cmake --build /src/backend/build --config Release -- -j$(nproc)
RUN cmake --install /src/backend/build --prefix /opt/app
