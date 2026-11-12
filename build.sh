#!/usr/bin/env bash
# Build Bogodroid inside the ARM64 container.
#   ./build.sh            Release build  (build-arm64)
#   ./build.sh debug      Debug build    (build-arm64-debug)
# Set REBUILD_IMAGE=1 to force rebuilding the image (e.g. after Dockerfile changes).
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

IMAGE=${IMAGE:-bogodroid-build}

case "${1:-release}" in
    release) BUILD_DIR=build-arm64;       BUILD_TYPE=Release ;;
    debug)   BUILD_DIR=build-arm64-debug; BUILD_TYPE=Debug ;;
    *) echo "usage: $0 [release|debug]" >&2; exit 1 ;;
esac

if [[ -n "${REBUILD_IMAGE:-}" ]] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    docker build --platform linux/arm64 -t "$IMAGE" .
fi

# $(nproc) is evaluated inside the container so it reflects the container's CPUs.
TTY=(); [[ -t 0 && -t 1 ]] && TTY=(-it)
docker run --rm "${TTY[@]}" --platform linux/arm64 \
    -v "$PWD":/src -w /src \
    "$IMAGE" \
    bash -c "[ -f $BUILD_DIR/CMakeCache.txt ] || cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE; cmake --build $BUILD_DIR -j\$(nproc)"
