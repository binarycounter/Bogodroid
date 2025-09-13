    
#include "../globals.h"
#include "jnibridge.h"
#include "javac.h"
#include "baron/baron.h"
#include "logging.h"
#include "platform.h"
#include <set>
#include <string>

using namespace jnivm::bitter::jnibridge;

// --- JNIBridgeProxy Implementation ---

JNIBridgeProxy::JNIBridgeProxy(long handle, const std::set<std::string>& interfaces)
    : nativeHandle(handle), implementedInterfaces(interfaces) {}

void JNIBridgeProxy::run() {
    // Runtime check: does this instance actually implement Runnable?
    if (implementedInterfaces.count("java/lang/Runnable")) {
        // Yes, so forward the call to the native engine via the invoker.
        JNIBridge::invoke(nativeHandle, "java/lang/Runnable", "run", "()V");
    } else {
        // This should not happen if the engine is well-behaved.
        verbose("JNIBridgeProxy", "run() called on a proxy that doesn't implement java/lang/Runnable!");
    }
}

bool JNIBridgeProxy::handleMessage(std::shared_ptr<jnivm::android::os::Message> msg) {
    // Runtime check: does this instance actually implement Handler.Callback?
    if (implementedInterfaces.count("android/os/Handler$Callback")) {
        // Yes, so forward the call.
        JNIBridge::invoke(nativeHandle, "android/os/Handler$Callback", "handleMessage", "(Landroid/os/Message;)Z", msg);
        // The native invoke probably returns a Boolean object. We'll assume null means 'false'.
        return true;
    } else {
        verbose("JNIBridgeProxy", "handleMessage() called on a proxy that doesn't implement android/os/Handler$Callback!");
        return false;
    }
}

void JNIBridgeProxy::doFrame(jlong frameTimeNanos) {
    if (implementedInterfaces.count("android/view/Choreographer$FrameCallback")) {
        // Note: The native method probably doesn't take an argument. The frame time
        // is usually queried from a native system. We just call the method.
        JNIBridge::invoke(nativeHandle, "android/view/Choreographer$FrameCallback", "doFrame", "(J)V" /* Check signature! */, frameTimeNanos);
    } else {
        verbose("JNIBridgeProxy", "doFrame() called on a proxy that doesn't implement FrameCallback!");
    }
}

// --- JNIBridge Factory and Invoker Implementation ---

std::shared_ptr<jnivm::java::lang::Object> JNIBridge::newInterfaceProxy(FakeJni::JLong j, std::shared_ptr<jnivm::Array<FakeJni::JClass>> classes) {
    
    std::set<std::string> interfaceNames;
    for (int i = 0; i < classes->getSize(); i++) {
        std::string name = (*classes)[i]->getName();
        interfaceNames.insert(name);
        verbose("JBRIDGE", "Requesting proxy to implement: %s", name.c_str());
    }

    // The factory is now trivial. It always creates the same C++ type,
    // just configured with a different set of interfaces to implement.
    auto proxy = std::make_shared<JNIBridgeProxy>(j, interfaceNames);
    return proxy;
}

// The generic, type-safe C++ function that calls back into the native engine.
template<typename... Args>
void JNIBridge::invoke(long nativeHandle, const char* className, const char* methodName, const char* methodSig, Args... args) {
    

    // Find the JNIBridge.invoke method
    auto jniBridgeClass = vm.findClass("bitter/jnibridge/JNIBridge").get();
    auto invokeMethod = jniBridgeClass->getMethod("(JLjava/lang/Class;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;", "invoke");

    // Get the Class and Method objects for the interface method we're proxying
    auto interfaceClass = vm.findClass(className);
    auto interfaceMethod = std::shared_ptr<Method>((Method*)interfaceClass->getMethod(methodSig, methodName));

    // Package the C++ arguments into a Java Object array
    std::shared_ptr<FakeJni::JArray<jnivm::java::lang::Object>> argsArray = std::make_shared<FakeJni::JArray<jnivm::java::lang::Object>>(sizeof...(args));
    int i = 0;

    // We must explicitly cast each argument to the base Object type.
     ( ( (*argsArray)[i++] = autobox(args) ), ... );
    verbose("JNIBridge", "Invoking native handle %ld for %s->%s", nativeHandle, className, methodName);
    FakeJni::LocalFrame frame(vm);
    // The invoke call itself was correct, as you pointed out.
    invokeMethod.invoke(frame.getJniEnv(), jniBridgeClass, nativeHandle, interfaceClass, interfaceMethod, argsArray);
}


// Explicit template instantiation is still required to prevent linker errors.
template void JNIBridge::invoke(long, const char*, const char*, const char*); // For Runnable.run()
template void JNIBridge::invoke(long, const char*, const char*, const char*, std::shared_ptr<jnivm::android::os::Message>); // For Handler.Callback.handleMessage()
template void JNIBridge::invoke(long, const char*, const char*, const char*, jlong); // for FrameCallback.doFrame()


BEGIN_NATIVE_DESCRIPTOR(jnivm::bitter::jnibridge::JNIBridge) { FakeJni::Function<&JNIBridge::newInterfaceProxy> {}, "newInterfaceProxy", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::bitter::jnibridge::JNIBridgeProxy)
        END_NATIVE_DESCRIPTOR
