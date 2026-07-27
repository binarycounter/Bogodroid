# Layton 1 (Curious Village HD) ARM Linux port — plan & research notes

Status: **research complete, implementation not started**. Working on the `neo` branch.
Last updated: 2026-07-27.

## Goal

Port *Professor Layton and the Curious Village HD* (Android release, native lib
`libll1.so`, a custom Level-5 engine — **not Unity**) to run on ARM Linux handhelds
using Bogodroid as the loader/shim framework, following the same approach as the
existing Switch homebrew port in `reference/layton_nx-main/`.

## Key decision: build on `neo`, not `main`

`main`'s loader (`platform/common/so_util.c`) is ELF32/armeabi-v7a only. The `neo`
branch (`upstream/neo`, now checked out locally) already has a working ELF64/aarch64
loader, cross-compile tooling, and a much cleaner per-project scaffold. **Use `neo`
as the base for the new `laytonloader` project.**

## Reference project: `reference/layton_nx-main/` (Switch homebrew port)

- Loads `libll1.so` (arm64 Elf64) via a from-scratch loader (`source/so_util.c`):
  parses PT_LOAD segments, handles `R_AARCH64_ABS64/RELATIVE/GLOB_DAT/JUMP_SLOT`
  plus packed RELR relocations, maps memory via Switch syscalls.
- `source/jni.c` — **hand-rolled name-dispatch fake JNIEnv**, not a generic Java
  object model. Builds a 233-slot JNIEnv vtable; `Call*Method` dispatch compares
  the resolved method **name string** against a hardcoded set of ~15 custom native
  methods the Level-5 engine calls: `MO_PlayMovie`, `MO_GetState`, `UI_GetEditState`,
  `L5iD_*`, `LVL_GetState`, `GL_LoadPNG`, `CARD_GetFilesDirName`,
  `UI_SetIdleTimerDisabled`, etc. Has a manual ref-counted "live object registry"
  (8192 slots) for fake String/Array/Object handles.
- `source/imports.c` — import table mapping every undefined symbol in `libll1.so`
  to newlib/libnx passthroughs, bionic-ABI shims, GLES2, or the mini OpenSL ES impl.
- `source/libc_shim.c` — bionic↔newlib ABI conversion (struct stat layout, `_chk`
  fortify wrappers, fake bionic `__sF` FILE* array for libc++ cout/cerr).
- `source/movie.c` (581 lines) — **FFmpeg**-based .mp4 cutscene decode on a worker
  thread, composited into a GL texture the game itself creates
  (`MO_CreateTexture`/`GL_DrawMovie` JNI calls intercepted).
- `source/opensl.c` (580 lines) — minimal **OpenSL ES 1.0.1** (engine/outputmix/
  buffer-queue player) implemented over libnx `audout`, because CRI ADX2's Android
  backend expects that API.
- `source/main.c` — entry sequence: EGL init → `so_load`+relocate+resolve →
  `patch_game()` hooks a few exported symbols → resolve `JNI_OnLoad`/
  `setViewSize`/`resume`/`render`/`MO_CreateTexture` **before** finalize (dynsym
  becomes unreadable after finalize remaps memory) → finalize+flush caches →
  `so_execute_init_array` → call `JNI_OnLoad` then `setViewSize`/`resume` →
  per-frame loop calling the render entry point with touch input.
- **Not Unity.** No il2cpp/mono, no generic reflection, no real `AAssetManager`/
  `ANativeActivity` lifecycle — engine credited to Level-5 (`OS_Run`, `NitroMain`
  refs — DS-ported engine). Audio via CRI ADX2 (needs OpenSL ES). Credits confirm
  lineage: TheOfficialFloW's original Android so-loader (gtasa_vita) →
  Rinnegatamante's Vita Layton port → fgsfds' Switch so-loader groundwork → this port.

## Bogodroid `neo` branch architecture (confirmed via `upstream/neo`)

### Loader — `loader/` (arch-generic, replaces main's ELF32-only `platform/common/so_util.c`)
- `loader/so_util.cpp`/`.h` — shared relocation/loading logic, handles both ARM32
  and AArch64 relocation types in one switch (`R_AARCH64_RELATIVE`, `_ABS64`,
  `_GLOB_DAT`, `_JUMP_SLOT` alongside `R_ARM_*`).
- `loader/so_util_arm32.cpp` / `loader/so_util_arm64.cpp` — per-arch hooking/trampolines.
- `loader/platform.h` — compile-time `Elf32_*`/`Elf64_*` type selection based on
  `__aarch64__`/`__x86_64`.
