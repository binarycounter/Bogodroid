#ifndef JNIVM_VM_H_1
#define JNIVM_VM_H_1
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <jni.h>
#ifdef JNI_DEBUG
#include <jnivm/internal/codegen/namespace.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Processthreadsapi.h>
#else
#include <pthread.h>
#endif

namespace jnivm {

#ifdef _WIN32
    using pthread_t = DWORD;
#endif

}

namespace jnivm {
    class ENV;
    class Object;
    class Class;
    class VM {
        // Invocation Table
        JNIInvokeInterface iinterface;
        // Holder of the invocation table
        JavaVM javaVM;
        // Native Interface base Invocation Table
        JNINativeInterface ninterface;
        // Map of all jni threads and local stuff by thread id
        std::unordered_map<pthread_t, std::shared_ptr<ENV>> jnienvs;
    protected:
        void OverrideJNIInvokeInterface(const JNIInvokeInterface& iinterface);
        // If you override this, you have to use the contructor with skipInit=true and call VM::initialize in your derived Class
        virtual std::shared_ptr<ENV> CreateEnv();
        const JNINativeInterface& GetNativeInterfaceTemplate();
        std::vector<std::function<void(JNINativeInterface&)>> jnienvhooks;
    public:
        template<bool ReturnNull>
        static JNINativeInterface GetNativeInterfaceTemplate();

        void AddHook(std::function<void(JNINativeInterface&)>&& hook) {
            jnienvhooks.emplace_back(std::move(hook));
        }
#ifdef JNI_DEBUG
        // For Generating Stub header files out of captured jni usage
        Namespace np;
#endif
        // Map of all classes hooked or implicitly declared
        std::unordered_map<std::string, std::shared_ptr<Class>> classes;

        std::mutex mtx;
        // Stores all global references
        std::vector<std::shared_ptr<Object>> globals;
        // Stores all classes by c++ typeid
        std::unordered_map<std::type_index, std::shared_ptr<Class>> typecheck;
        VM(const VM&) = delete;
        VM(VM&&) = delete;
        // Initialize the native VM instance
        VM();
        // Skip initialize if requested
        VM(bool skipInit, bool ReturnNull = false);
        void initialize();
        // Returns the jni JavaVM
        JavaVM * GetJavaVM();
        // Returns the jni JNIEnv of the current thread
        JNIEnv * GetJNIEnv();
        // Returns the Env of the current thread
        const std::shared_ptr<ENV>& GetEnv();

        static VM* FromJavaVM(JavaVM * env);

#ifdef JNI_DEBUG
        // Dump all classes incl. function referenced or called from the (foreign) code
        // Namespace / Header Pre Declaration (no class body)
        std::string GeneratePreDeclaration();
        // Normal Header with class contents and member declarations
        std::string GenerateHeader();
        // Default Stub implementation of all declared members
        std::string GenerateStubs();
        // Register the previous class types to the java native interface
        std::string GenerateJNIPreDeclaration();
        // Register the previous classes to the java native interface
        std::string GenerateJNIBinding();
        // Dumps all previous functions at once, into a single file
        void GenerateClassDump(const char * path);
#endif
    };
}
#endif