#include "globals.h"
#include <cstdlib>
#include <execinfo.h>
#include <iostream>

#include "toml++/toml.hpp"
toml::table config;
#include "config.h"

#include "io_util.h"
#include "javastubs/binding.h"
#include "monocompat/monobridge.h"
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

#include "debug_utils.h"
#include "egl_sdl.h"
#include "glad.h"
#include "glad_egl.h"
#include "gles2.h"
#include "input_backend.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_hints.h>

thread_local int tls0[2 << 12] = {};
int foo() { return tls0[0]++; }

using namespace FakeJni;

extern DynLibFunction symtable_monobridge[];
extern DynLibFunction symtable_libc[];
extern DynLibFunction symtable_ndk[];
extern DynLibFunction symtable_gles2[];
extern DynLibFunction symtable_egl_sdl[];

DynLibFunction* so_static_patches[32] = {
    NULL,
};

DynLibFunction* so_dynamic_libraries[32] = {
    symtable_libc,
    symtable_ndk,
    symtable_egl_sdl,
    symtable_gles2,
    NULL
};

so_module* loaded_modules[32] = {
    NULL
};

extern SDL_Window* sdl_win;
extern SDL_GLContext sdl_ctx;
extern EGLDisplay egl_display;
extern EGLContext egl_context;
extern EGLSurface egl_surface;

Baron::Jvm vm;

#pragma GCC push_options
#pragma GCC optimize("O0")
void gdb_break_here()
{
}
#pragma GCC pop_options

