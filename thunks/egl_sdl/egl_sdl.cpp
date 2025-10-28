#include "egl_sdl.h"
#include "SDL2/SDL.h"
#include "glad_egl.h"
#include "gles2.h"
#include "logging.h"
#include "platform.h"
#include "so_util.h"
#include "thunk_gen.h"
#include <chrono>
#include <inttypes.h>
#include <memory>
#include <dlfcn.h>

SDL_Window* sdl_win;
SDL_GLContext sdl_ctx;
EGLDisplay egl_display;
EGLContext egl_context;
EGLSurface egl_surface;

namespace jnivm::android::view {
class Choreographer;

class Choreographer {
public:
    static std::shared_ptr<Choreographer> getInstance();
    void signalVSync();
};
}

void* getProc(const char* sym)
{
    // First try SDL
    void* proc=SDL_GL_GetProcAddress(sym);
    if(proc)
        return proc;
    
    // Some platforms don't expose EGL over SDL, so try dynamic linking
    static void* libEGL_handle=dlopen("libEGL.so", RTLD_NOW);
    proc=dlsym(libEGL_handle, sym);
    if(proc)
       return proc;

    return NULL;
}

EGLBoolean eglSwapBuffers_impl(EGLDisplay display,
    EGLSurface surface)
{
    SDL_GL_MakeCurrent(sdl_win, sdl_ctx);
    SDL_GL_SwapWindow(sdl_win);

    using namespace std::chrono;

    // Persist across calls
    static int frameCount = 0;
    static auto lastTime = high_resolution_clock::now();
    static float fps = 0.0f;

    frameCount++;
    auto now = high_resolution_clock::now();
    duration<float> elapsed = now - lastTime;

    if (elapsed.count() >= 1.0f) {
        fps = frameCount / elapsed.count();
        frameCount = 0;
        lastTime = now;
        warning("FPS: %f\n", fps);
    }
    // verbose("EGL_SDL", "[Thread: %" PRIxPTR "] eglSwapBuffers about to call getInstance().", (uintptr_t)pthread_self());

    auto choreographer = jnivm::android::view::Choreographer::getInstance();
    if (choreographer) {
        // choreographer->dispatchFrameCallbacks(true);
        choreographer->signalVSync();
    }
    return EGL_TRUE;
}

// Just return the current display
EGLDisplay eglGetDisplay_impl(NativeDisplayType native_display)
{
    printf("[NATIVE] eglGetDisplay\n");
    if (egl_display)
        return egl_display;

    // Initialize SDL with video, audio, joystick, and controller support
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fatal_error("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        // return -1;
    }

    sdl_win = SDL_CreateWindow("Teapot", 0, 0, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (sdl_win == NULL) {
        fatal_error("Failed to create SDL Window: %s\n", SDL_GetError());
        // return -1;
    }

    // Basic OpenGL ES 2.x setup
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    sdl_ctx = SDL_GL_CreateContext(sdl_win);
    if (sdl_ctx == NULL) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        sdl_ctx = SDL_GL_CreateContext(sdl_win);
        if (sdl_ctx == NULL) {
            fatal_error("Failed to create OpenGL Context: %s\n", SDL_GetError());
        }
        // return -1;
    }
    SDL_GL_MakeCurrent(sdl_win, sdl_ctx);

    //

    egl_display = ((EGLDisplay (*)())getProc("eglGetCurrentDisplay"))();
    egl_context = ((EGLDisplay (*)())getProc("eglGetCurrentContext"))();
    egl_surface = ((EGLSurface (*)(EGLint))getProc("eglGetCurrentSurface"))(EGL_DRAW);

    load_egl_funcs();
    load_gles2_funcs();

    // Print OpenGL information
    const char* glVersion = (const char*)glad_glGetString(GL_VERSION);
    const char* glVendor = (const char*)glad_glGetString(GL_VENDOR);
    const char* glRenderer = (const char*)glad_glGetString(GL_RENDERER);
    const char* glExtensions = (const char*)glad_glGetString(GL_EXTENSIONS);

    if (glVersion) {
        printf("OpenGL Version: %s\n", glVersion);
    } else {
        fatal_error("Failed to retrieve OpenGL version.\n");
    }

    if (glVendor) {
        printf("OpenGL Vendor: %s\n", glVendor);
    } else {
        fatal_error("Failed to retrieve OpenGL vendor.\n");
    }

    if (glRenderer) {
        printf("OpenGL Renderer: %s\n", glRenderer);
    } else {
        fatal_error("Failed to retrieve OpenGL renderer.\n");
    }

    if (glExtensions) {
        printf("OpenGL Extensions: %s\n", glExtensions);
    } else {
        fatal_error("Failed to retrieve OpenGL extensions.\n");
    }

    // Just for good measure
    SDL_GL_SwapWindow(sdl_win);
    SDL_GL_SwapWindow(sdl_win);
    SDL_GL_SwapWindow(sdl_win);
    SDL_GL_SwapWindow(sdl_win);
    SDL_GL_SwapWindow(sdl_win);

    return egl_display;
}

