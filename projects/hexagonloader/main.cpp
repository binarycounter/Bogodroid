#include "globals.h"
#include <cstdlib>
#include <execinfo.h>
#include <iostream>

#include "toml++/toml.hpp"
toml::table config;
#include "config.h"

#include "io_util.h"
#include "javastubs/binding.h"
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
    sdl_initialize_gles();
    InitJNIBinding(&vm);

    printf("Loading libc++\n");
    so_module lcpp = {};
    uintptr_t addr_lcpp = 0x4000000000;
    const char* path_lcpp = "lib/arm64-v8a/libc++_shared.so";
    if (!load_so_from_file(&lcpp, path_lcpp, addr_lcpp)) {
        return 1;
    }
    loaded_modules[0] = &lcpp;

    printf("Loading oboe\n");
    so_module loboe = {};
    uintptr_t addr_loboe = 0x5000000000;
    const char* path_loboe = "lib/arm64-v8a/liboboe.so";
    if (!load_so_from_file(&loboe, path_loboe, addr_loboe)) {
        return 1;
    }

    loaded_modules[1] = &loboe;

    printf("Loading openframeworks\n");
    so_module lopenfw = {};
    uintptr_t addr_lopenfw = 0x6000000000;
    const char* path_lopenfw = "lib/arm64-v8a/libopenFrameworksAndroid.so";
    if (!load_so_from_file(&lopenfw, path_lopenfw, addr_lopenfw)) {
        return 1;
    }

    loaded_modules[2] = &lopenfw;

    printf("Loading libsuperhexagon\n");
    so_module lhexagon = {};
    uintptr_t addr_lhexagon = 0x7000000000;
    const char* path_lhexagon = "lib/arm64-v8a/libsuperhexagon.so";
    if (!load_so_from_file(&lhexagon, path_lhexagon, addr_lhexagon)) {
        return 1;
    }

    loaded_modules[3] = &lhexagon;

    printf("calling JNI_OnLoad from libopenframeworks\n");
    auto openfwJNI_OnLoad = (jint (*)(JavaVM* vm, void* reserved))(so_symbol(&lopenfw, "JNI_OnLoad"));
    auto openfwJNI_OnLoadResult = openfwJNI_OnLoad(&vm, nullptr);

    auto assetManager = std::make_shared<jnivm::android::content::res::AssetManager>();
    auto openfwSet_AssetManager  = (jint (*)(JNIEnv* vm, void* reserved, jnivm::android::content::res::AssetManager* assetManager))(so_symbol(&lopenfw, "Java_cc_openframeworks_OFAndroid_setAssetManager"));
    LocalFrame frame(vm);
    openfwSet_AssetManager(&frame.getJniEnv(), nullptr, assetManager.get());
    
    auto hexagon_onCreate = (void (*)(void))so_symbol(&lhexagon, "Java_cc_openframeworks_OFAndroid_onCreate");
    hexagon_onCreate();

    printf("Exit.\n");
    return 0;
}
