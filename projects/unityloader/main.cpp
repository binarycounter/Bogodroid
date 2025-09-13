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

// Mangled symbols (from your last message)
static const char* SYM_GetBackgroundJobQueue = "_Z21GetBackgroundJobQueuev";
static const char* SYM_JobQueue_GetPending = "_ZNK8JobQueue14GetPendingJobsEv";
static const char* SYM_AtomicList_Peek = "_ZN10AtomicList4PeekEv";

// Offsets derived from decompiled code
enum {
    // BackgroundJobQueue fields
    BGQ_JOBQUEUE_PTR_OFF = 0x00, // *(BGQ + 0x00) -> JobQueue*

    // JobQueue fields
    JQ_ATOMICQUEUE_PTR_OFF = 0x08, // *(JobQueue + 0x08) -> AtomicQueue* (async pending queue)
    JQ_PENDING_COUNT_OFF = 0x144, // *(JobQueue + 0x144) -> uint32_t pending count

    // AtomicQueue fields
    AQ_HEAD_SLOT_PTR_OFF = 0x00, // *(AtomicQueue + 0x00) -> void** head_slot; head = *head_slot

    // Queue node layout
    NODE_NEXT_OFF = 0x00, // *(node + 0x00) -> next
    GROUP_NODE_OFFSET = 0x38, // queue node is at group + 0x38, so group = node - 0x38

    // Job-info node layout (from ScheduleJob)
    JOBINFO_FUNC_OFF = 0x08, // *(jobinfo + 0x08) = job function/type pointer
    JOBINFO_DATA_OFF = 0x10 // *(jobinfo + 0x10) = job data/context pointer
};

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

    printf("Loading libmain\n");
    so_module lmain = {};
    uintptr_t addr_lmain = 0x4000000000;
    const char* path_lmain = "lib/arm64-v8a/libmain.so";
    if (!load_so_from_file(&lmain, path_lmain, addr_lmain)) {
        return 1;
    }

    printf("Loading libil2cpp\n");
    so_module lil2cpp = {};
    uintptr_t addr_lil2cpp = 0x3000000000;
    const char* path_lil2cpp = "lib/arm64-v8a/libil2cpp.so";
    if (!load_so_from_file(&lil2cpp, path_lil2cpp, addr_lil2cpp)) {
        return 1;
    }

    printf("Loading libunity\n");
    so_module lunity = {};
    uintptr_t addr_lunity = 0x5000000000;
    const char* path_lunity = "lib/arm64-v8a/libunity.so";
    if (!load_so_from_file(&lunity, path_lunity, addr_lunity)) {
        return 1;
    }

    // printf("Loading libburst\n");
    // so_module lburst = {};
    // uintptr_t addr_lburst = 0x7000000000;
    // const char *path_lburst = "lib/arm64-v8a/lib_burst_generated.so";
    // if (!load_so_from_file(&lburst, path_lburst, addr_lburst))
    // {
    //   return 1;
    // }

    loaded_modules[0] = &lmain;
    loaded_modules[1] = &lunity;
    loaded_modules[2] = &lil2cpp;
    // loaded_modules[3]=&lburst;

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

    // Audio stuff, currently unimplemented and sometimes causes a segfault, need to investigate that.
    // JClass* fmodClass = vm.findClass("org/fmod/FMODAudioDevice").get();
    // auto fmodProcess = fmodClass->getMethod("(Ljava/nio/ByteBuffer;)I", "fmodProcess");

    void* buffer = malloc(10000);
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
