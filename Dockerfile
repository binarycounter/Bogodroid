# ARM64 build environment for Bogodroid.
# Uses an OLD distro on purpose: binaries only run on devices whose glibc is
# at least as new as the one they were built against (ArkOS etc. are old).
FROM --platform=linux/arm64 ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git pkg-config ca-certificates curl \
        libzip-dev libsdl2-dev libbsd-dev \
        libegl-dev libgles-dev libgl-dev python3-pip \
    && rm -rf /var/lib/apt/lists/*

# mold isn't packaged for Ubuntu 20.04 (first appears in 22.04); install the
# upstream prebuilt aarch64 release instead (it only needs an old glibc).
ARG MOLD_VERSION=2.40.4
RUN curl -fsSL "https://github.com/rui314/mold/releases/download/v${MOLD_VERSION}/mold-${MOLD_VERSION}-aarch64-linux.tar.gz" \
    | tar -xz -C /usr/local --strip-components=1 \
    && mold --version

# Ubuntu 20.04's cmake 3.16 can't spawn make under qemu-user; use a newer one.
RUN pip3 install --no-cache-dir --upgrade pip && pip3 install --no-cache-dir cmake==3.31.6

WORKDIR /src
CMD ["bash"]
