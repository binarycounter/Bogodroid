#pragma once
#include "object.h"
#include <mutex>
#include <unordered_map>
#include <vector>
#include <functional>
#include "array.h"
#include "internal/findclass.h"
namespace jnivm {
    class ENV;
    template<class Funk, class ...EnvOrObjOrClass> struct Wrap;
    enum class FunctionType;
    template<FunctionType, class> struct HookManager;
    class Field;
    class Method;
    class MethodProxy;

    class Class : public Object {
    public:
        std::mutex mtx;
        std::unordered_map<std::string, void*> natives;
        std::string name;
        std::string nativeprefix;
#ifdef JNI_DEBUG
        std::vector<std::shared_ptr<Class>> classes;
#endif
        std::vector<std::shared_ptr<Field>> fields;
        std::vector<std::shared_ptr<Method>> methods;
        std::function<std::shared_ptr<Object>(ENV* env)> Instantiate;
        std::function<std::shared_ptr<Array<Object>>(ENV* env, jsize length)> InstantiateArray;
        std::function<std::vector<std::shared_ptr<Class>>(ENV*)> baseclasses;

        Class() {

        }

        MethodProxy getMethod(const char* sig, const char* name);

        std::string getName() const {
            return nativeprefix;
        }

        template<class T> void Hook(ENV* env, const std::string& method, T&& t);
        template<class T> void HookInstance(ENV* env, const std::string& id, T&& t);
        template<class T> void HookInstanceFunction(ENV* env, const std::string& id, T&& t);
        template<class T> void HookInstanceGetterFunction(ENV* env, const std::string& id, T&& t);
        template<class T> void HookInstanceSetterFunction(ENV* env, const std::string& id, T&& t);
        template<class T> void HookGetterFunction(ENV* env, const std::string& id, T&& t);
        template<class T> void HookSetterFunction(ENV* env, const std::string& id, T&& t);
        template<class T> void HookInstanceProperty(ENV* env, const std::string& id, T&& t);

#ifdef JNI_DEBUG
        std::string GenerateHeader(std::string scope);
        std::string GeneratePreDeclaration();
        std::string GenerateStubs(std::string scope);
        std::string GenerateJNIPreDeclaration();
        std::string GenerateJNIBinding(std::string scope);
#endif
    };
}

#include "hookManager.h"
#include "wrap.h"

namespace jnivm {
    namespace impl {
        template<class T, FunctionType bind, size_t offset, bool b = Function<T>::type == FunctionType::None && ((int)bind & (int)FunctionType::Instance ? (std::is_base_of<Object, std::remove_pointer_t<typename Function<T>::template Parameter<offset>>>::value || std::is_same<jobject, typename Function<T>::template Parameter<offset>>::value) : (std::is_same<Class*, typename Function<T>::template Parameter<offset>>::value || std::is_same<jclass, typename Function<T>::template Parameter<offset>>::value))> struct HookManagerHelper {
            static void install(ENV* env, Class* cl, const std::string& id, T&& t) {}
        };

        template<class T, FunctionType bind> struct HookManagerHelper<T, bind, 1, true> {
            static void install(ENV* env, Class* cl, const std::string& id, T&& t) {
                // Filter false positives at runtime
                if((int)bind & (int)FunctionType::Instance ? !std::is_same<Object*, typename Function<T>::template Parameter<1>>::value && !std::is_same<jobject, typename Function<T>::template Parameter<1>>::value : !std::is_same<Class*, typename Function<T>::template Parameter<1>>::value && !std::is_same<jclass, typename Function<T>::template Parameter<1>>::value) {
                    auto oc = env->GetVM()->typecheck.find(typeid(std::remove_pointer_t<typename Function<T>::template Parameter<1>>));
                    if(oc == env->GetVM()->typecheck.end() || oc->second.get() != cl) {
                        return;
                    }
                }
                using w = Wrap<T, typename Function<T>::template Parameter<0>, typename Function<T>::template Parameter<1>>;
                HookManager<bind, w>::install(env, cl, id, std::move(t));
            }
        };
        template<class T, FunctionType bind> struct HookManagerHelper<T, bind, 0, true> {
            static void install(ENV* env, Class* cl, const std::string& id, T&& t) {
                // Filter false positives at runtime
                if((int)bind & (int)FunctionType::Instance ? !std::is_same<Object*, typename Function<T>::template Parameter<0>>::value && !std::is_same<jobject, typename Function<T>::template Parameter<0>>::value : !std::is_same<Class*, typename Function<T>::template Parameter<0>>::value && !std::is_same<jclass, typename Function<T>::template Parameter<0>>::value) {
                    auto oc = env->GetVM()->typecheck.find(typeid(std::remove_pointer_t<typename Function<T>::template Parameter<0>>));
                    if(oc == env->GetVM()->typecheck.end() || oc->second.get() != cl) {
                        return;
                    }
                }
                using w = Wrap<T, typename Function<T>::template Parameter<0>>;
                HookManager<bind, w>::install(env, cl, id, std::move(t));
            }
        };