int main(int argc, char* argv[])
{
    print_backtrace_on_segfault(); // Registers a signal handler to print backtrace on segfaults
    exit_on_signals(); // Exits when CTRL-C is presset (or SIGINT or SIGTERM is received)

    if (argc < 2) {
        fatal_error("Usage: %s <config file>\n", argv[0]);
        return -1;
    }

    // Init config, GLES pointers, JNI VN and bindings
    init_config(argv[1]);
    // sdl_initialize_gles();
    InitJNIBinding(&vm);

    int module_count = 0;

    printf("Loading libc++\n");
    so_module lcpp = {};
    uintptr_t addr_lcpp = 0x3100000000;
    const char* path_lcpp = "lib/arm64-v8a/libc++_shared.so";
    if (!load_so_from_file(&lcpp, path_lcpp, addr_lcpp)) {
        printf("No libhelp found\n");
    }

    loaded_modules[module_count++] = &lcpp;

    printf("Loading libmain\n");
    so_module lmain = {};
    uintptr_t addr_lmain = 0x3200000000;
    const char* path_lmain = "lib/arm64-v8a/libmain.so";
    if (!load_so_from_file(&lmain, path_lmain, addr_lmain)) {
        return 1;
    }

    loaded_modules[module_count++] = &lmain;

    printf("Loading libil2cpp\n");
    so_module lil2cpp = {};
    uintptr_t addr_lil2cpp = 0x3600000000;
    const char* path_lil2cpp = "lib/arm64-v8a/libil2cpp.so";
    if (!load_so_from_file(&lil2cpp, path_lil2cpp, addr_lil2cpp)) {
        printf("il2cpp not found, trying libmono\n");
        const char* path_mono = "lib/arm64-v8a/libmonobdwgc-2.0.so";
        if (!load_so_from_file(&lil2cpp, path_mono, addr_lil2cpp)) {
            return 1;
        }
        printf("Loading libMonoPosixHelper.so\n");
        so_module lposix = {};
        uintptr_t addr_lposix = 0x3700000000;
        const char* path_lposix = "lib/arm64-v8a/libMonoPosixHelper.so";
        if (!load_so_from_file(&lposix, path_lposix, addr_lposix)) {
            return 1;
        }
        loaded_modules[module_count++] = &lposix;

        so_dynamic_libraries[4] = symtable_monobridge;
        so_dynamic_libraries[5] = NULL;
        monobridge_init(&lil2cpp);
    }
    loaded_modules[module_count++] = &lil2cpp;

    printf("Loading libunity\n");
    so_module lunity = {};
    uintptr_t addr_lunity = 0x3800000000;
    const char* path_lunity = "lib/arm64-v8a/libunity.so";
    if (!load_so_from_file(&lunity, path_lunity, addr_lunity)) {
        return 1;
    }
    loaded_modules[module_count++] = &lunity;

    printf("Loading libburst\n");
    so_module lburst = {};
    uintptr_t addr_lburst = 0x4000000000;
    const char* path_lburst = "lib/arm64-v8a/lib_burst_generated.so";
    if (!load_so_from_file(&lburst, path_lburst, addr_lburst)) {
        printf("No libburst found\n");
    }

    printf("Loading libUnityHelp\n");
    so_module lhelpers = {};
    uintptr_t addr_lhelpers = 0x4200000000;
    const char* path_lhelpers = "lib/arm64-v8a/libUnityHelpers_Android.so";
    if (!load_so_from_file(&lhelpers, path_lhelpers, addr_lhelpers)) {
        printf("No libhelp found\n");
    }

    loaded_modules[module_count++] = &lhelpers;

    loaded_modules[module_count++] = &lburst;

    printf("calling JNI_OnLoad from libmain.so\n");
    auto mainJNI_OnLoad = (jint (*)(JavaVM* vm, void* reserved))(so_symbol(&lmain, "JNI_OnLoad"));
    mainJNI_OnLoad(&vm, nullptr);

    JClass* nativeLoaderClass = vm.findClass("com/unity3d/player/NativeLoader").get();
    LocalFrame frame(vm);
    auto mainLoad = nativeLoaderClass->getMethod("(Ljava/lang/String;)Z", "load");

    printf("calling com/unity3d/player/NativeLoader/load from libmain.so\n");
    jvalue ret = mainLoad.invoke(frame.getJniEnv(), nativeLoaderClass, (JString) "lib/arm64-v8a");
    if (!ret.z) {
        printf("libmain.so:load returned false, game could not be loaded\n");
        return 1;
    }

    printf("calling JNI_OnLoad from libil2cpp.so\n");
    auto il2cppJNI_OnLoad = (jint (*)(JavaVM* vm, void* reserved))(so_symbol(&lil2cpp, "JNI_OnLoad"));
    std::cout << &il2cppJNI_OnLoad << std::endl;
    il2cppJNI_OnLoad(&vm, nullptr);

    printf("calling JNI_OnLoad from libunity.so\n");
    auto unityJNI_OnLoad = (jint (*)(JavaVM* vm, void* reserved))(so_symbol(&lunity, "JNI_OnLoad"));
    std::cout << &unityJNI_OnLoad << std::endl;
    unityJNI_OnLoad(&vm, nullptr);

    JClass* unityClass = vm.findClass("com/unity3d/player/UnityPlayer").get();

    auto unityInitJni = unityClass->getMethod("(Landroid/content/Context;)V", "initJni");
    printf("calling initJni from libunity.so\n");
    auto activity = std::make_shared<jnivm::android::app::Activity>();
    LocalFrame frame2(vm);
    unityInitJni.invoke(frame2.getJniEnv(), unityClass, activity);

    auto unityActivity = std::make_shared<jnivm::com::unity3d::player::UnityPlayerActivity>();
    auto& backend = InputBackend::instance();

    backend.setKeyCallback([unityActivity](std::shared_ptr<jnivm::android::view::KeyEvent> event) {
        unityActivity->injectEvent(event);
    });

    backend.setMotionCallback([unityActivity](std::shared_ptr<jnivm::android::view::MotionEvent> event) {
        unityActivity->injectEvent(event);
    });

    // In another thread, start the event loop
    std::thread([&backend]() {
        backend.runEventLoop();
    }).detach();

    // vm.printStatistics();
    // return 0;

    auto unityNRecreateGfxState = unityClass->getMethod("(ILandroid/view/Surface;)V", "nativeRecreateGfxState");
    printf("calling nativeRecreateGfxState from libunity.so\n");
    auto surface = std::make_shared<jnivm::android::view::Surface>();
    LocalFrame frame3(vm);
    auto ret2 = unityNRecreateGfxState.invoke(frame3.getJniEnv(), unityClass, 0, surface);

    auto unityNRestartACtivityIndicator = unityClass->getMethod("()V", "nativeRestartActivityIndicator");
    printf("calling nativeRestartActivityIndicator from libunity.so\n");
    unityNRestartACtivityIndicator.invoke(frame3.getJniEnv(), unityClass);

    auto unityNSendSurfaceChangedEvent = unityClass->getMethod("()V", "nativeSendSurfaceChangedEvent");
    printf("calling nativeSendSurfaceChangedEvent from libunity.so\n");
    unityNSendSurfaceChangedEvent.invoke(frame3.getJniEnv(), unityClass);

    auto unityNResume = unityClass->getMethod("()V", "nativeResume");
    printf("calling nativeResume from libunity.so\n");
    unityNResume.invoke(frame3.getJniEnv(), unityClass);

    auto unityNFocusChanged = unityClass->getMethod("(Z)V", "nativeFocusChanged");
    printf("calling nativeFocusChanged from libunity.so\n");
    unityNFocusChanged.invoke(frame3.getJniEnv(), unityClass, true);

    auto unityNRender = unityClass->getMethod("()Z", "nativeRender");
    printf("calling nativeRender from libunity.so\n");
    auto ret3 = unityNRender.invoke(frame3.getJniEnv(), unityClass);

    printf("NativeRender returned %d, Entering loop...\n", ret3.z);

    constexpr auto frame_duration = std::chrono::milliseconds(15);
    // The app has created new threads and is happily doing its thing, we just do nothing for now. Eventually, this will be a SDL based event loop for controller input.
    while (1) {
        // printf(".");
        fflush(stdout);
        auto start = std::chrono::steady_clock::now();
        auto ret4 = unityNRender.invoke(frame3.getJniEnv(), unityClass);
        auto end = std::chrono::steady_clock::now();
        auto elapsed = end - start;
        if (elapsed < frame_duration) {
            std::this_thread::sleep_for(frame_duration - elapsed);
        }
    }

    printf("Exit.\n");
    return 0;
}
