    
#include "../globals.h"
#include "jnibridge.h"
#include "javac.h"
#include "baron/baron.h"
#include "logging.h"
#include "breadcrumbs.h"
#include "so_util.h"
#include "localrefs.h"
#include <cstring>
#include "platform.h"
#include <set>
#include <string>

using namespace jnivm::bitter::jnibridge;

// --- JNIBridgeProxy Implementation ---

JNIBridgeProxy::JNIBridgeProxy(long handle, const std::set<std::string>& interfaces, bool reflection)
    : nativeHandle(handle), viaReflectionHelper(reflection), implementedInterfaces(interfaces) {}

void JNIBridgeProxy::run() {
    // Runtime check: does this instance actually implement Runnable?
    if (implementedInterfaces.count("java/lang/Runnable")) {
        // Yes, so forward the call to the native engine via the invoker.
        callInterface("java/lang/Runnable", "run", "()V");
    } else {
        // This should not happen if the engine is well-behaved.
        verbose("JNIBridgeProxy", "run() called on a proxy that doesn't implement java/lang/Runnable!");
    }
}

bool JNIBridgeProxy::handleMessage(std::shared_ptr<jnivm::android::os::Message> msg) {
    // Runtime check: does this instance actually implement Handler.Callback?
    if (implementedInterfaces.count("android/os/Handler$Callback")) {
        // Yes, so forward the call.
        callInterface("android/os/Handler$Callback", "handleMessage", "(Landroid/os/Message;)Z", msg);
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
        callInterface("android/view/Choreographer$FrameCallback", "doFrame", "(J)V", frameTimeNanos);
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
    if (!jniBridgeClass) {
        verbose("JNIBridge", "bitter/jnibridge/JNIBridge isn't known to the VM; dropping %s->%s", className, methodName);
        return;
    }
    auto invokeMethod = jniBridgeClass->getMethod("(JLjava/lang/Class;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;", "invoke");
    if (!invokeMethod) {
        // libunity hasn't registered the native yet (or never does): nothing to call back into.
        verbose("JNIBridge", "JNIBridge.invoke isn't registered; dropping %s->%s", className, methodName);
        return;
    }

    // Get the Class and Method objects for the interface method we're proxying
    auto interfaceClass = vm.findClass(className);
    if (!interfaceClass) {
        verbose("JNIBridge", "Unknown interface class %s; dropping %s", className, methodName);
        return;
    }
    // The class owns its Method objects; this shared_ptr must only borrow it. (Without the no-op deleter the
    // Method was deleted when this call returned, leaving the class with a dangling pointer.)
    auto interfaceMethod = std::shared_ptr<Method>((Method*)interfaceClass->getMethod(methodSig, methodName), [](Method*) { });

    // Package the C++ arguments into a Java Object array
    std::shared_ptr<FakeJni::JArray<jnivm::java::lang::Object>> argsArray = std::make_shared<FakeJni::JArray<jnivm::java::lang::Object>>(sizeof...(args));
    int i = 0;

    // We must explicitly cast each argument to the base Object type.
     ( ( (*argsArray)[i++] = autobox(args) ), ... );
    // Frame and Runnable callbacks arrive hundreds of times a second and would push everything else out of the
    // crash report's trail, so only the rarer calls are recorded.
    if (strcmp(methodName, "doFrame") != 0 && strcmp(methodName, "run") != 0)
        bogo_breadcrumb(className, methodName, nativeHandle);

    // The handle is a pointer to a C++ object of Unity's; its first word is a vtable inside a loaded library, and
    // InterfaceProxy.invoke calls through it. If that word isn't, the object behind the handle was freed or
    // overwritten (this crashed a soak run in the Play Billing callback, jumping into the heap), so don't call.
    {
        auto inGuestImage = [](uintptr_t p) {
            for (so_module* m = so_get_head(); m; m = m->next) {
                if (p >= m->text_base && p < m->text_base + m->text_size)
                    return true;
                for (int i = 0; i < m->n_data && i < MAX_DATA_SEG; i++)
                    if (p >= m->data_base[i] && p < m->data_base[i] + m->data_size[i])
                        return true;
            }
            return false;
        };
        uintptr_t vptr = nativeHandle ? *(uintptr_t*)nativeHandle : 0;
        if (nativeHandle && !inGuestImage(vptr)) {
            fprintf(stderr, "[JNIBRIDGE] handle %p for %s->%s has a bad vtable pointer %p (first words %p %p %p %p); the native "
                            "object behind it was freed or overwritten; dropping the call\n",
                (void*)nativeHandle, className, methodName, (void*)vptr, ((void**)nativeHandle)[0], ((void**)nativeHandle)[1],
                ((void**)nativeHandle)[2], ((void**)nativeHandle)[3]);
            bogo_breadcrumb("JNIBridge: handle has a bad vtable", methodName, (long)vptr);
            return;
        }
    }

    // The native behind JNIBridge.invoke is libunity code. If the Method record no longer points into a loaded
    // library, calling it would jump into data (this crashed a long soak run); say so and skip the call instead.
    if (Method* im = (Method*)invokeMethod) {
        uintptr_t fn = (uintptr_t)im->native;
        bool inGuestCode = false;
        for (so_module* m = so_get_head(); m && !inGuestCode; m = m->next)
            inGuestCode = fn >= m->text_base && fn < m->text_base + m->text_size;
        if (!inGuestCode) {
            fprintf(stderr, "[JNIBRIDGE] JNIBridge.invoke native %p is not inside any loaded library (Method %p, calling %s->%s); dropping the call\n",
                (void*)fn, (void*)im, className, methodName);
            bogo_breadcrumb("JNIBridge.invoke: bad native pointer", methodName, (long)fn);
            return;
        }
    }
    verbose("JNIBridge", "Invoking native handle %ld for %s->%s", nativeHandle, className, methodName);
    FakeJni::LocalFrame frame(vm);
    // Everything this call creates (the argument array, boxed values, class and method references) is dead once it
    // returns; without this, a callback fired hundreds of times a second leaked until memory ran out.
    LocalRefScope localRefs(frame.getJniEnv());
    // The invoke call itself was correct, as you pointed out.
    invokeMethod.invoke(frame.getJniEnv(), jniBridgeClass, nativeHandle, interfaceClass, interfaceMethod, argsArray);
}


// C# AndroidJavaProxy objects: Unity's Java ReflectionHelper.newProxyInstance hands Java a Proxy whose handler calls
// the static native ReflectionHelper.nativeProxyInvoke(long handle, String methodName, Object[] args); libunity finds
// the C# method by name from there.
template<typename... Args>
void JNIBridge::invokeReflection(long nativeHandle, const char* methodName, Args... args) {
    if (strcmp(methodName, "doFrame") != 0 && strcmp(methodName, "run") != 0)
        bogo_breadcrumb("C# proxy", methodName, nativeHandle);
    auto helper = vm.findClass("com/unity3d/player/ReflectionHelper").get();
    if (!helper) {
        fprintf(stderr, "[JNIBRIDGE] com/unity3d/player/ReflectionHelper isn't known to the VM; dropping %s\n", methodName);
        return;
    }
    auto proxyInvoke = helper->getMethod("(JLjava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;", "nativeProxyInvoke");
    if (!proxyInvoke) {
        fprintf(stderr, "[JNIBRIDGE] ReflectionHelper.nativeProxyInvoke isn't registered; dropping %s\n", methodName);
        return;
    }
    std::shared_ptr<FakeJni::JArray<jnivm::java::lang::Object>> argsArray = std::make_shared<FakeJni::JArray<jnivm::java::lang::Object>>(sizeof...(args));
    int i = 0;
    ( ( (*argsArray)[i++] = autobox(args) ), ... );
    (void)i;
    FakeJni::LocalFrame frame(vm);
    LocalRefScope localRefs(frame.getJniEnv()); // see JNIBridge::invoke
    try {
        proxyInvoke.invoke(frame.getJniEnv(), helper, (jlong)nativeHandle, std::make_shared<FakeJni::JString>(methodName), argsArray);
    } catch (const std::exception& e) {
        // An exception from the C# side; the Java caller of an interface method can't do anything with it either.
        fprintf(stderr, "[JNIBRIDGE] C# proxy method %s threw: %s\n", methodName, e.what());
        bogo_breadcrumb("C# proxy method threw", methodName, nativeHandle);
    }
}

// Explicit template instantiation is still required to prevent linker errors.
template void JNIBridge::invoke(long, const char*, const char*, const char*); // For Runnable.run()
template void JNIBridge::invoke(long, const char*, const char*, const char*, std::shared_ptr<jnivm::android::os::Message>); // For Handler.Callback.handleMessage()
template void JNIBridge::invoke(long, const char*, const char*, const char*, jlong); // for FrameCallback.doFrame()
template void JNIBridge::invoke(long, const char*, const char*, const char*, std::shared_ptr<jnivm::java::lang::Object>); // listeners taking one object (Play Billing)
template void JNIBridge::invokeReflection(long, const char*);
template void JNIBridge::invokeReflection(long, const char*, std::shared_ptr<jnivm::android::os::Message>);
template void JNIBridge::invokeReflection(long, const char*, jlong);
template void JNIBridge::invokeReflection(long, const char*, std::shared_ptr<jnivm::java::lang::Object>);


BEGIN_NATIVE_DESCRIPTOR(jnivm::bitter::jnibridge::JNIBridge) { FakeJni::Function<&JNIBridge::newInterfaceProxy> {}, "newInterfaceProxy", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::bitter::jnibridge::JNIBridgeProxy)
        END_NATIVE_DESCRIPTOR
