
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

        private:
            // Stores the names of the interfaces this specific instance should implement.
            std::set<std::string> implementedInterfaces;

        public:
            JNIBridgeProxy(long handle, const std::set<std::string>& interfaces);

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
        };

    }
}
}
#endif
