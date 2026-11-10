# Bogodroid NEO — Unity 7.0 fork

This is a fork of [Bogodroid NEO](https://github.com/binarycounter/Bogodroid/tree/neo) that adds support for **Unity 7.0 (7000.x)** games.

It was done to get [Kye](https://playkye.com) running on ARM64 Linux handhelds such as the **R36S**. The [Kye PortMaster zip](https://playkye.com/dl/?type=portmaster) is a complete, working example of how the `unityloader` binary, its config file and the game files fit together. It's worth downloading to see a real port.

Bogodroid is an experimental wrapper environment for the native libraries of ARM Android apps, so that those libraries (or whole apps and games) can run on ARM Linux handhelds. The focus is the Unity engine. It is heavily based on [JohnnyOnFlame/gmloader-next](https://github.com/JohnnyonFlame/gmloader-next) and [ChristopherHX/libjnivm](https://github.com/ChristopherHX/libjnivm).

The idea is to:
- load the libraries and patch them at runtime
- wrap, bridge or stub the symbols they need from Android's system libraries
- provide a JNI interface to placeholder "Java" classes
- provide implementations and stubs for common Java and Android classes
- wrap audio, GLES and input onto host interfaces (SDL2, EGL)

It won't run every app out of the box: it's a framework and starting point for porting projects.

# What the Unity build needs

Build the game for Android with these Player Settings:

| Setting | Value |
|---|---|
| Scripting Backend | **IL2CPP** |
| Target Architectures | **ARM64** only |
| Graphics APIs | **OpenGLES3** only (untick *Auto Graphics API*; Vulkan isn't supported) |
| Application Entry Point | **Activity** (GameActivity isn't supported) |
| Build output | an **APK** (not an App Bundle / `.aab`) |
| Render pipeline | URP works. The Built-in pipeline also works, but in Unity 7 it is only a fallback that is off by default. |

Managed code stripping, Linear/Gamma colour space and Development builds all work. Unity 7.0 support was developed against **7000.0.0a16**; older Unity versions should keep working as before (see *Ports in progress*).

# Building

The loader is built inside an ARM64 Docker container (`Dockerfile`, Ubuntu 20.04). The old base distro is deliberate: binaries only run on devices whose glibc is at least as new as the one they were built with, and handheld firmwares such as ArkOS are old. On an x86 PC Docker runs the container through QEMU, which is slow but needs nothing else installed.

```bash
git clone --recursive https://github.com/GeoffWoollams/Bogodroid.git
cd Bogodroid
```

```bash
./build.sh
```

That's a **Release** build: `build-arm64/unityloader`.

```bash
./build.sh debug
```

That's a **Debug** build, `build-arm64-debug/unityloader`. It's slower and logs every JNI call, which is useful when porting.

- The image is built on first use. Set `REBUILD_IMAGE=1` to rebuild it, for example after changing the `Dockerfile`.
- Don't run `build.sh` with `sudo`. Add yourself to the `docker` group instead (`sudo usermod -aG docker $USER`, then log out and back in), otherwise the build files end up owned by root.
- Another project can be built with `-DPROJ=<name>` (the folder name under `projects/`) by running CMake in the container yourself. `build.sh` builds `unityloader`.

# Running a Unity game

A port folder looks like this (the probe in `tools/portmaster/` and the Kye zip are both laid out this way):

```
mygame/
  unityloader
  mygame.toml
  gamedata/
    base.apk                           # the game's APK, unedited
    lib/arm64-v8a/libc++_shared.so     # not in the APK: copy it from the Unity NDK (see tools/buildprobe.sh)
    usr/share/zoneinfo/tzdata          # time zones for Unity: python3 tools/make_android_tzdata.py <this path>
```

```toml
[paths]
game_files="./gamedata/"
support_files="./gamedata/"
apk="./base.apk"                 # relative to game_files
android_package_code="./"
android_external_files="../conf/"
android_files="../conf/"         # PlayerPrefs, saves, Application.persistentDataPath
android_cache="../cache/"
android_obb_dirs=["./gamedata/"]
android_source_dirs=["./gamedata/"]

[package]
packageName="com.company.game"
versionName="1.0.0"
mainIntentBundle = { unity = "" }

[device]
splash = "splash.bmp"
```

```bash
./unityloader mygame.toml
```

With `[paths] apk`, the loader gives Unity the real APK as its package and Unity reads its data straight from it, as it does on Android. That is also what makes `jar:file://…!/assets/…` URLs work: StreamingAssets, Addressables/AssetBundles and the Unity Services config. On the first run (and whenever the APK changes) the loader extracts the native libraries and any managed code into `gamedata/`. Without `apk`, the APK's `assets/` and `lib/` folders can be extracted into `game_files` instead, as older versions required, but `jar:` URLs won't work then.

Native plugins (`.so` files the game ships besides Unity's own) are loaded when the game first asks for them, as on Android: through `dlopen` (C# P/Invoke, native code) or Java's `System.loadLibrary`/`System.load`. The loader finds them in `gamedata/lib/arm64-v8a/`, loads the libraries they depend on that the game ships first, runs their constructors, and calls `JNI_OnLoad` when they're loaded from Java. Dependencies the game doesn't ship are Android system libraries (`liblog`, `libandroid`, `libGLESv2`, ...), whose functions come from the loader's own implementations. A plugin that must exist before Unity starts can be listed in `[libraries] preload`. A plugin can still need more work: the log shows `Missing: <symbol>` for Android functions the loader doesn't provide yet (calling one aborts), and SDKs that ship a Java half need stubs for those Java classes.

*Most loaders need `/proc` to be mounted (it isn't in a plain chroot: run `mount -t proc none /proc`).*

# Configuration options

The config file is plain TOML. Besides `[paths]` and `[package]` (above, and `configs/unity.toml`), these optional keys are supported. Everything here is off or unchanged by default.

```toml
[device]
displayWidth = 640        # With displayWidth AND displayHeight set, a window of that size is created.
displayHeight = 480       # If both are left out, the real screen size is detected and the window is fullscreen.
fullscreen = false        # Force fullscreen on/off, whatever the above says.
swapInterval = 1          # Force the swap interval (0 = no vsync). Default: whatever the game asks for.
splash = "splash.bmp"     # BMP shown while the game loads (path relative to where the loader is started).
pause_on_focus_loss = false  # Pause the game while the window is not focused.
quit_grace_ms = 300      # After the game asks to quit, wait this long, then exit.
features = ["android.hardware.gamepad"]  # Android system features the game is told exist (none by default).

[input]
controller = true
swap_ab = true            # Swap A<->B (default true: Nintendo-style labels). false = Xbox layout.
swap_xy = true            # Swap X<->Y (default true).
back_button = "select"    # What the pad's Back/Select button sends: "select", "back" (Android Back key) or "both".
escape_as_back = false    # Also send Android's Back key when Escape is pressed.
touch = "auto"           # Real multi-touch from the touchscreen: true, false, or "auto" (on when SDL finds a touch device).

[audio]
queue_bytes = 8192        # Audio data kept queued before it runs dry. Raise it if sound crackles.

[libraries]
preload = ["libfoo.so"]   # Native plugins to load before Unity starts (normally they load when the game asks for them).
```

Game preferences written through Android's `SharedPreferences` (what Unity's `PlayerPrefs` uses) are saved under `<android_files>/shared_prefs/`.

Environment variables:
- `BOGODROID_VERBOSE=1` turns on the chatty diagnostics (dlopen/dlsym traces, JNI type checks, GL extensions, ...)
- `BOGODROID_FPS=1` prints just the FPS counter
- `BOGODROID_NO_MMAP=1` makes the loader read and copy libraries into memory instead of mapping them from the file (slower; only as a workaround if mapping misbehaves on some device)

# Unity 7.0 support

Tested on an R36S (Rockchip RK3326, Mali-G31, ArkOS) with Unity 7000.0.0a16 IL2CPP builds: Kye, and the compatibility probe described below.

**Working**
- Startup, the main loop, pause/resume, and quitting. `Application.Quit()` exits within a fraction of a second.
- Rendering through OpenGL ES 3.2. The probe has checked:
  - URP with post-processing, HDR, MSAA, soft shadows, transparency, particles, render textures, reflection probes and SSAO.
  - The Built-in pipeline with Post Processing v2, including Ambient Occlusion.
  - uGUI, and fonts through FreeType.
- Audio: FMOD output through its Java `AudioTrack` path, backed by SDL. FMOD's OpenSLES attempt fails first and logs a harmless warning.
- Input: the legacy Input Manager and the Input System, gamepads (with hotplug), keyboard and multi-touch.
- Files and data:
  - `persistentDataPath` and PlayerPrefs, which persist between runs.
  - StreamingAssets, Addressables, AssetBundles and Localization, all read from the APK.
- Platform services:
  - Culture and ICU, using the host's ICU.
  - Time zones, through `persist.sys.timezone` and the generated `tzdata`.
  - Threads, loopback sockets and Burst-compiled code.
  - Native plugins, loaded on demand with their dependencies (see *Running a Unity game*).
- Crypto:
  - `MessageDigest` (MD5, SHA-1, SHA-2) and `SecureRandom`.
  - AES `Cipher` (ECB, CBC and CTR, PKCS5/7 or no padding).
  - HMAC `Mac`.
- Android framework pieces Unity asks about:
  - `Build`, `UnityPlayer.currentActivity`, `Context` folders, `PackageManager`, `Configuration` and `Locale`.
  - Battery level and charging state, read from `/sys/class/power_supply`.
- Mobile Notifications package: initializes, registers channels, and schedules or cancels notifications. They are never displayed.

**Not implemented, or deliberately reporting "none"**
- **Networking beyond plain sockets is untested.** My R36S has no network, so HTTP/HTTPS through Unity has never succeeded here. TLS through Java would also need `KeyAgreement`, certificates and AES-GCM, which aren't implemented.
- **Unity Gaming Services and In-App Purchasing.** These need a network, a linked Unity Cloud project and (for IAP) Google Play Billing. The device reports itself as offline, so they fail the way they would on a phone with no connection, and games should already handle that. Google Play Billing is stubbed: connecting answers `BILLING_UNAVAILABLE`, as on a phone without the Play Store, so IAP reports "purchasing unavailable" instead of hanging or crashing. There are no other Google Play services (Play Games, ads).
- **Video playback** (`VideoPlayer`, through `AMediaCodec`/`AImageReader`) isn't implemented: there's no video decoder. The media functions return "unsupported", so a video fails to play instead of crashing the game.
- **AAudio and OpenSL ES** aren't available. AAudio reports "unavailable" and OpenSL ES can't be opened, so audio libraries fall back to other output. Unity's FMOD falls back to `AudioTrack`, which works.
- **No sensors, vibration, camera or GPS:** the accelerometer, gyroscope, vibrator, camera and location providers all report none.
- **No other apps:** `Application.OpenURL` and other Android intents are logged and ignored.
- **Plugins' Java halves:** SDKs such as Firebase or ad networks pair their `.so` with Java code that isn't here. Their native part loads, but the Java classes need stubs (as done for Google Play Billing and Mobile Notifications). Ad SDKs, Firebase and Google Play Games aren't stubbed yet.
- **Compute-shader effects** depend on the GPU. On the Mali-G31, for example, PPv2's MSVO Ambient Occlusion and the debug monitors aren't supported.

**Untested**
- The Mono scripting backend on Unity 7 (Mono works on older Unity, see *Ports in progress*).
- Unity 7 releases other than 7000.0.0a16, and devices other than the R36S.
- Anything online: leaderboards, cloud saves, remote config, ads and analytics uploads.

# The compatibility probe (`tools/buildprobe.sh`)

As new Unity versions come out, they use more of Android than the loader implements. The probe is a Unity project (`tools/UnityProbe/`) that exercises a broad set of platform features (files, prefs, culture and time zones, threads, sockets, crypto, graphics, audio, input, Java/Android APIs, StreamingAssets, Addressables, Localization, a native plugin, notifications, Google Play Billing, IAP, Unity Services) and quits by itself. It's meant for finding the next batch of things to implement, not for running games.

```bash
tools/buildprobe.sh
```

It needs Unity 7000.0.0a16 installed through Unity Hub with Android Build Support, plus Docker for the loader. Close the Unity editor first, because it locks the project.

The script builds a debug `unityloader`, builds the probe's small native test plugin (`tools/UnityProbe/NativePlugin/`) with Unity's NDK, builds the probe APK with Unity in batch mode, and packages everything as a PortMaster port in `tools/portmaster/probe.zip`. `SKIP_LOADER=1` or `SKIP_UNITY=1` reuse the previous loader or APK.

Unzip it into the device's `ports/` folder and run *Probe* from PortMaster. The results land in `ports/probe/`:
- `run.txt`: the loader's full log.
- `conf/probe_report.txt`: one OK/FAIL line per feature.
- `conf/probe_*.png`: screenshots of the graphics checks.

To turn a log into a list of what's missing, most-called first:

```bash
python3 tools/unity_probe_report.py run.txt > report.md
```

The report covers missing Java methods and classes, native symbols, syscalls, and the problems Unity reported itself. To probe a newer Unity, open `tools/UnityProbe` in that version and rebuild. Add checks to `Assets/ProbeRunner.cs` or `Assets/ProbeRunnerExtra.cs` for anything a game uses that the probe doesn't cover yet.