// Do not actually initialize, just return the EGL version number.
EGLBoolean eglInitialize_impl(EGLDisplay display, int* major, int* minor)
{
    verbose("EGL_SDL", "eglInitialize\n");
#ifdef FAKE_EGL
    if (major != NULL)
        *major = 1;
    if (minor != NULL)
        *minor = 4;
    return EGL_TRUE;
#endif

    if (!egl_display)
        eglGetDisplay_impl(NULL);

    int temp_major = 0, temp_minor = 0;
    const char* versionString = ((const char* (*)(EGLDisplay, EGLint))getProc("eglQueryString"))(display, EGL_VERSION);
    if (!versionString) {
        fatal_error("Failed to retrieve EGL version string.\n");
        return EGL_FALSE;
    }

    if (sscanf(versionString, "%d.%d", &temp_major, &temp_minor) != 2) {
        fatal_error("Failed to parse EGL version string: %s\n", versionString);
        return EGL_FALSE;
    }

    if (major != NULL)
        *major = temp_major;
    if (minor != NULL)
        *minor = temp_minor;

    return EGL_TRUE;
}

// Do not actually search for configs. Just always return the config that the current context uses
EGLBoolean eglChooseConfig_impl(EGLDisplay display, const EGLint* attribList, EGLConfig* configs, EGLint configSize, EGLint* numConfigs)
{
    verbose("EGL_SDL", "eglChooseConfig\n");
#ifdef FAKE_EGL
    *configs = malloc(1 * sizeof(EGLConfig));
    *numConfigs = 1;
    return EGL_TRUE;
#endif

    // Inline fetching of eglGetCurrentContext
    EGLContext context = egl_context;
    if (context == EGL_NO_CONTEXT) {
        fatal_error("Failed to get current EGLContext.\n");
        return EGL_FALSE;
    }

    EGLint configID;
    if (!((EGLBoolean (*)(EGLDisplay, EGLContext, EGLint, EGLint*))getProc("eglQueryContext"))(display, context, EGL_CONFIG_ID, &configID)) {
        fatal_error("Failed to query EGL_CONFIG_ID.\n");
        return EGL_FALSE;
    }

    EGLint totalConfigs;
    if (!((EGLBoolean (*)(EGLDisplay, EGLConfig*, EGLint, EGLint*))getProc("eglGetConfigs"))(display, NULL, 0, &totalConfigs)) {
        fatal_error("Failed to get the number of EGLConfigs.\n");
        return EGL_FALSE;
    }

    EGLConfig* allConfigs = (EGLConfig*)malloc(totalConfigs * sizeof(EGLConfig));
    if (!((EGLBoolean (*)(EGLDisplay, EGLConfig*, EGLint, EGLint*))getProc("eglGetConfigs"))(display, allConfigs, totalConfigs, &totalConfigs)) {
        fatal_error("Failed to retrieve EGLConfigs.\n");
        free(allConfigs);
        return EGL_FALSE;
    }

    // eglGetConfigAttrib to find the matching config
    EGLConfig matchingConfig = NULL;
    for (EGLint i = 0; i < totalConfigs; i++) {
        EGLint id;
        if (((EGLBoolean (*)(EGLDisplay, EGLConfig, EGLint, EGLint*))getProc("eglGetConfigAttrib"))(display, allConfigs[i], EGL_CONFIG_ID, &id) && id == configID) {
            matchingConfig = allConfigs[i];
            break;
        }
    }
    free(allConfigs);

    if (!matchingConfig) {
        fatal_error("Failed to find a matching EGLConfig.\n");
        return EGL_FALSE;
    }

    // Populate the results
    if (configs && configSize > 0) {
        configs[0] = matchingConfig;
    }
    if (numConfigs) {
        *numConfigs = 1; // Always return exactly 1 config
    }

    return EGL_TRUE;
}

EGLSurface eglCreateWindowSurface_impl(EGLDisplay display, EGLConfig config, NativeWindowType native_window, EGLint const* attrib_list)
{
    verbose("EGL_SDL", "eglCreateWindowSurface\n");
#ifdef FAKE_EGL
    return (EGLSurface)0xDEAD;
#endif
    return egl_surface;
}

EGLBoolean eglQuerySurface_impl(EGLDisplay display, EGLSurface surface, EGLint attribute, EGLint* value)
{
    verbose("EGL_SDL", "eglQuerySurface\n");
#ifdef FAKE_EGL
    if (attribute == EGL_WIDTH)
        *value = 640;
    if (attribute == EGL_HEIGHT)
        *value = 480;
    return EGL_TRUE;
#endif
    return ((EGLBoolean (*)(EGLDisplay, EGLSurface, EGLint, EGLint*))getProc("eglQuerySurface"))(display, surface, attribute, value);
}

