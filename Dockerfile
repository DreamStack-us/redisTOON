# Build stage
FROM redis:7-alpine AS builder

# Install build dependencies
RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    curl

# Set working directory
WORKDIR /build

# Copy source code
COPY src/ ./src/
COPY deps/ ./deps/
COPY CMakeLists.txt ./

# Build the module
RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make

# Runtime stage
FROM redis:7-alpine

# Install Python for the client library
RUN apk add --no-cache python3 py3-pip

# Copy the compiled module
COPY --from=builder /build/build/redistoon.so /usr/local/lib/redis/modules/

# Copy Python client library
COPY redistoon.py /usr/local/lib/python3.11/site-packages/
COPY requirements.txt /tmp/

# Install Python dependencies
RUN pip3 install --no-cache-dir -r /tmp/requirements.txt && \
    rm /tmp/requirements.txt

# Create Redis data directory
RUN mkdir -p /data

# Copy example configuration
COPY docker/redis.conf /usr/local/etc/redis/redis.conf

# Expose Redis port
EXPOSE 6379

# Set working directory
WORKDIR /data

# Start Redis with redisTOON module
CMD ["redis-server", "/usr/local/etc/redis/redis.conf", "--loadmodule", "/usr/local/lib/redis/modules/redistoon.so"]
