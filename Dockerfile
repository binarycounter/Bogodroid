# ARM64 build environment for Bogodroid.
# Uses an OLD distro on purpose: binaries only run on devices whose glibc is
# at least as new as the one they were built against (ArkOS etc. are old).
FROM --platform=linux/arm64 ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git pkg-config ca-certificates \
        libzip-dev libsdl2-dev libbsd-dev \
        libegl-dev libgles-dev libgl-dev python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Ubuntu 20.04's cmake 3.16 can't spawn make under qemu-user; use a newer one.
RUN pip3 install --no-cache-dir --upgrade pip && pip3 install --no-cache-dir cmake==3.31.6

WORKDIR /src
CMD ["bash"]