- `loader/leb128.h` — SLEB128 decode for Android packed relocations.
- **Caveat:** top-level `CMakeLists.txt` currently only compiles
  `loader/so_util_arm64.cpp` into the build. `so_util_arm32.cpp` exists but is not
  wired into CMake anywhere (dead code today) — the branch is effectively arm64-only
  right now, which is exactly what we need for Layton's `libll1.so`.

### Project scaffold — `projects/<name>/`
Each project is `main.cpp` + `javastubs/` (own JNI classes + `binding.cpp`),
auto-globbed by CMake via `-DPROJ=<name>`. Existing projects:
- **`teapotloader`** — best architectural template. Verified directly (read
  `projects/teapotloader/main.cpp` and `javastubs/teapot.h` on disk): loads
  Google's NDK "Teapot" NativeActivity sample (`lib/arm64-v8a/
  libTeapotNativeActivity.so`, class `com.sample.teapot.TeapotNativeActivity`).
  Sequence: init config/GLES → create fake `Baron::Jvm` → `InitJNIBinding` →
  `so_load` the game .so → build an `ANativeActivity` bound to a fake
  `TeapotNativeActivity` jnivm class → resolve+call `ANativeActivity_onCreate`
  from the loaded .so directly → manually drive `onNativeWindowCreated`/
  `onWindowFocusChanged`/`onResume`/`onStart` → idle loop. **No Unity/Mono
  anywhere in this path** — confirmed no UnityPlayer/mono references in
  `main.cpp` or `teapot.h`. (Note: initial research pass and a later offhand
  comment both floated it as possibly Unity-related — double-checked directly
  against the source and it is not.)
- **`limboloader`** — Playdead's *Limbo*, custom engine, closest precedent for a
  game with **bespoke non-Android native methods**: `LimboActivity` class declares
  `GetLimboCachedAassetsPath()` etc. as real registered methods; `main.cpp` also
  resolves and calls a specific exported symbol directly by name
  (`Java_com_playdead_limbo_LimboActivity_native_1ReportVSyncCallEvent`) rather
  than going through a generic Java dispatch — same pattern the Layton port needs
  for `MO_PlayMovie`/`GL_LoadPNG`/etc.
- `hexagonloader` — openFrameworks-based, less relevant.
- `unityloader` — Mono/IL2CPP + "LemonLoader" .NET CoreCLR host compat path, not
  relevant to Layton (not Unity).

### JNI model
Still real C++ class registration via vendored `libjnivm`/fake-jni
(`Baron::Jvm`, `DEFINE_CLASS_NAME`, `BEGIN_NATIVE_DESCRIPTOR`/
`FakeJni::Function<...>`), **not** the Switch port's name-dispatch hack. Base
hierarchy in shared `javastubs/android.h`: `Context → Activity → NativeActivity`.

**Plan for Layton's custom native surface:** define a `LaytonActivity` (or
similar) extending `jnivm::android::app::NativeActivity` with each of the ~15
custom native methods as real registered member functions
(`MO_PlayMovie`, `MO_GetState`, `UI_GetEditState`, `GL_LoadPNG`,
`CARD_GetFilesDirName`, `UI_SetIdleTimerDisabled`, `L5iD_*`, `LVL_GetState`, ...),
following `teapot.h`/`limbo.h`'s pattern exactly. No changes to jnivm/fake-jni core
needed.

### What's missing on `neo` that Layton needs (net-new work)
- **No FFmpeg / movie decode anywhere** on the branch (confirmed via full-tree
  grep — zero matches for ffmpeg/avcodec/avformat/movie). Need to port
  `reference/layton_nx-main/source/movie.c` logic: FFmpeg decode thread →
  upload frames into a GL texture the game creates, intercepted via the
  `MO_CreateTexture`/`GL_DrawMovie` native methods.
- **No OpenSL ES / CRI ADX2-compatible audio backend.** `neo` only has
  `thunks/openal/` (OpenAL-Soft thunk) and stub `android_media.cpp`
  (`AudioManager` stubs, hardcoded values). Need to port
  `reference/layton_nx-main/source/opensl.c`'s minimal OpenSL ES 1.0.1
  (engine/outputmix/buffer-queue player) onto whatever host audio Bogodroid uses
  on Linux (likely SDL2 audio, since `neo` already links SDL2 for EGL/window/input
  via `thunks/egl_sdl/`).
- **No `bridges/` content yet** — reserved include path in CMakeLists but empty;
  main's bridge concept doesn't exist on neo (folded into `thunks/`). PNG decode
  (`GL_LoadPNG`) can reuse `stb_image.h` (already vendored in the reference project,
  can be copied over) — check `thunks/` for an existing image decode dependency
  before adding a new one.
- **No `projects/laytonloader/`** — new project directory needed, following the
  `teapotloader` template.
- **No `configs/layton.toml`** — new config, same schema as existing
  `configs/teapot.toml` (`[paths]`, `[package]`, `[device]` tables).
- **No `gamefiles/layton/`** — needs `lib/arm64-v8a/libll1.so` + assets from the
  actual Android APK, which we do not currently have on disk.

## Progress log

**2026-07-27**: Switched to `neo` branch locally (`git checkout neo`, tracking
`upstream/neo`). User confirmed they have `libll1.so` + assets and copied them
to `gamefiles/layton/` (verified: `libll1.so` is arm64-v8a, NDK r23, Android 26
target — matches the reference Switch port's binary). Assets live directly under
`gamefiles/layton/data/...` (not an `assets/` APK-style folder), so no
AAssetManager involvement needed — matches the Switch port's direct-fopen
behavior.

Scaffolded `projects/laytonloader/` (Milestone 1: boot-to-render, no movies/audio):
- `main.cpp` — mirrors the Switch port's entry sequence but simplified for
  Bogodroid's thunk model (the guest .so drives its own EGL/GLES calls via
  `so_dynamic_libraries`, so the host doesn't need to manage the GL context
  per-frame like the Switch port did): `init_config` → `sdl_initialize_gles`
  → `so_load` `libll1.so` at `0x50000000` → resolve+call `JNI_OnLoad` →
  resolve `Java_com_Level5_LT1R_MainActivity_{setViewSize,resume,render}` by
  symbol name → call `setViewSize`/`resume` → loop calling `render(env, null,
  1, 0, 0, 0,0,0,0)` each frame with basic SDL event pump (quit on
  `SDL_QUIT`). No touch input, no movie/audio, no frame-timing yet.
- `javastubs/layton.h`/`.cpp` — `jnivm::com::Level5::LT1R::MainActivity`
  (extends `Activity`, not `NativeActivity`, since this engine calls exported
  `Java_com_Level5_LT1R_MainActivity_*` symbols directly rather than driving
  the `ANativeActivity` callback lifecycle) with all ~21 custom native methods
  from the Switch port's `jni.c` dispatch table registered as real jnivm
  methods, each stubbed to a safe default (movie playback returns
  false/not-playing, license checks return 2/licensed, `CARD_GetFilesDirName`
  returns ".", etc.) — same defaults the Switch/Vita ports use. `GL_LoadPNG`
  is fully implemented via vendored `stb_image.h` (copied from
  `reference/layton_nx-main/source/stb_image.h` into
  `projects/laytonloader/javastubs/`).
- `javastubs/binding.h`/`.cpp` — registers `MainActivity` plus the shared
  Android/Java stub classes, following `teapotloader`'s `InitJNIBinding`
  pattern exactly.
- `configs/layton.toml` — same schema as `configs/teapot.toml`;
  `paths.game_files = "../gamefiles/layton/"`, package
  `com.Level5.LT1R`, device display 960x544 (Vita/PS-Vita-era portrait
  resolution used by this game's other ports; adjust as needed).
- No changes needed to the top-level `CMakeLists.txt` — `PROJ_SOURCES` is a
  recursive glob over `projects/${PROJ}/`, so `-DPROJ=laytonloader` picks up
  the new files automatically, same as every other project.

**2026-07-27 (manual review pass, no build available)**: Without a compiler on
hand, did a careful static read-through of `main.cpp`/`layton.h`/`layton.cpp`
against the actual `loader/`, `libjnivm/include/`, and `thunks/` headers on
disk (not guessing from memory) and found/fixed one real bug, plus resolved
one open question:

- **Bug fixed**: `main.cpp` called the `fatal_error(...)` macro without
  including `platform/common/logging.h` (the only place it's defined). Traced
  the full include chain (`config.h`, `so_util.h`, `io_util.h`, `android.h`,
  `javac.h`, the PCH headers) and none of them pull it in — `projects/teapotloader/main.cpp`
  appears to have this same latent gap (it calls `fatal_error` too, with an
  identical include list, and nothing there reaches `logging.h` either).
  Added `#include "logging.h"` to `laytonloader/main.cpp` directly rather than
  relying on an unverified transitive include; worth flagging to upstream if
  `teapotloader` actually fails to build for the same reason.
