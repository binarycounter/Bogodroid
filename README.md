# Bogodroid NEO

Bogodroid is a wrapper environment for the native libraries of ARM Android apps, so that those libraries (or whole apps and games) can run on ARM Linux handhelds. The focus is the Unity engine, including **Unity 6.0, 6.6 and 7.0** IL2CPP games.

The project is heavily based on [JohnnyOnFlame/gmloader-next](https://github.com/JohnnyonFlame/gmloader-next) and [ChristopherHX/libjnivm](https://github.com/ChristopherHX/libjnivm).

The idea is to:
- load the libraries and patch them at runtime
- wrap, bridge or stub the symbols they need from Android's system libraries
- provide a JNI interface to placeholder "Java" classes
- provide implementations and stubs for common Java and Android classes
- wrap audio, GLES and input onto host interfaces (SDL2, EGL)

It won't run every app out of the box: it's a framework and starting point for porting projects. For existing porting projects, see the *Ports in progress* table and the `projects/` folder.

For a complete, working example of a packaged Unity 7 game, see this [PortMaster zip](https://playkye.com/dl/?type=portmaster). It shows how the `unityloader` binary, its config file and the game files fit together in a real port.

# What the Unity build needs

Build the game for Android as an **ARM64 IL2CPP APK** (not an App Bundle / `.aab`). Everything else in Player Settings is up to you: both Activity and GameActivity entry points, any render pipeline, stripping, colour space and Development builds all work.

The one graphics requirement: **OpenGLES3 must be in the Graphics APIs list**. *Auto Graphics API* is fine, and Vulkan can be listed alongside it, but the loader only provides OpenGL ES.

| Unity version | Supported |
|---|---|
| 6.0 (6000.0) | ✅ |
| 6.6 (6000.6) | ✅ |
| 7.0 (7000.0) | ✅ (developed against 7000.0.0a16) |

Unity 2021.3 support from earlier NEO versions is unchanged (see *Ports in progress*).

# Building

```bash
git clone --recursive --branch neo https://github.com/binarycounter/Bogodroid/
cd Bogodroid
```

### In Docker (recommended)

The loader is built inside an ARM64 Docker container (`Dockerfile`, Ubuntu 20.04). The old base distro is deliberate: binaries only run on devices whose glibc is at least as new as the one they were built with, and handheld firmwares such as ArkOS are old. On an x86 PC, Docker runs the container through QEMU, which is slow but needs nothing else installed.

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
- `./build.sh asan` and `./build.sh test` are for testing; see *Testing and debugging*.

### Natively, on an ARM Linux machine

On a Debian-based system:

```bash
sudo apt install build-essential cmake git pkg-config libzip-dev libsdl2-dev libegl-dev libgles-dev
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release     # or Debug (slower, extensive trace logging), or RelWithDebInfo
make -j$(nproc)
```

The binary only runs on systems whose glibc is at least as new as the build machine's.

### Building a specific project

By default the `unityloader` project is built. To build another one, pass the folder name under `projects/` to CMake, for example `cmake .. -DCMAKE_BUILD_TYPE=Release -DPROJ=hexagonloader`.

# Running a Unity game

A port folder looks like this (the probe in `tools/portmaster/` is laid out this way):

```
mygame/
  unityloader
  mygame.toml
  gamedata/
    mygame.apk                         # the game's APK, unedited, any name (only one .apk in this folder)
    lib/arm64-v8a/libc++_shared.so     # not in the APK: copy it from the Unity NDK (see tools/buildprobe.sh)
    usr/share/zoneinfo/tzdata          # time zones for Unity: python3 tools/make_android_tzdata.py <this path>
```

The config file is optional. Run `./unityloader mygame.toml`, or just `./unityloader` to use `game.toml` if it exists. Every key has a default, so an empty file, an unreadable one or none at all still loads a game laid out as above:

| Setting | Default |
|---|---|
| `[paths] game_files` | `./gamedata/` if it exists, else the folder the loader is started from |
| `[paths] apk` | the one `.apk` file in `game_files`, whatever it's called (an error if there are several) |
| `[paths] android_files`, `android_external_files` | `conf/` next to where the loader is started (created) |
| `[paths] android_cache` | `cache/` next to where the loader is started (created) |
| `[package] ...` | read from the APK's `AndroidManifest.xml` |
| `[device] splash` | `splash.bmp` next to where the loader is started, if present |
| `[device] displayWidth/Height` | the real screen size, fullscreen |
| `[input] controller` | `true` |

A file that fails to parse is reported and ignored, not fatal. The example below spells out the layout above (the defaults):

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
# Optional overrides. Without them, packageName, versionName, versionCode, minSdkVersion and targetSdkVersion
# are read from the AndroidManifest.xml inside [paths] apk.
packageName="com.company.game"
versionName="1.0.0"
mainIntentBundle = { unity = "" }

[device]
splash = "splash.bmp"
```

```bash
./unityloader mygame.toml
```

With `[paths] apk`, the loader gives Unity the real APK as its package and Unity reads its data straight from it, as it does on Android. That is also what makes `jar:file://…!/assets/…` URLs work: StreamingAssets, Addressables/AssetBundles and the Unity Services config. On the first run (and whenever the APK changes) the loader extracts the native libraries and any managed code into `gamedata/`. Without `apk`, the APK's `assets/` and `lib/` folders can be extracted into `game_files` instead, as older versions required (see `configs/unity.toml`), but `jar:` URLs won't work then.

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
controller = true         # Gamepad support (default true).
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

# Unity 6 and 7 support

Tested on an R36S (Rockchip RK3326, Mali-G31, ArkOS) with Unity 7000.0.0a16 IL2CPP builds, including 45-minute soak runs of the compatibility probe described below.

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
- Java callbacks into C#: `AndroidJavaProxy` listeners and `AndroidJavaRunnable` (for example `runOnUiThread`).
- Crypto:
  - `MessageDigest` (MD5, SHA-1, SHA-2) and `SecureRandom`.
  - AES `Cipher` (ECB, CBC and CTR, PKCS5/7 or no padding).
  - HMAC `Mac`.
- Android framework pieces Unity asks about:
  - `Build`, `UnityPlayer.currentActivity`, `Context` folders, `PackageManager`, `Configuration` and `Locale`.
  - Battery level and charging state, read from `/sys/class/power_supply`.
  - `Choreographer` frame callbacks at the display rate (60 Hz).
- Mobile Notifications package: initializes, registers channels, and schedules or cancels notifications. They are never displayed.

**Not implemented, or deliberately reporting "none"**
- **Networking beyond plain sockets is untested.** The test device has no network, so HTTP/HTTPS through Unity has never succeeded there. TLS through Java would also need `KeyAgreement`, certificates and AES-GCM, which aren't implemented.
- **Unity Gaming Services and In-App Purchasing.** These need a network, a linked Unity Cloud project and (for IAP) Google Play Billing. The device reports itself as offline, so they fail the way they would on a phone with no connection, and games should already handle that. Google Play Billing is stubbed: connecting answers `BILLING_UNAVAILABLE` through the game's listener, as on a phone without the Play Store, so IAP reports "purchasing unavailable" instead of hanging or crashing. There are no other Google Play services (Play Games, ads).
- **Video playback** (`VideoPlayer`, through `AMediaCodec`/`AImageReader`) isn't implemented: there's no video decoder. The media functions return "unsupported", so a video fails to play instead of crashing the game.
- **AAudio and OpenSL ES** aren't available. AAudio reports "unavailable" and OpenSL ES can't be opened, so audio libraries fall back to other output. Unity's FMOD falls back to `AudioTrack`, which works.
- **No sensors, vibration, camera or GPS:** the accelerometer, gyroscope, vibrator, camera and location providers all report none.
- **No other apps:** `Application.OpenURL` and other Android intents are logged and ignored.
- **Plugins' Java halves:** SDKs such as Firebase or ad networks pair their `.so` with Java code that isn't here. Their native part loads, but the Java classes need stubs (as done for Google Play Billing and Mobile Notifications). Ad SDKs, Firebase and Google Play Games aren't stubbed yet.
- **Compute-shader effects** depend on the GPU. On the Mali-G31, for example, PPv2's MSVO Ambient Occlusion and the debug monitors aren't supported.

**Untested**
- Anything online: leaderboards, cloud saves, remote config, ads and analytics uploads.

# Testing and debugging

## The compatibility probe (`tools/buildprobe.sh`)

As new Unity versions come out, they use more of Android than the loader implements. The probe is a Unity project (`tools/UnityProbe/`) that exercises a broad set of platform features (files, prefs, culture and time zones, threads, sockets, crypto, graphics, audio, input, Java/Android APIs, StreamingAssets, Addressables, Localization, a native plugin, notifications, Google Play Billing, IAP, Unity Services) and quits by itself. It's meant for finding the next batch of things to implement, and for soak testing the loader (below), not for running games.

```bash
tools/buildprobe.sh
```

It needs Unity 7 installed through Unity Hub with Android Build Support, plus Docker for the loader. Close the Unity editor first, because it locks the project.

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

## Soak testing

Some loader bugs only show up after a game has been running for a while (leaks, races between threads). After its checks, the probe can keep running for a set time, doing what a game does all the time: rendering, allocating and collecting garbage, file I/O, worker threads and PlayerPrefs. It logs a heartbeat every 10 seconds (`[SOAK] t=…s frames=… fps=…`) and `[SOAK] DONE` at the end.

- **On a device:** put the number of seconds in `conf/soak.txt` in the port folder (for example `2700` for 45 minutes), or set `PROBE_SOAK_SECONDS`. 0 or no file runs the probe as usual.
- **In a container:** `tools/soak.sh [seconds] [release|asan] [apk]` runs the loader with the probe in the build image, with a virtual display and software OpenGL ES (`tools/soak/Dockerfile`), and prints a verdict. On an x86 machine this runs under QEMU and renders well under one frame per second, so it exercises threads and callbacks rather than real frame rates; use a long run.

The loader also logs `[CHOREOGRAPHER] N frame callbacks/s …` every 30 seconds, to check frame pacing in any log.

## Stress tests and sanitizers

`./build.sh test` builds and runs host-side stress tests (`tools/stress/`) for the parts of the loader that don't need a game: the bionic pthread and semaphore shims, `ALooper`, the JNI VM (per-thread environments, class and method lookup, strings), `Looper`/`Handler`/`Message`/`Thread`, Java proxy callbacks and the crash-report trail. They run natively under AddressSanitizer+UBSan and ThreadSanitizer, which catch races and memory errors in minutes that would otherwise take a long session on a device. `STRESS_SECONDS=300 ./build.sh test` runs them longer. `tools/stress/run.sh arm64` runs the same tests inside the ARM64 container, which catches what x86 doesn't (for example misaligned atomics, which fault on ARM).

`./build.sh asan` builds the loader with AddressSanitizer+UBSan (`build-arm64-asan/`). It is slow and needs more memory than a handheld has; use it with `tools/soak.sh 600 asan`, or on a desktop ARM64 machine. ThreadSanitizer isn't available for ARM64 in the build image's GCC, so thread races are checked by the host stress tests.

## Crash reports

When the loader crashes, the end of the log has a `=== unityloader crash report ===` block: the build (git revision and build time), the crashing thread's name and id, the uptime, and the last 128 loader events (callbacks into the game, thread starts and exits, library loads, swallowed exceptions). Unity's own crash handler runs first, so the `LOG[CRASH]` lines above it have the register dump and backtrace. When reporting a crash, include the whole `run.txt`.

## Debugging with QEMU and GDB-multiarch

(This expects the chroot environment as provided by [this VM](https://forum.odroid.com/viewtopic.php?p=306185#p306185))

1. Exit your chroot, we will call the binary directly
2. Enter the build directory
3. Start the binary with:
`QEMU_LD_PREFIX=/mnt/data/aarch64/ qemu-arm-static -g 5 ./unityloader ../configs/unity.toml`
4. Open another terminal and type in `gdb-multiarch`
5. Enter the location of your chroot: `set sysroot /mnt/data/armhf`
6. Enter the location of your binary: `file /mnt/data/armhf/root/bogodroid/build/unityloader`
7. Connect to the open GDB session in QEMU: `target remote :5`
8. "c" to run the program, "si" to step a single instruction, "bt" to read stack trace, "break" to set breakpoint. ***Refer to the GDB manpage and internet resources on how to debug***

## JNI tracing your app

(This requires a rooted Android device and a computer with ADB and Python installed)

1. [Install a Frida server on your Android device](https://frida.re/docs/android/)
2. Install jnitrace on your computer: `pip install jnitrace`
3. Run the trace: `jnitrace -o jnitrace.json com.example.myapplication`
4. Press CTRL-C to finish acquiring data
5. `jnitrace.json` will contain all JNI calls made by the application
6. `python tools/jnitrace_parse.py <input_json_file> <output_json_file>` will give you a consolidated tree of all classes, methods and fields (and their signatures) accessed during the tracing period.

See `tools/unity_traces` for a trace of a minimal Unity game starting up.

## IL2CPP trace logging your game

1. Locate your copy of il2cpp from your Unity Editor in `Data/il2cpp`
2. Copy this folder to `projects/unityloader/`
3. Run `cd tools/ && python3 gen_il2cpp_log_shim.py`
4. Configure your build with `-DIL2CPP_TRACE=1` (run CMake yourself, as in *Building natively*)

# Ports in progress

| Name                              | Project Name  | Status        | Notes                                                                                                              |
|-----------------------------------|---------------|---------------|--------------------------------------------------------------------------------------------------------------------|
| Unity 6.0 / 6.6 / 7.0 (aarch64 il2cpp) | unityloader | Working | Complete games run; tested with 45-minute soak runs. See *Unity 6 and 7 support*. |
| Unity 2021.3f (aarch64 il2cpp)                      | unityloader   | Working  | Runs simple games. Graphics, audio and input work. |
| Unity 2021.3f (aarch64 mono)                      | unityloader   | Working POC  | Simple demos and benchmarks are working, with low but reasonable performance. Implementation is still incomplete. |
| Super Hexagon                     | hexagonloader | Not Booting   | Unchanged from Pre-NEO Bogodroid, needs complete rework  |
| Limbo                             | limboloader   | Loading Screen| Unchanged from Pre-NEO Bogodroid, needs complete rework  |
| Crazy Taxi Classic                | taxiloader    | Investigated  | Requires better file handling for loading the big OBB files, before it can be attempted.                           |
| Terraria                          | -             | Not attempted | (Engine: FNA, Monogame port might be easier)                                                                       |
| Baba is you                       | -             | Not attempted |                                                                                                                    |
| Super Meat Boy Forever            | -             | Not attempted |                                                                                                                    |
| Streets of rage 4                 | -             | Not attempted | (Engine: FNA, Monogame port might be easier)                                                                       |
| Chrono Trigger                    | -             | Not attempted | (Engine: cocos2d-x)                                                                                                |
| Dead Cells                        | -             | Not attempted |                                                                                                                    |
| Brotato                           | -             | Not attempted |                                                                                                                    |
| Castlevania Symphony of the Night | -             | Not attempted |                                                                                                                    |
| Five Nights At Freddy             | -             | Not attempted |                                                                                                                    |
| Gris                              | -             | Not attempted |                                                                                                                    |
| The Way Home                      | -             | Not attempted |                                                                                                                    |
| Vampire Survivors                 | -             | Not attempted |                                                                                                                    |
| Scourgebringer                    | -             | Not attempted |                                                                                                                    |
| Haak                              | -             | Not attempted |                                                                                                                    |
