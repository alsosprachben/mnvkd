# syntax=docker/dockerfile:1
ARG VARIANT="22.04"
FROM mcr.microsoft.com/devcontainers/base:ubuntu-${VARIANT}

RUN apt-get update \
    && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y --no-install-recommends \
        build-essential \
        clang \
        lld \
        cmake \
        ninja-build \
        pkg-config \
        bmake \
        sqlite3 \
        libsqlite3-dev \
        gdb \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

ENV CC=clang
ENV CXX=clang++

