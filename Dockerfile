FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Build dependencies
RUN apt update && apt install -y \
    git cmake g++ make pkg-config \
    libssl-dev zlib1g-dev uuid-dev \
    libjsoncpp-dev libmariadb-dev \
    curl ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Debug: check MySQL headers
RUN find /usr/include -name "mysql.h" 2>/dev/null; echo "---"; ls /usr/include/mysql/ 2>/dev/null || echo "no mysql dir"; dpkg -L libmariadb-dev 2>/dev/null | grep -i "mysql.h" || echo "dpkg not available"

# Header-only: jwt-cpp + picojson
RUN curl -fsSL https://github.com/Thalhammer/jwt-cpp/archive/refs/tags/v0.7.0.tar.gz \
    | tar xz -C /tmp && \
    cp -r /tmp/jwt-cpp-0.7.0/include/jwt-cpp /usr/include/ && \
    rm -rf /tmp/jwt-cpp-0.7.0

RUN curl -fsSL https://github.com/kazuho/picojson/archive/refs/tags/v1.3.0.tar.gz \
    | tar xz -C /tmp && \
    mkdir -p /usr/include/picojson && \
    cp /tmp/picojson-1.3.0/picojson.h /usr/include/picojson/ && \
    rm -rf /tmp/picojson-1.3.0

# Build drogon
RUN git clone --depth 1 https://github.com/drogonframework/drogon.git /tmp/drogon && \
    cd /tmp/drogon && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    rm -rf /tmp/drogon

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# ---- Runtime stage ----
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    libmariadb3 ca-certificates curl && \
    rm -rf /var/lib/apt/lists/*

# Copy built binary and config (shared libs come from builder)
COPY --from=builder /app/build/win-xp-kernel /app/win-xp-kernel
COPY --from=builder /app/views /app/views
COPY --from=builder /app/config.json /app/config.json
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/lib /usr/lib
RUN ldconfig

# CA cert (Render mounts secrets at /etc/secrets/)
RUN mkdir -p /etc/secrets
COPY tidb_ca.pem /etc/secrets/tidb_ca.pem

WORKDIR /app
EXPOSE 5555

CMD ["./win-xp-kernel"]