- **Resolved (was flagged as unverified)**: the double `BEGIN_NATIVE_DESCRIPTOR`
  entry both named `"MO_PlayMovie"` (two overloads). Checked
  `libjnivm/include/jnivm/class.h`: `Class::methods` is a
  `std::vector<std::shared_ptr<Method>>`, not a name-keyed map, and
  `Descriptor::registre` just appends — so registering the same name twice
  with different C++ signatures is exactly how the framework models real JNI
  method overloading, not an edge case. No longer considered a risk.
- Also individually verified: `JNIEnv*`/`JavaVM*`/`jint`/`jobject`/`jfloat`
  are available transitively via `thunks/ndk/anative_activity.h` (already
  included, already uses these types) — no missing `<jni.h>` issue.
  `FakeJni::JByteArray`/`JIntArray`/`JFloatArray` and their
  `getArray()`/`getSize()` accessors, `jnivm::String`'s `c_str()` (inherited
  from `std::string`, confirmed in `libjnivm/include/jnivm/string.h`) and
  `asStdString()` all check out against their actual definitions. `stb_image.h`
  living in `projects/laytonloader/javastubs/` (not project root) is required
  for the include path to find it, since `CMakeLists.txt` only adds
  `${PROJ_SOURCE_DIR}/javastubs` to the per-project include dirs, not
  `${PROJ_SOURCE_DIR}` itself — confirmed this placement is correct.
