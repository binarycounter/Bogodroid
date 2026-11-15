#!/usr/bin/env bash
# Build Bogodroid inside the ARM64 container.
#   ./build.sh            Release build  (build-arm64)
#   ./build.sh debug      Debug build    (build-arm64-debug)
#   ./build.sh asan       Release-with-symbols + AddressSanitizer + UBSan (build-arm64-asan)
#   ./build.sh test       Host-side stress tests under ASan/UBSan and TSan (tools/stress, runs natively, no container).
#                         (ThreadSanitizer isn't available for ARM64 in this image's GCC 9, so thread races are checked here.)
# The sanitizer builds are for catching memory errors and data races in the loader on a real run (they are slow and
# need more RAM than a handheld has: use them on a desktop ARM64 machine, or as a build check). The guest game's own
# code isn't instrumented; only the loader, the Java/Android emulation and the libc shims are.
# Set REBUILD_IMAGE=1 to force rebuilding the image (e.g. after Dockerfile changes).
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

IMAGE=${IMAGE:-bogodroid-build}

SAN_FLAGS=""
case "${1:-release}" in
    release) BUILD_DIR=build-arm64;       BUILD_TYPE=Release ;;
    debug)   BUILD_DIR=build-arm64-debug; BUILD_TYPE=Debug ;;
    asan)    BUILD_DIR=build-arm64-asan;  BUILD_TYPE=RelWithDebInfo; SAN_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" ;;
    test)    exec "$(dirname "$(readlink -f "$0")")/tools/stress/run.sh" "${@:2}" ;;
    *) echo "usage: $0 [release|debug|asan|test]" >&2; exit 1 ;;
esac

if [[ -n "${REBUILD_IMAGE:-}" ]] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    docker build --platform linux/arm64 -t "$IMAGE" .
fi

# $(nproc) is evaluated inside the container so it reflects the container's CPUs.
TTY=(); [[ -t 0 && -t 1 ]] && TTY=(-it)
docker run --rm "${TTY[@]}" --platform linux/arm64 \
    -e SAN_FLAGS="$SAN_FLAGS" \
    -v "$PWD":/src -w /src \
    "$IMAGE" \
    bash -c "[ -f $BUILD_DIR/CMakeCache.txt ] || cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE \${SAN_FLAGS:+-DCMAKE_C_FLAGS=\"\$SAN_FLAGS\" -DCMAKE_CXX_FLAGS=\"\$SAN_FLAGS\" -DCMAKE_EXE_LINKER_FLAGS=\"\$SAN_FLAGS\" -DCMAKE_SHARED_LINKER_FLAGS=\"\$SAN_FLAGS\"}; cmake --build $BUILD_DIR -j\$(nproc)"
