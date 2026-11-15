#!/usr/bin/env bash
# Build and run the host-side stress tests under AddressSanitizer+UBSan and ThreadSanitizer.
#   tools/stress/run.sh [asan|tsan|plain|arm64 ...]      (default: asan tsan)
# Env: STRESS_SECONDS=N  how long each test hammers (default 2; use 60+ for a soak run)
set -uo pipefail
HERE=$(dirname "$(readlink -f "$0")")
MODES=("$@"); [[ ${#MODES[@]} -eq 0 ]] && MODES=(asan tsan)
rc=0
# arm64: the same tests, built and run natively in the ARM64 container. The x86 runs can't catch alignment faults
# (ARM64 raises SIGBUS on misaligned atomics, x86 doesn't) or other ARM-only differences.
if [[ " ${MODES[*]} " == *" arm64 "* ]]; then
    ROOT=$(dirname "$(dirname "$HERE")")
    echo "==== arm64: building and running in the container"
    docker run --rm --platform linux/arm64 -v "$ROOT":/src:ro -w /tmp "${IMAGE:-bogodroid-build}" bash -c '
        cmake -S /src/tools/stress -B /tmp/sb -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null 2>&1 &&
        cmake --build /tmp/sb -j8 2>&1 | grep -E "error" ;
        rc=0
        for t in shim_pthread alooper_stress jnivm_stress os_stress breadcrumbs_test jnibridge_test; do
            echo "---- arm64: $t"; STRESS_SECONDS=${STRESS_SECONDS:-2} timeout 900 /tmp/sb/$t 2>&1 | tail -30; [ ${PIPESTATUS[0]} -eq 0 ] || rc=1
        done; exit $rc' || rc=1
    MODES=("${MODES[@]/arm64}")
fi
for mode in "${MODES[@]}"; do
    case $mode in
        asan)  FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1" ;;
        tsan)  FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g -O1" ;;
        plain) FLAGS="-g -O1" ;;
        *) echo "unknown mode $mode" >&2; exit 1 ;;
    esac
    B=$HERE/build-$mode
    echo "==== $mode: building"
    cmake -S "$HERE" -B "$B" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_FLAGS="$FLAGS" -DCMAKE_CXX_FLAGS="$FLAGS" \
          -DCMAKE_EXE_LINKER_FLAGS="$FLAGS" >/dev/null || { echo "cmake failed"; rc=1; continue; }
    cmake --build "$B" -j"$(nproc)" 2>&1 | grep -E "error|Error|warning: unused" | head -20
    for t in shim_pthread alooper_stress jnivm_stress os_stress breadcrumbs_test jnibridge_test; do
        [[ -x $B/$t ]] || { echo "$t: not built"; rc=1; continue; }
        echo "---- $mode: $t"
        TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" ASAN_OPTIONS="detect_leaks=1" \
            timeout 600 "$B/$t" 2>&1 | tail -60
        [[ ${PIPESTATUS[0]} -eq 0 ]] || rc=1
    done
done
exit $rc
