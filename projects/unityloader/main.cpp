#include "abi.h"
#include "logging.h"
#include "apk.h"
#include "globals.h"
#include <cstdlib>
#include <string>
#include <execinfo.h>
#include <iostream>

#include "toml++/toml.hpp"
toml::table config;
#include "config.h"

#include "io_util.h"
#include "javastubs/binding.h"
#include "monocompat/monobridge.h"
#include "monocompat/il2cpp_log_shim.h"
#include "platform.h"
#include "so_util.h"
#include <baron/baron.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>

#include "anative_activity.h"
#include "ndk.h"
#include "alooper.h"

#include "debug_utils.h"
#include "egl_sdl.h"
#include "glad.h"
#include "glad_egl.h"
#include "gles2.h"
#include "input_backend.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_hints.h>
#include <cstring>
#include <dirent.h>

thread_local int tls0[2 << 12] = {};
int foo() { return tls0[0]++; }

using namespace FakeJni;

#include <chrono>
// Prints how long we've been running, so slow startup phases are easy to spot in the log.
static void stage(const char* what)
{
    // The same clock as the "LOG[tag]" lines (bogo_uptime_ms), so the two interleave in order.
    fprintf(stderr, "[+%5ldms] %s\n", bogo_uptime_ms(), what);
}

// bool il2cpp_gc_is_incremental_stub()
// {
//     return false;
// }
// #include "thunk_gen.h"
// DynLibFunction symtable_il2cppfake[] = {
//     NO_THUNK("il2cpp_gc_is_incremental",(uintptr_t)&il2cpp_gc_is_incremental_stub),
//     NULL
// };

extern DynLibFunction symtable_monobridge[];
extern DynLibFunction symtable_il2cpp_log[];
extern DynLibFunction symtable_libc[];
extern DynLibFunction symtable_ndk[];
extern DynLibFunction symtable_gles2[];
extern DynLibFunction symtable_media[];
extern DynLibFunction symtable_egl_sdl[];

DynLibFunction* so_static_patches[32] = {
    NULL,
};

DynLibFunction* so_dynamic_libraries[32] = {
    symtable_libc,
    symtable_ndk,
    symtable_egl_sdl,
    symtable_gles2,
    symtable_media,
    NULL
};

so_module* loaded_modules[128] = {
    NULL
};

extern SDL_Window* sdl_win;
extern SDL_GLContext sdl_ctx;
extern EGLDisplay egl_display;
extern EGLContext egl_context;
extern EGLSurface egl_surface;

bool llcompat = true;

Baron::Jvm vm;

#pragma GCC push_options
#pragma GCC optimize("O0")
void gdb_break_here()
{
}
#pragma GCC pop_options
/**
 * @brief Checks if a string ends with a given suffix.
 */
static int ends_with(const char* str, const char* suffix)
{
    if (!str || !suffix)
        return 0;
    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);
    if (len_suffix > len_str)
        return 0;
    return strncmp(str + len_str - len_suffix, suffix, len_suffix) == 0;
}



