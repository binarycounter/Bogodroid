#pragma once
#include "../jnivm.h"
#include <functional>
#include <stdexcept>
#include "../jnivm/object.h"
#include "../jnivm/string.h"
#include "../jnivm/class.h"
#include "libraryoptions.h"
namespace FakeJni {
    class Jvm;
    class Env;

    using JObject = jnivm::Object;
    using JString = jnivm::String;
    using JClass = jnivm::Class;
    using JThrowable = jnivm::Throwable;
    using JWeak = jnivm::Weak;

    template<class T> using JArray = jnivm::Array<T>;
    using JBoolean = jboolean;
    using JByte = jbyte;
    using JChar = jchar;
    using JShort = jshort;
    using JInt = jint;
    using JFloat = jfloat;
    using JLong = jlong;
    using JDouble = jdouble;
    using JBooleanArray = jnivm::Array<JBoolean>;
    using JByteArray = jnivm::Array<JByte>;
    using JCharArray = jnivm::Array<JChar>;
    using JShortArray = jnivm::Array<JShort>;
    using JIntArray = jnivm::Array<JInt>;
    using JFloatArray = jnivm::Array<JFloat>;
    using JLongArray = jnivm::Array<JLong>;
    using JDoubleArray = jnivm::Array<JDouble>;

    class JMethodID {
    public:
        enum Modifiers : uint32_t {
            PUBLIC = 1,
            PRIVATE = 2,
            PROTECTED = 4,
            STATIC = 8,
        };
    };

    class JFieldID {
    public:
        enum Modifiers : uint32_t {
            PUBLIC = 1,
            PRIVATE = 2,
            PROTECTED = 4,
            STATIC = 8,
        };
    };

    class Jvm : public JavaVM, protected jnivm::VM {
    private:
        class libinst {
            void* handle;
            LibraryOptions loptions;
            JavaVM* javaVM;
            std::string path;
        public:
            libinst(const std::string& rpath, JavaVM* javaVM, LibraryOptions loptions);
            ~libinst();
        };
        std::unordered_map<std::string, std::unique_ptr<libinst>> libraries;
        // JNIInvokeInterface oldinterface;
    protected:
        virtual std::shared_ptr<jnivm::ENV> CreateEnv() override;
        Jvm(bool skipInit, bool returnNull);
    public:
        Jvm();

        // compatibility stub
        void registerDefaultSignalHandler() {}

        void start();
        void start(std::shared_ptr<JArray<JString>> args);

        void attachLibrary(const std::string &rpath = "", const std::string &options = "", LibraryOptions loptions = {});
        void removeLibrary(const std::string &rpath = "", const std::string &options = "");

        jobject createGlobalReference(std::shared_ptr<JObject> obj);

        template<class cl> inline void registerClass();

        // compatibility stub
        template<class cl> inline void unregisterClass() {

        }

        virtual std::shared_ptr<JClass> findClass(const char * name);

        std::vector<std::shared_ptr<FakeJni::JClass>> getClasses();

        // compatibility stub
        void destroy() {}
    };

    class Env : public JNIEnv, public jnivm::ENV {
        Jvm& jvm;
    public:
        Env(Jvm& jvm, jnivm::VM *vm, const JNINativeInterface& interface) : jvm(jvm), jnivm::ENV(vm, interface) {
            functions = GetJNIEnv()->functions;
        }

        std::shared_ptr<JObject> resolveReference(jobject obj);

        jobject createLocalReference(std::shared_ptr<JObject> obj) {
            return jnivm::JNITypes<std::shared_ptr<JObject>>::ToJNIReturnType(this, obj);
        }
        Jvm& getVM();
    };

    struct ThreadContext {
        std::weak_ptr<Env> env;
        ~ThreadContext();
    };
    class JniEnvContext {
        FakeJni::Jvm* vm;
        std::shared_ptr<Env> env2;
    public:
        JniEnvContext(Jvm& vm);
        JniEnvContext();
        ~JniEnvContext();
        static thread_local ThreadContext env;
        Env& getJniEnv();
    };

    struct LocalFrame : public JniEnvContext {
        LocalFrame(int size = 0) : JniEnvContext() {
            (&getJniEnv())->PushLocalFrame(size);
        }
        LocalFrame(const Jvm& vm, int size = 0) : JniEnvContext((Jvm&)vm) {
            (&getJniEnv())->PushLocalFrame(size);
        }
        ~LocalFrame() {
            (&getJniEnv())->PopLocalFrame(nullptr);
        }
    };

    class JniEnv {
    public:
        static auto getCurrentEnv() {
            return JniEnvContext::env.env.lock().get();
        }
    };

    struct DescriptorBase {

    };

#ifdef __cpp_nontype_template_parameter_auto

    template<auto T> struct Field : DescriptorBase {
        using Type = decltype(T);
        static constexpr Type handle = T;
    };
    template<auto T> struct Function : DescriptorBase {
        using Type = decltype(T);
        static constexpr Type handle = T;
    };

#endif

    template<class U, class... T> struct Constructor {
        using Type = U;
        using args = std::tuple<T...>;
        static std::shared_ptr<U> ctr(T...args) {
            return std::make_shared<U>(args...);
        }
    };

    struct Descriptor {

