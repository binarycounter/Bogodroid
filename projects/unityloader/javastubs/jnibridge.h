
#ifndef __JNIBRIDGE_H__
#define __JNIBRIDGE_H__

#include "android.h" // For Handler::Callback
#include "baron/baron.h"
#include "javac.h"
#include "logging.h"
#include <set>
#include <string>

namespace jnivm {
namespace bitter {
    namespace jnibridge {

        /**
         * A single, concrete proxy class that can dynamically implement multiple interfaces.
         * It inherits from all possible interfaces to satisfy the JNI type system.
         * At runtime, it checks which interfaces it was created to proxy.
         */
        class JNIBridgeProxy : public jnivm::java::lang::Runnable,
                               public jnivm::android::os::Handler::Callback,
                               public jnivm::android::view::Choreographer::FrameCallback,
                               public jnivm::android::hardware::input::InputManager::InputDeviceListener //Stub, since we're probably not handling device additions and removals
                                {
        public:
            // This gives the class a stable, registerable name for your JNI layer.
            DEFINE_CLASS_NAME("bitter/jnibridge/JNIBridgeProxy")

            long nativeHandle; // The pointer back to the engine's native object

            // Two kinds of proxy share this class, and the engine expects calls back through different natives:
            //  - false: made by JNIBridge.newInterfaceProxy for Unity's own C++ callbacks (Choreographer, Handler);
            //    nativeHandle is a C++ jni::ProxyObject, called through JNIBridge$InterfaceProxy.invoke.
            //  - true: made by ReflectionHelper.newProxyInstance for a C# AndroidJavaProxy (game listeners such as
            //    Play Billing's); nativeHandle is a GC handle, called through ReflectionHelper.nativeProxyInvoke.
            //    Sending these through JNIBridge treated the GC handle as a C++ object and jumped through garbage.
            bool viaReflectionHelper = false;

        private:
            // Stores the names of the interfaces this specific instance should implement.
            std::set<std::string> implementedInterfaces;

        public:
            JNIBridgeProxy(long handle, const std::set<std::string>& interfaces, bool viaReflectionHelper = false);

            // Calls one of the proxied interface's methods on the engine side, through the right native.
            template <typename... Args>
            void callInterface(const char* className, const char* methodName, const char* methodSig, Args... args);

            // --- Implementation of java.lang.Runnable ---
            void run() override;

            // --- Implementation of android.os.Handler.Callback ---
            bool handleMessage(std::shared_ptr<jnivm::android::os::Message> msg) override;

            // --- Implementation of android.view.Choreographer.FrameCallback ---
            void doFrame(jlong frameTimeNanos) override;
        };

        /**
         * The factory class that creates and manages proxies.
         */
        class JNIBridge : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("bitter/jnibridge/JNIBridge")

            // The factory function is now much simpler.
            static std::shared_ptr<jnivm::java::lang::Object> newInterfaceProxy(FakeJni::JLong j, std::shared_ptr<FakeJni::JArray<FakeJni::JClass>> classes);

            // The static invoker remains the same powerful, generic helper.
            template <typename... Args>
            static void invoke(long nativeHandle, const char* className, const char* methodName, const char* methodSig, Args... args);

            // For C# AndroidJavaProxy objects: ReflectionHelper.nativeProxyInvoke(handle, methodName, args).
            template <typename... Args>
            static void invokeReflection(long nativeHandle, const char* methodName, Args... args);
        };

        template <typename... Args>
        void JNIBridgeProxy::callInterface(const char* className, const char* methodName, const char* methodSig, Args... args)
        {
            if (viaReflectionHelper)
                JNIBridge::invokeReflection(nativeHandle, methodName, args...);
            else
                JNIBridge::invoke(nativeHandle, className, methodName, methodSig, args...);
        }

    }
}
}
#endif
