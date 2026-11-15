// Callbacks from Java proxies into the engine (projects/unityloader/javastubs/jnibridge.cpp).
// Unity has two kinds of proxy with different natives behind them; using the wrong one jumped into garbage (a C#
// listener's GC handle was treated as a C++ object). This checks that each kind reaches its own native, that a
// C++ proxy whose handle isn't an object is refused rather than called, and that callbacks don't pile up memory.
#include "stress_util.h"
#include "jnibridge.h"
#include "playbilling.h"
#include "so_util.h"
#include "toml++/toml.hpp"
#include "java_security.h"
#include <cstring>
#include <string>

// ---- what the code under test needs from the rest of the loader ----
toml::table config;
Baron::Jvm vm;
void request_quit(const char*, bool) { }
std::string bogo_host_time_zone() { return "UTC"; }
void InitJNISecurityClasses(FakeJni::Jvm*) { }
std::shared_ptr<jnivm::java::io::File> jnivm::android::content::Context::getExternalFilesDirInternal() { return nullptr; }
BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::Choreographer::FrameCallback)
END_NATIVE_DESCRIPTOR
BEGIN_NATIVE_DESCRIPTOR(jnivm::android::hardware::input::InputManager::InputDeviceListener)
END_NATIVE_DESCRIPTOR

// The loaded-library list the handle check consults: either empty (nothing counts as engine code/data) or one fake
// module spanning the whole address space (everything does).
static bool g_everythingLoaded = false;
static so_module g_everything;
extern "C" so_module* so_get_head()
{
    if (!g_everythingLoaded) return nullptr;
    g_everything.text_base = 0;
    g_everything.text_size = ~(size_t)0;
    g_everything.n_data = 0;
    g_everything.next = nullptr;
    return &g_everything;
}
extern "C" so_module* so_load_library(const char*, void*) { return nullptr; }

// ---- stand-ins for libunity's natives ----
static std::atomic<int> g_reflectionCalls{0}, g_bridgeCalls{0};
static long g_lastHandle = 0;
static std::string g_lastMethod;
static jsize g_lastArgCount = -1;

static jobject fake_nativeProxyInvoke(JNIEnv* env, jclass, jlong handle, jstring name, jobjectArray args)
{
    g_lastHandle = (long)handle;
    g_lastMethod = name ? env->GetStringUTFChars(name, nullptr) : "(null)";
    g_lastArgCount = args ? env->GetArrayLength(args) : -1;
    g_reflectionCalls++;
    return nullptr;
}
static jobject fake_jnibridgeInvoke(JNIEnv*, jclass, jlong handle, jclass, jobject, jobjectArray)
{
    g_lastHandle = (long)handle;
    g_bridgeCalls++;
    return nullptr;
}

static void register_natives()
{
    FakeJni::LocalFrame frame(vm);
    JNIEnv* e = &frame.getJniEnv();
    JNINativeMethod proxyInvoke = { (char*)"nativeProxyInvoke", (char*)"(JLjava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;", (void*)fake_nativeProxyInvoke };
    e->RegisterNatives(e->FindClass("com/unity3d/player/ReflectionHelper"), &proxyInvoke, 1);
    JNINativeMethod bridgeInvoke = { (char*)"invoke", (char*)"(JLjava/lang/Class;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;", (void*)fake_jnibridgeInvoke };
    e->RegisterNatives(e->FindClass("bitter/jnibridge/JNIBridge"), &bridgeInvoke, 1);
}

using jnivm::bitter::jnibridge::JNIBridgeProxy;

static void reset()
{
    g_reflectionCalls = g_bridgeCalls = 0;
    g_lastHandle = 0;
    g_lastMethod.clear();
    g_lastArgCount = -1;
}

