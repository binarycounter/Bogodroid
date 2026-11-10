// Test plugin for the probe (built by tools/buildprobe.sh with Unity's NDK): exercises the loader's on-demand library
// loading. It needs liblog (an Android system library: thunked) and libprobedep.so (shipped with the game: loaded
// first), runs a constructor, and records whether JNI_OnLoad was called (only System.loadLibrary should do that).
#include <jni.h>
#include <android/log.h>

int probe_dep_value(void);

static int g_constructed = 0;
static int g_jni_loaded = 0;

__attribute__((constructor)) static void probe_plugin_init(void)
{
    g_constructed = 1;
    __android_log_print(ANDROID_LOG_INFO, "ProbePlugin", "constructor ran");
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    g_jni_loaded = 1;
    __android_log_print(ANDROID_LOG_INFO, "ProbePlugin", "JNI_OnLoad called");
    return JNI_VERSION_1_6;
}

__attribute__((visibility("default"))) int probe_plugin_add(int a, int b) { return a + b + probe_dep_value(); }
__attribute__((visibility("default"))) int probe_plugin_state(void) { return g_constructed | (g_jni_loaded << 1); }
