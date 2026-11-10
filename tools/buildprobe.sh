#!/usr/bin/env bash
# Build the Unity compatibility probe as a PortMaster package:
#   1. debug unityloader         -> tools/portmaster/probe/unityloader
#   2. Unity project -> APK      -> tools/portmaster/probe/gamedata/base.apk (unedited; the loader extracts the
#                                   native libraries on first run, see [paths] apk in probe.toml)
#   3. zip Probe.sh + probe/     -> tools/portmaster/probe.zip
#
#   tools/buildprobe.sh
# Env: UNITY=/path/to/Unity  (default: ~/Unity/Hub/Editor/<ProjectVersion>/Editor/Unity)
#      SKIP_LOADER=1 / SKIP_UNITY=1  to skip step 1 / the Unity build (reuse the last APK)
# Close the Unity editor first: it holds a lock on the project.
set -euo pipefail

if [[ $EUID -eq 0 ]]; then
    echo "Don't run this as root/sudo: Unity's licence is activated for your own user (root gets 'No valid Unity" >&2
    echo "Editor license found') and root-owned build files get in your way later." >&2
    echo "If docker needs sudo: sudo usermod -aG docker \$USER, then log out and back in." >&2
    exit 1
fi

TOOLS=$(dirname "$(readlink -f "$0")")
ROOT=$(dirname "$TOOLS")
PROJECT=$TOOLS/UnityProbe
PM=$TOOLS/portmaster
PROBE=$PM/probe
GAMEDATA=$PROBE/gamedata
LIBCXX=lib/arm64-v8a/libc++_shared.so   # not part of the APK, but the loader needs it: taken from Unity's bundled NDK
APK=$PROJECT/Build/probe.apk

if [[ -z "${SKIP_LOADER:-}" ]]; then
    echo "==> Building unityloader (debug)"
    "$ROOT/build.sh" debug
    install -m 755 "$ROOT/build-arm64-debug/unityloader" "$PROBE/unityloader"
fi

VERSION=$(sed -n 's/^m_EditorVersion: *//p' "$PROJECT/ProjectSettings/ProjectVersion.txt")
UNITY=${UNITY:-$HOME/Unity/Hub/Editor/$VERSION/Editor/Unity}
[[ -x "$UNITY" ]] || { echo "Unity $VERSION not found at $UNITY (set UNITY=)" >&2; exit 1; }
NDK=$(dirname "$UNITY")/Data/PlaybackEngines/AndroidPlayer/NDK/toolchains/llvm/prebuilt/linux-x86_64

if [[ -z "${SKIP_UNITY:-}" ]]; then
    # The probe's native test plugin (libprobeplugin.so + its dependency libprobedep.so), built with Unity's NDK.
    echo "==> Building the native test plugin"
    PLUGINS=$PROJECT/Assets/Plugins/Android
    mkdir -p "$PLUGINS"
    CC=$NDK/bin/aarch64-linux-android26-clang
    "$CC" -shared -fPIC -O2 -o "$PLUGINS/libprobedep.so" "$PROJECT/NativePlugin/probedep.c"
    "$CC" -shared -fPIC -O2 -o "$PLUGINS/libprobeplugin.so" "$PROJECT/NativePlugin/probeplugin.c" -L"$PLUGINS" -lprobedep -llog

    echo "==> Building APK with Unity $VERSION"
    rm -f "$APK"
    "$UNITY" -batchmode -nographics -quit -projectPath "$PROJECT" \
        -buildTarget Android -executeMethod ProbeBuild.BuildAndroid \
        -probeOut "$APK" -logFile -
    [[ -f "$APK" ]] || { echo "Unity did not produce $APK" >&2; exit 1; }
fi
[[ -f "$APK" ]] || { echo "$APK missing (run without SKIP_UNITY)" >&2; exit 1; }

# The unedited APK is the package: Unity reads its data from it (as on Android, which also makes jar:file:// URLs
# work) and the loader extracts the native libraries from it on first run.
echo "==> Installing the APK as $GAMEDATA/base.apk"
unzip -l "$APK" 'lib/arm64-v8a/libunity.so' >/dev/null || { echo "APK has no lib/arm64-v8a/libunity.so" >&2; exit 1; }
rm -rf "$GAMEDATA/assets" "$GAMEDATA/lib"
mkdir -p "$GAMEDATA/lib/arm64-v8a"
cp "$APK" "$GAMEDATA/base.apk"

# libc++_shared.so isn't in a Unity APK; use the one from the editor's own NDK (stripped to save space).
SRC=$NDK/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so
[[ -f "$SRC" ]] || { echo "libc++_shared.so not found in the Unity NDK: $SRC" >&2; exit 1; }
cp "$SRC" "$GAMEDATA/$LIBCXX"
"$NDK/bin/llvm-strip" --strip-unneeded "$GAMEDATA/$LIBCXX"

# Unity reads time zones from an Android-format tzdata file: generate it from the host's zoneinfo.
python3 "$TOOLS/make_android_tzdata.py" "$GAMEDATA/usr/share/zoneinfo/tzdata"

echo "==> Creating probe.zip"
rm -f "$PM/probe.zip"
(cd "$PM" && zip -qr probe.zip Probe.sh probe)
echo "Done: $PM/probe.zip ($(du -h "$PM/probe.zip" | cut -f1))"