int main(int argc, char* argv[])
{
    std::string startDir;
    {
        char d[1024];
        if (getcwd(d, sizeof(d))) startDir = d;
    }
    setvbuf(stdout, NULL, _IONBF, 0); // Unbuffered so nothing is lost if we die or _exit
    setvbuf(stderr, NULL, _IONBF, 0);
    {
        char cwd[512] = "?";
        if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '?';
        fprintf(stderr, "unityloader built %s %s, cwd=%s\n", __DATE__, __TIME__, cwd);
        for (int i = 0; i < argc; i++) fprintf(stderr, "  argv[%d]=%s\n", i, argv[i]);
        atexit([]() { fprintf(stderr, "unityloader: process exiting (atexit)\n"); });
    }
    // Stop SDL from installing its own fatal-signal handlers: they swallow the original fault and re-raise,
    // which hides the real crash address/registers from Unity's crash reporter.
    setenv("SDL_NO_SIGNAL_HANDLERS", "1", 1);
    print_backtrace_on_segfault(); // Registers a signal handler to print backtrace on segfaults
    exit_on_signals(); // Exits when CTRL-C is presset (or SIGINT or SIGTERM is received)

    // The config file is optional: without one (or with an empty one) the defaults in init_config() apply.
    if (argc < 2)
        fprintf(stderr, "Usage: %s [config file]; no config given, using game.toml if present, else defaults\n", argv[0]);

    // The main thread plays the role of Android's UI thread: Unity looks its looper up with ALooper_forThread().
    ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);

    // Init config, GLES pointers, JNI VN and bindings
    init_config(argc > 1 ? argv[1] : nullptr);
    stage("config loaded");
    {
        std::string splash = config["device"]["splash"].value_or<std::string>("");
        if (!splash.empty()) {
            if (splash[0] != '/')
                splash = startDir + "/" + splash;
            splash_show(splash.c_str());
            stage("splash shown");
        }
    }
    if (!config_apk_problem().empty()) {
        fprintf(stderr, "Error: %s\n", config_apk_problem().c_str());
        return 1;
    }
    // After the splash, so the first run's extraction isn't spent on a black screen.
    // [paths] apk: native libraries and managed code have to be plain files; take any missing ones from the APK.
    if (!bogo_apk_extract_runtime_files())
        fprintf(stderr, "[APK] some files could not be extracted; loading may fail\n");
    if (!bogo_apk_path().empty())
        stage("APK runtime files ready");
    if (!bogo_apk() && access(BOGO_LIB_DIR "/libunity.so", R_OK) != 0) {
        fprintf(stderr, "Error: no game found: put the game's .apk in the game files folder (or set [paths] apk), "
                        "or extract it there so that " BOGO_LIB_DIR "/libunity.so exists.\n");
        return 1;
    }
    // A process can only run code of its own bitness: say so now, rather than failing on missing libraries later.
    if (ApkArchive* apk = bogo_apk()) {
        if (!apk->find(BOGO_LIB_DIR "/libunity.so")) {
            std::string have;
            for (auto& abi : bogo_apk_abis())
                have += (have.empty() ? "" : ", ") + abi;
            fprintf(stderr, "Error: %s has no " BOGO_LIB_DIR "/libunity.so. It ships native libraries for: %s.\n"
                            "This is a %d-bit loader and can only run %s builds. Export the game for that ABI%s.\n",
                    bogo_apk_path().c_str(), have.empty() ? "(none)" : have.c_str(), BOGO_ABI_BITS, BOGO_ABI,
                    BOGO_ABI_BITS == 64 ? " (Unity: Player Settings > Other Settings > Target Architectures > ARM64)" : "");
            return 1;
        }
    }
    // sdl_initialize_gles();
    InitJNIBinding(&vm);

    JClass* unityClass = vm.findClass("com/unity3d/player/UnityPlayer").get();

    auto unityActivity = std::make_shared<jnivm::com::unity3d::player::UnityPlayerActivity>();
    auto unityPlayer = std::make_shared<jnivm::com::unity3d::player::UnityPlayer>();
    auto unityPlayerObj = std::dynamic_pointer_cast<jnivm::Object>(unityPlayer);
    jnivm::com::unity3d::player::UnityPlayer::currentActivity = unityActivity;
    auto& backend = InputBackend::instance();

    int module_count = 0;

    // There is a weird incompatibility with Unity's incremental GC. Luckily there's a commandline parameter to overwrite it.
    putenv("GC_DISABLE_INCREMENTAL=1");

    stage("loading libraries");
    printf("Loading libc++\n");
    so_module lcpp = {};
    uintptr_t addr_lcpp = 0x3100000000;
    const char* path_lcpp = BOGO_LIB_DIR "/libc++_shared.so";
    if (!load_so_from_file(&lcpp, path_lcpp, addr_lcpp)) {
        printf("libc++_shared.so not found; the game may bring its own C++ runtime\n");
    }
    loaded_modules[module_count++] = &lcpp;

    so_module lbootstrap = {};
    uintptr_t addr_lbootstrap = 0x4200000000;
    const char* path_lbootstrap = BOGO_LIB_DIR "/libBootstrap.so";

    so_module ldobby = {};
    uintptr_t addr_ldobby = 0x4250000000;
    const char* path_ldobby = BOGO_LIB_DIR "/libdobby.so";
    if (llcompat) {
        printf("LemonLoader Compat active, loading libBootstrap.so and libdobby.so\n");
        load_so_from_file(&ldobby, path_ldobby, addr_ldobby, true);
        loaded_modules[module_count++] = &ldobby;

        load_so_from_file(&lbootstrap, path_lbootstrap, addr_lbootstrap, true);
        loaded_modules[module_count++] = &lbootstrap;

        so_module* mod = (so_module*)calloc(1, sizeof(so_module));
        if (mod && load_so_from_file(mod, BOGO_LIB_DIR "/libcrypto.so", 0x4251000000, true)) {
            printf("  Loaded: libcrypto.so\n");
            loaded_modules[module_count++] = mod;
        }

        mod = (so_module*)calloc(1, sizeof(so_module));
        if (mod && load_so_from_file(mod, BOGO_LIB_DIR "/libssl.so", 0x4252000000, true)) {
            printf("  Loaded: libssl.so\n");
            loaded_modules[module_count++] = mod;
        }

        mod = (so_module*)calloc(1, sizeof(so_module));
        if (mod && load_so_from_file(mod, "assets/dotnet/host/fxr/8.0.6/libhostfxr.so", 0x4253000000, true)) {
            printf("  Loaded: libhostfxr.so\n");
            loaded_modules[module_count++] = mod;
        }

        mod = (so_module*)calloc(1, sizeof(so_module));
        if (mod && load_so_from_file(mod, "assets/dotnet/shared/Microsoft.NETCore.App/8.0.6/libhostpolicy.so", 0x4254000000, true)) {
            printf("  Loaded: libhostpolicy.so\n");
            loaded_modules[module_count++] = mod;
        }

        mod = (so_module*)calloc(1, sizeof(so_module));
        if (mod && load_so_from_file(mod, "assets/dotnet/shared/Microsoft.NETCore.App/8.0.6/libcoreclr.so", 0x4255000000, true)) {
            printf("  Loaded: libcoreclr.so\n");
            loaded_modules[module_count++] = mod;
        }

        mod = (so_module*)calloc(1, sizeof(so_module));
        if (mod && load_so_from_file(mod, "assets/dotnet/shared/Microsoft.NETCore.App/8.0.6/libSystem.Native.so", 0x4256000000, true)) {
            printf("  Loaded: libSystem.Native.so\n");
            loaded_modules[module_count++] = mod;
        }

        mod = (so_module*)calloc(1, sizeof(so_module));
        if (mod && load_so_from_file(mod, "assets/dotnet/shared/Microsoft.NETCore.App/8.0.6/libSystem.Globalization.Native.so", 0x4257000000, true)) {
            printf("  Loaded: libSystem.Globalization.Native.so\n");
            loaded_modules[module_count++] = mod;
        }

        mod = (so_module*)calloc(1, sizeof(so_module));
        if (mod && load_so_from_file(mod, "assets/dotnet/shared/Microsoft.NETCore.App/8.0.6/libSystem.IO.Compression.Native.so", 0x4258000000, true)) {
            printf("  Loaded: libSystem.IO.Compression.Native.so\n");
            loaded_modules[module_count++] = mod;
        }

        mod = (so_module*)calloc(1, sizeof(so_module));
        if (mod && load_so_from_file(mod, "assets/dotnet/shared/Microsoft.NETCore.App/8.0.6/libSystem.Security.Cryptography.Native.OpenSsl.so", 0x4259000000, true)) {
            printf("  Loaded: libSystem.Security.Cryptography.Native.OpenSsl.so\n");
            loaded_modules[module_count++] = mod;
        }

        auto lemonBootJNI_OnLoad = (jint (*)(JavaVM* vm, void* reserved))(so_symbol(&lbootstrap, "JNI_OnLoad"));
        if (lemonBootJNI_OnLoad) {
            printf("calling JNI_OnLoad from libBootstrap.so\n");
            lemonBootJNI_OnLoad(&vm, nullptr);
        }
    }

    printf("Loading libmain\n");
    so_module lmain = {};
    uintptr_t addr_lmain = 0x3200000000;
    const char* path_lmain = BOGO_LIB_DIR "/libmain.so";
    // Newer Unity versions have no libmain.so: the Java side loads libunity.so directly.
    bool haveLibmain = load_so_from_file(&lmain, path_lmain, addr_lmain, true);
    if (haveLibmain) {
        loaded_modules[module_count++] = &lmain;
    } else {
        printf("No libmain.so found, assuming newer Unity (libunity.so is loaded directly)\n");
    }

    printf("Loading libil2cpp\n");
    so_module lil2cpp = {};
    so_module lposix = {};
    so_module lmnative = {};
    uintptr_t addr_lil2cpp = 0x3600000000;
    const char* path_lil2cpp = BOGO_LIB_DIR "/libil2cpp.so";
    if (!load_so_from_file(&lil2cpp, path_lil2cpp, addr_lil2cpp, true)) {
        printf("il2cpp not found, trying libmono\n");
        const char* path_mono = BOGO_LIB_DIR "/libmonobdwgc-2.0.so";
        if (!load_so_from_file(&lil2cpp, path_mono, addr_lil2cpp)) {
            return 1;
        }
        printf("Loading libMonoPosixHelper.so\n");
        uintptr_t addr_lposix = 0x3700000000;
        const char* path_lposix = BOGO_LIB_DIR "/libMonoPosixHelper.so";
        if (!load_so_from_file(&lposix, path_lposix, addr_lposix)) {
            return 1;
        }
        loaded_modules[module_count++] = &lposix;

        printf("Loading libmono-native.so\n");
        uintptr_t addr_lmnative = 0x3750000000;
        const char* path_lmnative = BOGO_LIB_DIR "/libmono-native.so";
        if (!load_so_from_file(&lmnative, path_lmnative, addr_lmnative)) {
            return 1;
        }
        loaded_modules[module_count++] = &lmnative;

        so_dynamic_libraries[4] = symtable_monobridge;
        so_dynamic_libraries[5] = NULL;
        monobridge_init(&lil2cpp);
    }
    loaded_modules[module_count++] = &lil2cpp;

