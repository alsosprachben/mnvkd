#!/bin/sh
docker build . --build-arg UBUNTU_VERSION=22.04 --build-arg GCC_MAJOR=12 -f Dockerfile.http11 -t vkhttp11:22.04
