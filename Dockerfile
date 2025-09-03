# Build stage for backend
FROM ubuntu:22.04 AS backend-builder

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    curl \
    git \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
WORKDIR /opt
RUN git clone https://github.com/Microsoft/vcpkg.git
RUN /opt/vcpkg/bootstrap-vcpkg.sh

# Copy backend code
WORKDIR /app
COPY backend/ .

# Build the project
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake && \
    cmake --build . --config Release

# Build stage for frontend
FROM node:18 AS frontend-builder

WORKDIR /app/frontend
COPY frontend/package*.json ./
RUN npm install
COPY frontend/ ./
RUN npm run build

# Final runtime image
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/*

# Copy backend binary
COPY --from=backend-builder /app/build/server /app/server

# Copy frontend build files
COPY --from=frontend-builder /app/frontend/build /app/static

# Create non-root user for security
RUN useradd -m myapp
USER myapp

# Heroku uses dynamic port binding
EXPOSE $PORT

# Start the server - Heroku will set the PORT environment variable
CMD ["/app/server"]