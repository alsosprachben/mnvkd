#!/bin/sh
docker build . --build-arg UBUNTU_VERSION=22.04 --build-arg GCC_MAJOR=12 -f Dockerfile.vcpkg -t vkvcpkg:22.04