        std::function<void(jnivm::ENV*env, jnivm::Class* cl)> registre;

        template<class U,bool isInstance, class ID> struct Helper5;
        template<class U, class ID> struct Helper5<U, true, ID> {
            static std::function<void (jnivm::ENV *env, jnivm::Class *cl)> Get(U && d, const char* name, int ty) {
                if((ty & (int)ID::Modifiers::STATIC) != 0) {
                    throw std::runtime_error("Fatal: Tried to link instance function as static");
                } else {
                    return [name, d](jnivm::ENV*env, jnivm::Class* cl) {
                        cl->HookInstance(env, name, d);
                    };
                }
            }
            static int GetFlags() {
                return ID::Modifiers::PUBLIC;
            }
        };

        template<class U, class ID> struct Helper5<U, false, ID> {
            static std::function<void (jnivm::ENV *env, jnivm::Class *cl)> Get(U && d, const char* name, int ty) {
                if((ty & (int)ID::Modifiers::STATIC) != 0) {
                    return [name, d](jnivm::ENV*env, jnivm::Class* cl) {
                        cl->Hook(env, name, d);
                    };
                } else {
                    return [name, d](jnivm::ENV*env, jnivm::Class* cl) {
                        cl->HookInstance(env, name, d);
                    };
                }
            }
            static int GetFlags() {
                return ID::Modifiers::PUBLIC
#ifdef JNIVM_FAKE_JNI_MINECRAFT_LINUX_COMPAT
                        | ID::Modifiers::STATIC
#endif
                ;
            }
        };

        template<class U,bool o = std::is_base_of<DescriptorBase, U>::value> struct Helper4 : Helper5<U, (int)jnivm::Function<U>::type & (int)jnivm::FunctionType::Instance, std::conditional_t<(bool)((int)jnivm::Function<U>::type & (int)jnivm::FunctionType::Property), JFieldID, JMethodID>> {

        };

        template<class U>
        struct Helper4<U, true> : Helper4<decltype(U::handle)> {
            static std::function<void (jnivm::ENV *env, jnivm::Class *cl)> Get(U && d, const char* name, int ty) {
                return Helper4<decltype(U::handle)>::Get(std::move(U::handle), name, ty);
            }
        };

        template<class U> Descriptor(U && d, const char* name, int flags) {
            registre = Helper4<U>::Get(std::move(d), name, flags);
        }

        template<class U> Descriptor(U && d, const char* name) : Descriptor(std::move(d), name, Helper4<U>::GetFlags()) {
        }

        template<class U> Descriptor(U && d, int flags = 0) {
            registre = [](jnivm::ENV*env, jnivm::Class* cl) {
                cl->Hook(env, "<init>", U::ctr);
            };
        }
    };
}

namespace FakeJni {
    template<class cl> void Jvm::registerClass() {
        cl::registerClass();
    }

    void createMainMethod(FakeJni::Jvm &jvm, std::function<void (std::shared_ptr<FakeJni::JArray<FakeJni::JString>> args)>&& callback);
    // Deprecated: only provided for compatibility with original baron / fake-jni https://github.com/dukeify/baron/blob/old_prototype/README.md#how-do-i-use-it
    void createMainMethod(FakeJni::Jvm &jvm, std::function<void (FakeJni::JArray<FakeJni::JString>* args)>&& callback);
}

#define DEFINE_CLASS_NAME(cname, ...)   static std::vector<std::shared_ptr<jnivm::Class>> GetBaseClasses(jnivm::ENV *env) {\
                                            return jnivm::Extends< __VA_ARGS__ >::GetBaseClasses(env);\
                                        }\
                                        static std::string getClassName() {\
                                            return cname;\
                                        }\
                                        static std::shared_ptr<jnivm::Class> registerClass();\
                                        static std::shared_ptr<jnivm::Class> getDescriptor();\
                                        virtual std::shared_ptr<jnivm::Class> getClassInternal(jnivm::ENV* env) override {\
                                            return getDescriptor();\
                                        }
#define BEGIN_NATIVE_DESCRIPTOR(name, ...)  std::shared_ptr<jnivm::Class> name ::getDescriptor() {\
                                                auto cl = jnivm::ENV::FromJNIEnv(&FakeJni::JniEnvContext().getJniEnv())->GetClass< name >( name ::getClassName().data());\
                                                if(cl->methods.size() == 0 && cl->fields.size() == 0 && !cl->Instantiate && !cl->baseclasses) {\
                                                    registerClass();\
                                                }\
                                                return cl;\
                                            }\
                                            std::shared_ptr<jnivm::Class> name ::registerClass() {\
                                                using ClassName = name ;\
                                                static std::vector<FakeJni::Descriptor> desc({
#define END_NATIVE_DESCRIPTOR                   });\
                                                FakeJni::LocalFrame frame;\
                                                std::shared_ptr<jnivm::Class> clazz = jnivm::ENV::FromJNIEnv(&FakeJni::JniEnvContext().getJniEnv())->GetClass<ClassName>(ClassName::getClassName().data());\
                                                for(auto&& des : desc) {\
                                                    des.registre(jnivm::ENV::FromJNIEnv(&FakeJni::JniEnvContext().getJniEnv()), clazz.get());\
                                                }\
                                                return clazz;\
                                            }
