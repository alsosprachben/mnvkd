ARG UBUNTU_VERSION=22.04
FROM ubuntu:${UBUNTU_VERSION} AS vkcc

ENV QEMU_CPU=max
ENV QEMU_EXECVE=1
ENV QEMU_USERSPACE_MMU=1

ARG DEBIAN_FRONTEND=noninteractive
ENV DEBIAN_FRONTEND=${DEBIAN_FRONTEND}

ARG GCC_MAJOR=12
ENV GCC_MAJOR=${GCC_MAJOR}

# Install dependencies
RUN apt-get update && \
	apt-get install -y  \
	gcc-${GCC_MAJOR} \
	g++-${GCC_MAJOR} \
	make \
	curl \
	libssl-dev \
	ninja-build && \
	update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_MAJOR} 100 && \
	update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_MAJOR} 100 && \
	update-alternatives --install /usr/bin/cc  cc  /usr/bin/gcc-${GCC_MAJOR} 100 && \
	update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-${GCC_MAJOR} 100 && \
	apt-get clean && \
	apt-get clean && \
	rm -rf /var/lib/apt/lists/*

FROM vkcc AS vkcmake

WORKDIR "/"

# download cmake release
RUN curl -L --output cmake-3.31.0.tar.gz https://github.com/Kitware/CMake/releases/download/v3.31.0/cmake-3.31.0.tar.gz && \
	tar -xvf cmake-3.31.0.tar.gz && \
	cd cmake-3.31.0 && \
	./configure --prefix=/usr/local --generator=Ninja && ninja && ninja install && \
	cd .. && \
	 rm -rf cmake-3.31.0.tar.gz cmake-3.31.0