// A C# AndroidJavaProxy (made by ReflectionHelper.newProxyInstance) goes to nativeProxyInvoke with the method name.
static void test_csharp_proxy_routing(double)
{
    g_everythingLoaded = false; // the C# path must not depend on the handle being a C++ object
    auto p = std::make_shared<JNIBridgeProxy>(0x1234, std::set<std::string>{ "java/lang/Runnable", "android/os/Handler$Callback",
                                                                     "android/view/Choreographer$FrameCallback" }, true);
    reset();
    p->run();
    CHECK(g_reflectionCalls == 1 && g_bridgeCalls == 0);
    CHECK(g_lastHandle == 0x1234 && g_lastMethod == "run" && g_lastArgCount == 0);
    reset();
    p->doFrame(123456789);
    CHECK(g_reflectionCalls == 1 && g_lastMethod == "doFrame" && g_lastArgCount == 1);
    reset();
    p->handleMessage(jnivm::android::os::Message::obtain());
    CHECK(g_reflectionCalls == 1 && g_lastMethod == "handleMessage" && g_lastArgCount == 1);
}

// Play Billing's startConnection calls the C# listener back with BILLING_UNAVAILABLE.
static void test_billing_listener(double)
{
    g_everythingLoaded = false;
    auto listener = std::make_shared<JNIBridgeProxy>(0x5678, std::set<std::string>{ "com/android/billingclient/api/BillingClientStateListener" }, true);
    auto client = std::make_shared<jnivm::com::android::billingclient::api::BillingClient>();
    reset();
    client->startConnection(listener);
    CHECK(g_reflectionCalls == 1 && g_bridgeCalls == 0);
    CHECK(g_lastHandle == 0x5678 && g_lastMethod == "onBillingSetupFinished" && g_lastArgCount == 1);
}

struct FakeProxyObject {
    virtual ~FakeProxyObject() = default;
    virtual void invoke() { }
};

// Unity's own C++ proxies go through JNIBridge.invoke, but only if the handle really is an object.
static void test_cpp_proxy_routing(double)
{
    FakeProxyObject object;
    auto good = std::make_shared<JNIBridgeProxy>((long)&object, std::set<std::string>{ "android/view/Choreographer$FrameCallback" });
    g_everythingLoaded = true;
    reset();
    good->doFrame(1);
    CHECK(g_bridgeCalls == 1 && g_reflectionCalls == 0 && g_lastHandle == (long)&object);

    // A handle whose first word isn't a vtable inside a loaded library is refused, not called.
    void* notAnObject[4] = { (void*)0x1, nullptr, nullptr, nullptr };
    auto bad = std::make_shared<JNIBridgeProxy>((long)notAnObject, std::set<std::string>{ "android/view/Choreographer$FrameCallback" });
    g_everythingLoaded = false;
    reset();
    bad->doFrame(1);
    CHECK(g_bridgeCalls == 0);
}

// Callbacks fire hundreds of times a second: none of what they create may stay behind.
static void test_callbacks_dont_accumulate(double secs)
{
    g_everythingLoaded = true;
    FakeProxyObject object;
    auto cpp = std::make_shared<JNIBridgeProxy>((long)&object, std::set<std::string>{ "android/view/Choreographer$FrameCallback" });
    auto cs = std::make_shared<JNIBridgeProxy>(0x99, std::set<std::string>{ "java/lang/Runnable" }, true);
    FakeJni::LocalFrame frame(vm);
    auto* env = jnivm::ENV::FromJNIEnv(&frame.getJniEnv());
    cpp->doFrame(1);
    cs->run();
    size_t before = env->localframe.front().size();
    for (int i = 0; i < 2000; i++) {
        cpp->doFrame(i);
        cs->run();
    }
    CHECK(env->localframe.front().size() == before);
}

int main()
{
    register_natives();
    RUN(test_csharp_proxy_routing);
    RUN(test_billing_listener);
    RUN(test_cpp_proxy_routing);
    RUN(test_callbacks_dont_accumulate);
    return g_failures ? 1 : 0;
}