EGLContext eglCreateContext_impl(EGLDisplay display,
    EGLConfig config,
    EGLContext share_context,
    EGLint const* attrib_list)
{
    verbose("EGL_SDL", "eglCreateContext\n");
#ifdef FAKE_EGL
    return (EGLContext)0xDEAD;
#endif
    return egl_context;
}

EGLBoolean eglDestroyContext_impl(EGLDisplay display,
    EGLContext context)
{
    return EGL_TRUE;
}

EGLBoolean eglDestroySurface_impl(EGLDisplay display,
    EGLSurface surface)
{
    return EGL_TRUE;
}

EGLBoolean eglMakeCurrent_impl(EGLDisplay display,
    EGLSurface draw,
    EGLSurface read,
    EGLContext context)
{
    static auto cached_eglMakeCurrent = (EGLBoolean (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext))getProc("eglMakeCurrent");
    verbose("EGL_SDL", "eglMakeCurrent\n");
    return cached_eglMakeCurrent(display, draw, read, context);
}

EGLint eglGetError_impl()
{
    return EGL_SUCCESS; // TRULY AWFUL
}

EGLBoolean eglGetConfigAttrib_impl(EGLDisplay display,
    EGLConfig config,
    EGLint attribute,
    EGLint* value)
{
    return ((EGLBoolean (*)(EGLDisplay, EGLConfig, EGLint, EGLint*))getProc("eglGetConfigAttrib"))(display, config, attribute, value);
}

char const* eglQueryString_impl(EGLDisplay display,
    EGLint name)
{
    verbose("EGL_SDL", "eglQueryString %d\n", name);
    return ((char const* (*)(EGLDisplay, EGLint))getProc("eglQueryString"))(display, name);
}

EGLDisplay eglGetCurrentDisplay_impl()
{
    return egl_display;
}

EGLContext eglGetCurrentContext_impl()
{
    return egl_context;
}

EGLSurface eglGetCurrentSurface_impl()
{
    return egl_surface;
}

EGLBoolean eglSwapInterval_impl(EGLDisplay display,
    EGLint interval)
{
    return EGL_FALSE; // Generally can't set swap interval on these platforms.
}

// Actually implemented in egl.cpp
ABI_ATTR __eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress_impl(const char* procname);

DynLibFunction symtable_egl_sdl[] = {
    NO_THUNK("eglSwapBuffers", (uintptr_t)&eglSwapBuffers_impl),
    NO_THUNK("eglGetDisplay", (uintptr_t)&eglGetDisplay_impl),
    NO_THUNK("eglInitialize", (uintptr_t)&eglInitialize_impl),
    NO_THUNK("eglChooseConfig", (uintptr_t)&eglChooseConfig_impl),
    NO_THUNK("eglCreateWindowSurface", (uintptr_t)&eglCreateWindowSurface_impl),
    NO_THUNK("eglQuerySurface", (uintptr_t)&eglQuerySurface_impl),
    NO_THUNK("eglCreateContext", (uintptr_t)&eglCreateContext_impl),
    NO_THUNK("eglMakeCurrent", (uintptr_t)&eglMakeCurrent_impl),
    NO_THUNK("eglGetError", (uintptr_t)&eglGetError_impl),
    NO_THUNK("eglGetConfigAttrib", (uintptr_t)&eglGetConfigAttrib_impl),
    NO_THUNK("eglDestroyContext", (uintptr_t)&eglDestroyContext_impl),
    NO_THUNK("eglDestroySurface", (uintptr_t)&eglDestroySurface_impl),
    NO_THUNK("eglQueryString", (uintptr_t)&eglQueryString_impl),
    NO_THUNK("eglGetCurrentDisplay", (uintptr_t)&eglGetCurrentDisplay_impl),
    NO_THUNK("eglGetCurrentContext", (uintptr_t)&eglGetCurrentContext_impl),
    NO_THUNK("eglGetCurrentSurface", (uintptr_t)&eglGetCurrentSurface_impl),
    NO_THUNK("eglSwapInterval", (uintptr_t)&eglSwapInterval_impl),
    NO_THUNK("eglGetProcAddress", (uintptr_t)&eglGetProcAddress_impl),
    { NULL, (uintptr_t)NULL }
};

// Internal use, do not put these in the symtable

void sdl_initialize_gles()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fatal_error("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    }
    sdl_win = SDL_CreateWindow("Loader", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1, 1, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    if (sdl_win == NULL) {
        fatal_error("Failed to create SDL Window: %s\n", SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    sdl_ctx = SDL_GL_CreateContext(sdl_win);
    if (sdl_ctx == NULL) {
        fatal_error("Failed to create OpenGL Context: %s\n", SDL_GetError());
    }

    SDL_GL_DeleteContext(sdl_ctx);
    SDL_DestroyWindow(sdl_win);
    SDL_Quit();
}