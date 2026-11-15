#!/usr/bin/env bash
# Run the real unityloader with the Unity probe (or any APK) in a container for a while, and report whether it
# survived. The probe's soak mode (tools/UnityProbe/Assets/ProbeSoak.cs) keeps it busy for the requested time.
#
#   tools/soak.sh [seconds] [loader] [apk]
#     seconds  how long the game should keep running after its start-up (default 600)
#     loader   build to test: release (default) | asan   -> build-arm64[-asan]/unityloader
#     apk      the game (default tools/UnityProbe/Build/probe.apk). Needs the soak mode, i.e. a probe built after
#              ProbeSoak.cs was added (tools/buildprobe.sh). Other APKs just run until they quit on their own.
#
# Needs: the build image (./build.sh builds it), the loader built by ./build.sh [asan].
# Output goes to tools/soak/work-<loader>/: run.txt is the loader's log, result.txt the verdict.
# Exit status: 0 = survived with no crash report, 1 = crashed / died early / sanitizer report.
#
# (BOGO_SDL_ATTRS_FIRST: under X11 SDL would otherwise use GLX, but the loader's EGL emulation needs EGL, which is
# what the handhelds' KMS/DRM driver uses; see sdl_ensure_window.)
#
# Caveats: on an x86 host the container runs under qemu, so everything is much slower than on a handheld (the game
# may manage only a few frames per second, and start-up can take many minutes); that still exercises the same
# threads and callbacks but at a lower rate. The sanitizer builds are slower still: use a long run.
set -uo pipefail
ROOT=$(dirname "$(dirname "$(readlink -f "$0")")")
SECONDS_TO_RUN=${1:-600}
LOADER=${2:-release}
APK=${3:-$ROOT/tools/UnityProbe/Build/probe.apk}
case $LOADER in
    release) BIN=$ROOT/build-arm64/unityloader ;;
    asan)    BIN=$ROOT/build-arm64-asan/unityloader ;;
    *) echo "loader must be release or asan" >&2; exit 1 ;;
esac
[[ -x $BIN ]] || { echo "$BIN not found: build it first (./build.sh ${LOADER/release/})" >&2; exit 1; }
[[ -f $APK ]] || { echo "APK $APK not found" >&2; exit 1; }

WORK=$ROOT/tools/soak/work-$LOADER
mkdir -p "$WORK/gamedata" "$WORK/support_files" "$WORK/conf" "$WORK/cache"
cp "$BIN" "$WORK/unityloader"
# A copy (hard link when possible), not a symlink: a link to a host path doesn't resolve inside the container.
rm -f "$WORK/gamedata/base.apk"
ln -f "$APK" "$WORK/gamedata/base.apk" 2>/dev/null || cp "$APK" "$WORK/gamedata/base.apk"
# libc++_shared.so isn't part of the APK; the loader needs it next to the extracted libraries (see buildprobe.sh)
if [[ -f $ROOT/tools/portmaster/probe/gamedata/lib/arm64-v8a/libc++_shared.so ]]; then
    mkdir -p "$WORK/gamedata/lib/arm64-v8a"
    cp -n "$ROOT/tools/portmaster/probe/gamedata/lib/arm64-v8a/libc++_shared.so" "$WORK/gamedata/lib/arm64-v8a/"
fi
# A tiny window: software OpenGL under qemu renders at a fraction of a frame per second at 1280x720
cat > "$WORK/game.toml" <<TOML
[device]
displayWidth = ${SOAK_WIDTH:-320}
displayHeight = ${SOAK_HEIGHT:-180}
TOML
echo 0-3 > "$WORK/support_files/cpu_present.txt"
echo 0-3 > "$WORK/support_files/cpu_possible.txt"
rm -f "$WORK/run.txt" "$WORK/result.txt"

if ! docker image inspect bogodroid-soak >/dev/null 2>&1; then
    docker build --platform linux/arm64 -t bogodroid-soak "$ROOT/tools/soak" || exit 1
fi

# The game gets the soak time on top of its start-up; the outer timeout is only a safety net.
LIMIT=$(( SECONDS_TO_RUN * 4 + 1800 ))
echo "==> $LOADER loader, $(basename "$APK"), soak ${SECONDS_TO_RUN}s (safety limit ${LIMIT}s); log: $WORK/run.txt"
docker run --rm --platform linux/arm64 -v "$WORK":/work -w /work \
    -e PROBE_SOAK_SECONDS="$SECONDS_TO_RUN" -e BOGODROID_VERBOSE= -e LIBGL_ALWAYS_SOFTWARE=1 -e SDL_AUDIODRIVER=dummy -e BOGO_SDL_ATTRS_FIRST=1 \
    -e XDG_DATA_HOME=/work/conf -e XDG_CONFIG_HOME=/work/conf \
    -e ASAN_OPTIONS=detect_leaks=0:abort_on_error=0 -e TSAN_OPTIONS=halt_on_error=0 \
    bogodroid-soak \
    bash -c "timeout $LIMIT xvfb-run -a -s '-screen 0 1280x720x24' /work/unityloader > /work/run.txt 2>&1; echo \$? > /work/exitcode"

CODE=$(cat "$WORK/exitcode" 2>/dev/null || echo "?")
{
    echo "exit code: $CODE"
    echo "loader: $(grep -m1 '^unityloader' "$WORK/run.txt")"
    echo "last heartbeat: $(grep '\[SOAK\] t=' "$WORK/run.txt" | tail -1)"
    echo "soak finished: $(grep -c '\[SOAK\] DONE' "$WORK/run.txt")"
    echo "crash reports: $(grep -c 'unityloader crash report\|LOG\[CRASH\]' "$WORK/run.txt")"
    echo "sanitizer reports: $(grep -c 'ERROR: AddressSanitizer\|WARNING: ThreadSanitizer\|runtime error:' "$WORK/run.txt")"
} | tee "$WORK/result.txt"

if grep -q 'unityloader crash report\|LOG\[CRASH\]\|ERROR: AddressSanitizer\|WARNING: ThreadSanitizer' "$WORK/run.txt"; then
    echo "VERDICT: FAILED (crash or sanitizer report; see $WORK/run.txt)"; exit 1
fi
if ! grep -q '\[SOAK\] DONE' "$WORK/run.txt"; then
    echo "VERDICT: INCOMPLETE (the soak never finished: loader exited early, or the APK has no soak mode)"; exit 1
fi
echo "VERDICT: survived"
