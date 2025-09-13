#include <execinfo.h>
#include <iostream>
#include <cstdlib>

#include "toml++/toml.hpp"
toml::table config;
#include "config.h"

#include <unistd.h>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <baron/baron.h>
#include "javastubs/binding.h"
#include "javastubs/teapot.h"
#include <fstream>
#include <fcntl.h>
#include <stdlib.h>
#include "platform.h"
#include "so_util.h"
#include "io_util.h"

#include "ndk.h"
#include "anative_activity.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_hints.h>
#include "gles2.h"
#include "glad.h"
#include "glad_egl.h"
#include "egl_sdl.h"
#include "debug_utils.h"

using namespace FakeJni;

extern DynLibFunction symtable_libc[];
extern DynLibFunction symtable_ndk[];
extern DynLibFunction symtable_gles2[];
extern DynLibFunction symtable_egl_sdl[];

DynLibFunction *so_static_patches[32] = {
    NULL,
};

DynLibFunction *so_dynamic_libraries[32] = {
    symtable_libc,
    symtable_ndk,
    symtable_gles2,
    symtable_egl_sdl,
    NULL
};

extern SDL_Window *sdl_win;
extern SDL_GLContext sdl_ctx;
extern EGLDisplay egl_display;
extern EGLContext egl_context;
extern EGLSurface egl_surface;

int main(int argc, char *argv[])
{
    print_backtrace_on_segfault(); //Registers a signal handler to print backtrace on segfaults
    exit_on_signals(); //Exits when CTRL-C is presset (or SIGINT or SIGTERM is received)

    if(argc<2)
    {
        fatal_error("Usage: %s <config file>\n",argv[0]);
        return -1;
    }

    // Init config, GLES pointers, JNI VN and bindings
    init_config(argv[1]);
    sdl_initialize_gles();
    Baron::Jvm vm;
    InitJNIBinding(&vm);

    // Load the main so file
    printf("Loading libTeapot\n");
    so_module lmain = {};
    uintptr_t addr_lmain = 0x50000000;
    const char *path_lmain = "lib/arm64-v8a/libTeapotNativeActivity.so";
    if (!load_so_from_file(&lmain, path_lmain, addr_lmain))
    {
    printf("Failed to load libTeapot.\n");
    return 1;
    }

    // Create the ANativeActivity and NativeActivity bindings
    ANativeActivity nActivity = ANativeActivity_create<jnivm::com::sample::teapot::TeapotNativeActivity>(&vm, "assets");

    // Fetch pointer to ANativeActivity_onCreate from the so file...
    printf("calling ANativeActivity_onCreate from libTeapot\n");
    auto mainOnCreate = (jint(*)(ANativeActivity* act, void* savedState, size_t savedStateSize))(so_symbol(&lmain, "ANativeActivity_onCreate")); 
    // and call it. This populates the ANativeActivity Callback struct with function pointers
    mainOnCreate(&nActivity, NULL, 0);

    print_native_callbacks(nActivity);

    //Create a Window
    ANativeWindow nWindow;
    memset( &nWindow, 0, sizeof( ANativeWindow ) );

    //Start calling some of those callback functions, simulating an Android app initializing
    printf("calling ANativeActivity_onNativeWindowCreated from libTeapot\n");
    nActivity.callbacks->onNativeWindowCreated(&nActivity,&nWindow);

    printf("calling ANativeActivity_onWindowFocusChanged from libTeapot\n");
    nActivity.callbacks->onWindowFocusChanged(&nActivity,1);

    printf("calling ANativeActivity_onResume from libTeapot\n");
    nActivity.callbacks->onResume(&nActivity);

    printf("calling ANativeActivity_onStart from libTeapot\n");
    nActivity.callbacks->onStart(&nActivity);

    // The app has created new threads and is happily doing its thing, we just do nothing for now. Eventually, this will be a SDL based event loop for controller input.
    while(1)
        sleep(1);


    printf("Exit.\n");
    return 0;
}