- CMake itself needs no changes: `PROJ_SOURCES` is `GLOB_RECURSE` over
  `projects/${PROJ}/`, so `-DPROJ=laytonloader` will pick up `main.cpp` and
  everything under `javastubs/` automatically, same as every other project.

**Build/test still blocked on environment**: this machine (Windows, no WSL
distro, no local gcc/cmake) can't compile or run the ARM Linux target. User
will build on a separate Linux machine (with Docker) later — build/boot
verification (tasks #4/#5) deferred until then.

## Open items / blockers (current)

1. **Build/test environment not yet set up.** This dev machine is Windows with
   no WSL distro, no local gcc/cmake, no aarch64 cross toolchain. Resolved
   options: (a) install a WSL distro and build there, (b) use the armhf/aarch64
   chroot the README describes, (c) build+run directly on the target ARM Linux
   handheld, or (d) cross-compile via `cmake/aarch64-linux-gnu.cmake` from
   some other Linux host. **Need user input on which one to use** before Milestone
   1 can be verified.
2. **Overload registration unverified** — see "Known open question" in the
   progress log above (`MO_PlayMovie` double registration).
3. Task list (`TaskList` in this session) tracks remaining work:
   scaffold done (#1-#3), CMake glob verification and actual boot test (#4-#5)
   pending on the build environment above.

## Completed implementation steps (Milestone 1 scaffold)

1. ~~Scaffold `projects/laytonloader/`~~ — done, see progress log.
2. ~~Add `configs/layton.toml`~~ — done.
3. ~~Define JNI class with custom native methods as registered stubs~~ — done
   (`javastubs/layton.h`/`.cpp`), based on `reference/layton_nx-main/source/jni.c`.
4. ~~Wire up `so_load` of `libll1.so` + entry point resolution + call
   sequence~~ — done (`main.cpp`), simplified vs. `teapotloader`'s
   `ANativeActivity` pattern since this engine uses direct `Java_*` symbol
   exports instead.
5. **Get it booting to first render — blocked on build environment (open item #1).**

## Remaining work after Milestone 1 boots

6. Port PNG decode (`GL_LoadPNG`) via stb_image — already implemented, needs
   verification once building is possible.
7. Port movie playback (FFmpeg) — new `thunks/` or project-local module.
8. Port OpenSL-ES-equivalent audio backend onto SDL2 audio.
9. Touch/input wiring for `render()`'s touch args (currently always 0 touches).
10. Save directory / misc remaining JNI intercepts as they surface at runtime.

## Key file references

- Switch port JNI dispatch table (source of truth for method names): 
  `reference/layton_nx-main/source/jni.c`
- Switch port movie playback: `reference/layton_nx-main/source/movie.c`
- Switch port audio: `reference/layton_nx-main/source/opensl.c`
- Switch port import/symbol table: `reference/layton_nx-main/source/imports.c`
- Bogodroid neo loader: `loader/so_util.cpp`, `loader/so_util_arm64.cpp`
- Best template project: `projects/teapotloader/main.cpp`,
  `projects/teapotloader/javastubs/teapot.h`/`.cpp`
- Custom-native-method precedent: `projects/limboloader/javastubs/limbo.h`,
  `projects/limboloader/main.cpp`
- Config schema example: `configs/teapot.toml`