#ifdef IL2CPP_TRACE
    if(config["debug"]["log_il2cpp"].value_or<bool>(false))
        il2cpp_log_shim_init(&lil2cpp, symtable_monobridge);
#endif

    printf("Loading libunity\n");
    so_module lunity = {};
    uintptr_t addr_lunity = 0x3800000000;
    const char* path_lunity = BOGO_LIB_DIR "/libunity.so";
    if (!load_so_from_file(&lunity, path_lunity, addr_lunity)) {
        return 1;
    }
    loaded_modules[module_count++] = &lunity;

    printf("Loading libburst\n");
    so_module lburst = {};
    uintptr_t addr_lburst = 0x4000000000;
    const char* path_lburst = BOGO_LIB_DIR "/lib_burst_generated.so";
    if (!load_so_from_file(&lburst, path_lburst, addr_lburst, true)) {
        printf("No libburst found\n");
    } else
        loaded_modules[module_count++] = &lburst;

    printf("Loading libUnityHelp\n");
    so_module lhelpers = {};
    uintptr_t addr_lhelpers = 0x4200000000;
    const char* path_lhelpers = BOGO_LIB_DIR "/libUnityHelpers_Android.so";
    load_so_from_file(&lhelpers, path_lhelpers, addr_lhelpers, true);
    loaded_modules[module_count++] = &lhelpers;

    const char* directory = "assets/bin/Data/Managed/";
    DIR* d = opendir(directory);
    if (!d) {
        // IL2CPP builds (esp. newer Unity) have no Managed folder of .dll.so assemblies.
        printf("%s not found, skipping assembly loading (IL2CPP build?)\n", directory);
    } else
        printf("Loading assemblies from: %s\n", directory);
    struct dirent* entry;
    while (d && (entry = readdir(d)) != NULL) {
        if (!ends_with(entry->d_name, ".dll.so")) {
            continue;
        }

        char path[512];
        // Assumes the directory path has a trailing slash
        snprintf(path, sizeof(path), "%s%s", directory, entry->d_name);

        so_module* mod = (so_module*)calloc(1, sizeof(so_module));
        if (mod && load_so_from_file(mod, path, (uintptr_t)NULL)) {
            printf("  Loaded: %s\n", entry->d_name);
            loaded_modules[module_count++] = mod;
        } else {
            fprintf(stderr, "  Failed to load: %s\n", path);
            free(mod); // It is safe to call free() on a NULL pointer
        }
    }
    if (d) closedir(d);

    if (haveLibmain) {
        auto mainJNI_OnLoad = (jint (*)(JavaVM* vm, void* reserved))(so_symbol(&lmain, "JNI_OnLoad"));
        if (mainJNI_OnLoad) {
            printf("calling JNI_OnLoad from libmain.so\n");
            mainJNI_OnLoad(&vm, nullptr);
        }

        JClass* nativeLoaderClass = vm.findClass("com/unity3d/player/NativeLoader").get();
        LocalFrame frame(vm);
        auto mainLoad = nativeLoaderClass->getMethod("(Ljava/lang/String;)Z", "load");

        printf("calling com/unity3d/player/NativeLoader/load from libmain.so\n");
        jvalue ret = mainLoad.invoke(frame.getJniEnv(), nativeLoaderClass, (JString) BOGO_LIB_DIR);
        if (!ret.z) {
            printf("libmain.so:load returned false, game could not be loaded\n");
            return 1;
        }
    }

    auto il2cppJNI_OnLoad = (jint (*)(JavaVM* vm, void* reserved))(so_symbol(&lil2cpp, "JNI_OnLoad"));
    if (il2cppJNI_OnLoad) {
        printf("calling JNI_OnLoad from libil2cpp.so\n");
        il2cppJNI_OnLoad(&vm, nullptr);
    }

    auto unityJNI_OnLoad = (jint (*)(JavaVM* vm, void* reserved))(so_symbol(&lunity, "JNI_OnLoad"));
    if (unityJNI_OnLoad) {
        printf("calling JNI_OnLoad from libunity.so\n");
        unityJNI_OnLoad(&vm, nullptr);
    }

    backend.setKeyCallback([unityActivity](std::shared_ptr<jnivm::android::view::KeyEvent> event) {
        unityActivity->injectEvent(event);
    });

    // Optional: pause the game while the window doesn't have focus ([device] pause_on_focus_loss, off by default).
    static std::atomic<int> focusState{-1}; // -1 = nothing pending, 0 = lost, 1 = gained
    const bool pauseOnFocusLoss = config["device"]["pause_on_focus_loss"].value_or<bool>(false);
    if (pauseOnFocusLoss)
        backend.setFocusCallback([](bool focused) { focusState = focused ? 1 : 0; });

    backend.setMotionCallback([unityActivity](std::shared_ptr<jnivm::android::view::MotionEvent> event) {
        unityActivity->injectEvent(event);
    });

    // Debug: list everything libunity registered on UnityPlayer (names + signatures), to find signature changes between Unity versions.
    if (getenv("BOGODROID_VERBOSE")) {
        std::lock_guard<std::mutex> lock(unityClass->mtx);
        fprintf(stderr, "Methods registered on com/unity3d/player/UnityPlayer (%zu):\n", unityClass->methods.size());
        for (auto& m : unityClass->methods)
            fprintf(stderr, "  %s %s%s\n", m->name.c_str(), m->signature.c_str(), m->native ? " [native]" : "");
    }
    // [libraries] preload = ["libfoo.so", ...]: plugins that must be loaded before Unity starts (normally they're
    // loaded when the game first asks for them). Their dependencies come along and JNI_OnLoad is called.
    if (auto* preload = config["libraries"]["preload"].as_array()) {
        for (auto& item : *preload) {
            auto name = item.value<std::string>();
            if (!name) continue;
            if (so_load_library(name->c_str(), &vm))
                printf("Preloaded %s\n", name->c_str());
            else
                fprintf(stderr, "[libraries] preload: %s not found in " BOGO_LIB_DIR "\n", name->c_str());
        }
    }
    stage("libraries loaded, starting Unity");
    auto activity = std::make_shared<jnivm::android::app::Activity>();
    LocalFrame frame2(vm);
    {
        // initJni is a native method libunity registers (RegisterNatives) in JNI_OnLoad. Which class carries it and
        // with which signature depends on the Unity version: UnityPlayer in older and 7.x builds, one of the
        // UnityPlayerFor... subclasses in some 6.x builds. Look it up by name instead of assuming either.
        const char* initClasses[] = { "com/unity3d/player/UnityPlayer", "com/unity3d/player/UnityPlayerForActivityOrService",
                                      "com/unity3d/player/UnityPlayerForGameActivity" };
        jnivm::Method* initJni = nullptr;
        std::string initClassName;
        for (const char* cname : initClasses) {
            JClass* c = vm.findClass(cname).get();
            if (!c) continue;
            std::lock_guard<std::mutex> lock(c->mtx);
            for (auto& m : c->methods)
                if (m->name == "initJni" && m->native) {
                    initJni = m.get();
                    initClassName = cname;
                    break;
                }
            if (initJni) break;
        }
        if (!initJni) {
            fprintf(stderr, "Error: libunity registered no native initJni on any known UnityPlayer class; this Unity version isn't supported yet.\n");
            for (const char* cname : initClasses) {
                JClass* c = vm.findClass(cname).get();
                if (!c) continue;
                std::lock_guard<std::mutex> lock(c->mtx);
                fprintf(stderr, "Methods registered on %s (%zu):\n", cname, c->methods.size());
                for (auto& m : c->methods)
                    fprintf(stderr, "  %s %s%s\n", m->name.c_str(), m->signature.c_str(), m->native ? " [native]" : "");
            }
            return 1;
        }
        if (initJni->signature == "(Landroid/content/Context;ILjava/lang/String;)V") {
            // Newer Unity: initJni(Context, int, String). Meaning of the extra args is unknown; passing 0 and "".
            printf("calling initJni(Context,int,String) from libunity.so (%s)\n", initClassName.c_str());
            initJni->invoke(frame2.getJniEnv(), unityPlayerObj.get(), activity, 0, (JString) "");
        } else if (initJni->signature == "(Landroid/content/Context;I)V") {
            // Unity 6.0: initJni(Context, int). As with the 3-argument form, the int's meaning isn't known; passing 0.
            printf("calling initJni(Context,int) from libunity.so (%s)\n", initClassName.c_str());
            initJni->invoke(frame2.getJniEnv(), unityPlayerObj.get(), activity, 0);
        } else if (initJni->signature == "(Landroid/content/Context;)V" || initJni->signature == "(Landroid/content/Context;)Z") {
            printf("calling initJni from libunity.so (%s)\n", initClassName.c_str());
            initJni->invoke(frame2.getJniEnv(), unityPlayerObj.get(), activity);
        } else {
            fprintf(stderr, "Error: initJni on %s has signature %s, which the loader doesn't know how to call.\n", initClassName.c_str(),
                    initJni->signature.c_str());
            return 1;
        }
    }

    // In another thread, start the event loop
    std::thread([&backend]() {
        backend.runEventLoop();
    }).detach();

    // vm.printStatistics();
    // return 0;

    // Newer Unity: render/lifecycle natives are registered on UnityPlayerForActivityOrService instead of UnityPlayer.
    JClass* playerClass = unityClass;
    {
        JClass* c2 = vm.findClass("com/unity3d/player/UnityPlayerForActivityOrService").get();
        if (c2 && c2->getMethod("()Z", "nativeRender")) {
            printf("Using UnityPlayerForActivityOrService for render/lifecycle natives\n");
            playerClass = c2;
        }
    }
    auto unityNRecreateGfxState = playerClass->getMethod("(ILandroid/view/Surface;)V", "nativeRecreateGfxState");
    printf("calling nativeRecreateGfxState from libunity.so\n");
    auto surface = std::make_shared<jnivm::android::view::Surface>();
    LocalFrame frame3(vm);
    auto ret2 = unityNRecreateGfxState.invoke(frame3.getJniEnv(), unityPlayerObj.get(), 0, surface);

    auto unityNRestartACtivityIndicator = playerClass->getMethod("()V", "nativeRestartActivityIndicator");
    if (unityNRestartACtivityIndicator) {
        printf("calling nativeRestartActivityIndicator from libunity.so\n");
        unityNRestartACtivityIndicator.invoke(frame3.getJniEnv(), unityPlayerObj.get());
    }

    auto unityNSendSurfaceChangedEvent = playerClass->getMethod("()V", "nativeSendSurfaceChangedEvent");
    printf("calling nativeSendSurfaceChangedEvent from libunity.so\n");
    unityNSendSurfaceChangedEvent.invoke(frame3.getJniEnv(), unityPlayerObj.get());

    gdb_break_here();

    auto unityNResume = playerClass->getMethod("()V", "nativeResume");
    printf("calling nativeResume from libunity.so\n");
    unityNResume.invoke(frame3.getJniEnv(), unityPlayerObj.get());

    auto unityNFocusChanged = playerClass->getMethod("(Z)V", "nativeFocusChanged");
    printf("calling nativeFocusChanged from libunity.so\n");
    unityNFocusChanged.invoke(frame3.getJniEnv(), unityPlayerObj.get(), true);

    auto unityNRender = playerClass->getMethod("()Z", "nativeRender");
    printf("calling nativeRender from libunity.so\n");
    auto ret3 = unityNRender.invoke(frame3.getJniEnv(), unityPlayerObj.get());

    printf("NativeRender returned %d, Entering loop...\n", ret3.z);
    stage("first nativeRender returned, entering render loop");

    constexpr auto frame_duration = std::chrono::milliseconds(15);
    // The app has created new threads and is happily doing its thing, we just do nothing for now. Eventually, this will be a SDL based event loop for controller input.
    auto unityNPause = playerClass->getMethod("()Z", "nativePause");
    bool paused = false;
    int renderFalseFrames = 0;
    while (!g_quit_requested) {
        // Focus changes are applied here, on the thread that renders, like Android's UI thread would.
        int focusChange = focusState.exchange(-1);
        if (focusChange == 0 && !paused) {
            printf("Window lost focus, pausing\n");
            unityNFocusChanged.invoke(frame3.getJniEnv(), unityPlayerObj.get(), false);
            if (unityNPause)
                unityNPause.invoke(frame3.getJniEnv(), unityPlayerObj.get());
            paused = true;
        } else if (focusChange == 1 && paused) {
            printf("Window regained focus, resuming\n");
            unityNResume.invoke(frame3.getJniEnv(), unityPlayerObj.get());
            unityNFocusChanged.invoke(frame3.getJniEnv(), unityPlayerObj.get(), true);
            paused = false;
        }
        if (paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        // printf(".");
        fflush(stdout);
        // auto start = std::chrono::steady_clock::now();
        auto ret4 = unityNRender.invoke(frame3.getJniEnv(), unityPlayerObj.get());
        // Android's UnityPlayer finishes the activity when nativeRender() returns false: that's how
        // Application.Quit() arrives on newer Unity (it doesn't call Activity.finish() or exit() itself).
        if (!ret4.z) {
            if (++renderFalseFrames == 2)
                request_quit("nativeRender returned false (Application.Quit)", true);
        } else {
            renderFalseFrames = 0;
        }
        // auto end = std::chrono::steady_clock::now();
        // auto elapsed = end - start;
        // if (elapsed < frame_duration) {
        //     std::this_thread::sleep_for(frame_duration - elapsed);
        // }
    }

    // Quit requested. The exit thread started by request_quit() ends the process; this is the orderly part.
    stage("quit: render loop stopped");
    if (g_quit_from_guest) {
        // The game ran its own shutdown before asking to quit, so calling Unity again would only risk blocking
        // on an engine that is already gone. Just wait for the exit thread (it flushes and leaves).
        std::this_thread::sleep_for(std::chrono::seconds(10));
    } else {
        // Host-initiated (signal / window closed): let Unity pause and tear down, like Android's onPause/onDestroy,
        // so it saves its state. The exit thread's longer fallback timer covers a hang.
        if (auto m = playerClass->getMethod("()Z", "nativePause"))
            m.invoke(frame3.getJniEnv(), unityPlayerObj.get());
        stage("quit: paused");
        if (auto m = playerClass->getMethod("()Z", "nativeDone"))
            m.invoke(frame3.getJniEnv(), unityPlayerObj.get());
        stage("quit: done");
    }
    bogo_exit(0); // skips static destructors: guest threads are still running and can abort during teardown

}
