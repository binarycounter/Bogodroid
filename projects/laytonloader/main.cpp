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
#include "javastubs/layton.h"
#include <fstream>
#include <fcntl.h>
#include <stdlib.h>
#include "platform.h"
#include "so_util.h"
#include "io_util.h"
#include "logging.h"

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

// libll1.so's native entry points, resolved by JNI naming convention
// (Java_com_Level5_LT1R_MainActivity_<method>) -- same symbols the Switch
// homebrew port (reference/layton_nx-main) calls directly by address.
typedef void (*setViewSize_t)(JNIEnv *, jobject, jint, jint);
typedef void (*resume_t)(JNIEnv *, jobject);
typedef void (*render_t)(JNIEnv *, jobject, jint, jint, jint, jfloat, jfloat, jfloat, jfloat);

int main(int argc, char *argv[])
{
    print_backtrace_on_segfault(); // Registers a signal handler to print backtrace on segfaults
    exit_on_signals();             // Exits when CTRL-C is pressed (or SIGINT or SIGTERM is received)

    if (argc < 2)
    {
        fatal_error("Usage: %s <config file>\n", argv[0]);
        return -1;
    }

    // Init config (also chdir's into paths.game_files), GLES pointers, JNI VM and bindings
    init_config(argv[1]);
    sdl_initialize_gles();
    Baron::Jvm vm;
    InitJNIBinding(&vm);

    // Load the main so file
    printf("Loading libll1\n");
    so_module lmain = {};
    uintptr_t addr_lmain = 0x50000000;
    const char *path_lmain = "libll1.so";
    if (!load_so_from_file(&lmain, path_lmain, addr_lmain))
    {
        printf("Failed to load libll1.so.\n");
        return 1;
    }

    FakeJni::LocalFrame frame(vm);
    JNIEnv *env = &frame.getJniEnv();

    printf("calling JNI_OnLoad from libll1.so\n");
    auto jniOnLoad = (jint(*)(JavaVM *, void *))(so_symbol(&lmain, "JNI_OnLoad"));
    if (jniOnLoad)
        jniOnLoad(&vm, nullptr);

    auto setViewSize = (setViewSize_t)(so_symbol(&lmain, "Java_com_Level5_LT1R_MainActivity_setViewSize"));
    auto gameResume = (resume_t)(so_symbol(&lmain, "Java_com_Level5_LT1R_MainActivity_resume"));
    auto gameRender = (render_t)(so_symbol(&lmain, "Java_com_Level5_LT1R_MainActivity_render"));

    printf("setViewSize=%p resume=%p render=%p\n", (void *)setViewSize, (void *)gameResume, (void *)gameRender);
    if (!setViewSize || !gameResume || !gameRender)
    {
        printf("Missing one or more MainActivity entry points -- check the exported symbol names above.\n");
        return 1;
    }

    int viewWidth = config["device"]["displayWidth"].value_or<int>(640);
    int viewHeight = config["device"]["displayHeight"].value_or<int>(480);

    printf("calling setViewSize(%d, %d)\n", viewWidth, viewHeight);
    setViewSize(env, nullptr, viewWidth, viewHeight);

    printf("calling resume\n");
    gameResume(env, nullptr);

    printf("Entering render loop\n");
    bool running = true;
    while (running)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
                running = false;
        }

        // frame_step=1, unused=0, touch_num=0, no touch points yet
        gameRender(env, nullptr, 1, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    }

    printf("Exit.\n");
    return 0;
}
