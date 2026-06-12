FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    git \
    cmake \
    g++ \
    make \
    libssl-dev \
    zlib1g-dev \
    uuid-dev \
    libjsoncpp-dev \
    postgresql-client \
    curl

RUN git clone https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    git submodule update --init && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install && \
    ldconfig

WORKDIR /app

COPY . .

RUN mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc)

CMD ["./build/win-xp-kernel"]