        template<class T, FunctionType bind, bool EnvArg = std::is_same<typename Function<T>::template Parameter<0>, JNIEnv*>::value || std::is_same<typename Function<T>::template Parameter<0>, ENV*>::value, class=void> struct HookManagerHelper2 {
            using w = std::conditional_t<EnvArg, Wrap<T, typename Function<T>::template Parameter<0>>, Wrap<T>>;
            static void install(ENV* env, Class* cl, const std::string& id, T&& t) {
                HookManager<bind, w>::install(env, cl, id, t);
                impl::HookManagerHelper<T, bind, EnvArg ? 1 : 0>::install(env, cl, id, std::move(t));
            }
        };

        template<class T, FunctionType bind, bool EnvArg> struct HookManagerHelper2<T, bind, EnvArg, std::enable_if_t<((((int)bind & (int)FunctionType::Getter) && Function<T>::plength > (EnvArg ? 1 : 0)) || (((int)bind & (int)FunctionType::Setter) && Function<T>::plength > (EnvArg ? 2 : 1)))>> {
            static void install(ENV* env, Class* cl, const std::string& id, T&& t) {
                impl::HookManagerHelper<T, bind, EnvArg ? 1 : 0>::install(env, cl, id, std::move(t));
            }
        };
    }

    template<class T> void Class::Hook(ENV* env, const std::string& id, T&& t) {
        impl::HookManagerHelper2<T, Function<T>::type>::install(env, this, id, std::move(t));
    }

    template<class T> void Class::HookInstance(ENV * env, const std::string & id, T && t) {
        impl::HookManagerHelper2<T, (FunctionType)((int)Function<T>::type | (int)FunctionType::Instance)>::install(env, this, id, std::move(t));
    }

    template<class T> void Class::HookInstanceFunction(ENV* env, const std::string& id, T&& t) {
        impl::HookManagerHelper2<T, FunctionType::Instance>::install(env, this, id, std::move(t));
    }

    template<class T> void Class::HookInstanceGetterFunction(ENV* env, const std::string& id, T&& t) {
        impl::HookManagerHelper2<T, FunctionType::InstanceGetter>::install(env, this, id, std::move(t));
    }
    template<class T> void Class::HookInstanceSetterFunction(ENV* env, const std::string& id, T&& t) {
        impl::HookManagerHelper2<T, FunctionType::InstanceSetter>::install(env, this, id, std::move(t));
    }
    template<class T> void Class::HookGetterFunction(ENV* env, const std::string& id, T&& t) {
        impl::HookManagerHelper2<T, FunctionType::Getter>::install(env, this, id, std::move(t));
    }
    template<class T> void Class::HookSetterFunction(ENV* env, const std::string& id, T&& t) {
        impl::HookManagerHelper2<T, FunctionType::Setter>::install(env, this, id, std::move(t));
    }
    template<class T> void jnivm::Class::HookInstanceProperty(jnivm::ENV *env, const std::string &id, T &&t) {
        impl::HookManagerHelper2<T, FunctionType::InstanceProperty>::install(env, this, id, std::move(t));
    }
}