#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <utility>
#include <stdexcept>
#include <jni.h>
#include "jnitypes.h"
#include "function.h"

namespace jnivm {

    template<class... T> struct UnfoldJNISignature {
        static std::string GetJNISignature(ENV * env) {
            return "";
        }
    };

    template<class A, class... T> struct UnfoldJNISignature<A, T...> {
        static std::string GetJNISignature(ENV * env) {
            return JNITypes<A>::GetJNISignature(env) + UnfoldJNISignature<T...>::GetJNISignature(env);
        }
    };

    template<class T> struct AnotherHelper {
        static T GetEnvOrObject(ENV * env, jobject o) {
            return JNITypes<T>::JNICast(env, o);
        }
    };
    template<>
    struct AnotherHelper<JNIEnv*> {
        static JNIEnv* GetEnvOrClass(ENV * env, Class* clazz) {
            return env->GetJNIEnv();
        }
        static JNIEnv* GetEnvOrObject(ENV * env, jobject o) {
            return env->GetJNIEnv();
        }
    };
    template<>
    struct AnotherHelper<ENV*> {
        static ENV* GetEnvOrClass(ENV * env, Class* clazz) {
            return env;
        }
        static ENV* GetEnvOrObject(ENV * env, jobject o) {
            return env;
        }
    };
    template<>
    struct AnotherHelper<jclass> {
        static jclass GetEnvOrClass(ENV * env, Class* clazz) {
            return JNITypes<Class*>::ToJNIType(env, clazz->shared_from_this());
        }
    };
    template<>
    struct AnotherHelper<Class*> {
        static Class* GetEnvOrClass(ENV * env, Class* clazz) {
            return clazz;
        }
    };

    template<class Funk, class...EnvOrObjOrClass> struct Obj {
        using Function = jnivm::Function<Funk>;
        template<class Seq> struct InstanceBase;
        template<size_t...I> struct InstanceBase<std::index_sequence<I...>> {
            static constexpr auto InstanceInvoke(Funk& funk, ENV * env, jobject obj, const jvalue* values) {
                return (JNITypes<std::shared_ptr<typename Function::This>>::JNICast(env, obj).get()->*funk)(AnotherHelper<EnvOrObjOrClass>::GetEnvOrObject(env, obj)..., (JNITypes<typename Function::template Parameter<I+sizeof...(EnvOrObjOrClass)>>::JNICast(env, values[I]))...);
            }
            static constexpr typename Function::Return NonVirtualInstanceInvoke(Funk& funk, ENV * env, jobject obj, const jvalue* values) {
                throw std::runtime_error("Calling member function pointer non virtual is not supported in c++");
            }
            static constexpr auto InstanceSet(Funk& funk, ENV * env, jobject obj, const jvalue* values) {
                return (JNITypes<std::shared_ptr<typename Function::This>>::JNICast(env, obj).get()->*funk)(AnotherHelper<EnvOrObjOrClass>::GetEnvOrObject(env, obj)..., (JNITypes<typename Function::template Parameter<I+sizeof...(EnvOrObjOrClass)>>::JNICast(env, values[I]))...);
            }
            static std::string GetJNIInstanceInvokeSignature(ENV * env) {
                return "(" + UnfoldJNISignature<typename Function::template Parameter<I+sizeof...(EnvOrObjOrClass)>...>::GetJNISignature(env) + ")" + std::string(JNITypes<typename Function::Return>::GetJNISignature(env));
            }
            static std::string GetJNIInstanceSetterSignature(ENV * env) {
                static_assert(sizeof...(I) == 1, "To use this function as setter, you need to have exactly one parameter");
                return JNITypes<typename Function::template Parameter<sizeof...(EnvOrObjOrClass)>>::GetJNISignature(env);
            }
        };
        template<bool b, class Seq> struct StaticBase;
        template<size_t...I> struct StaticBase<true, std::index_sequence<I...>> {
            static constexpr auto InstanceInvoke(Funk& funk, ENV * env, jobject obj, const jvalue* values) {
                return funk(AnotherHelper<EnvOrObjOrClass>::GetEnvOrObject(env, obj)..., (JNITypes<typename Function::template Parameter<I+sizeof...(EnvOrObjOrClass)>>::JNICast(env, values[I]))...);
            }
            static constexpr auto NonVirtualInstanceInvoke(Funk& funk, ENV * env, jobject obj, const jvalue* values) {
                return InstanceInvoke(funk, env, obj, values);
            }
            static constexpr auto InstanceSet(Funk& funk, ENV * env, jobject obj, const jvalue* values) {
                static_assert(sizeof...(I) == 1, "To use this function as setter, you need to have exactly one parameter");
                return funk(AnotherHelper<EnvOrObjOrClass>::GetEnvOrObject(env, obj)..., (JNITypes<typename Function::template Parameter<I+sizeof...(EnvOrObjOrClass)>>::JNICast(env, values[I]))...);
            }
            static constexpr auto InstanceGet(Funk& funk, ENV * env, jobject obj, const jvalue* values) {
                static_assert(sizeof...(I) == 0, "To use this function as a getter, you need to have exactly zero parameter");
                return funk(AnotherHelper<EnvOrObjOrClass>::GetEnvOrObject(env, obj)...);
            }
            static std::string GetJNIInstanceInvokeSignature(ENV * env) {
                return "(" + UnfoldJNISignature<typename Function::template Parameter<I+sizeof...(EnvOrObjOrClass)>...>::GetJNISignature(env) + ")" + std::string(JNITypes<typename Function::Return>::GetJNISignature(env));
            }
            static std::string GetJNIInstanceGetterSignature(ENV * env) {
                static_assert(sizeof...(I) == 0, "To use this function as a getter, you need to have exactly zero parameter");
                return JNITypes<typename Function::Return>::GetJNISignature(env);
            }
            static std::string GetJNIInstanceSetterSignature(ENV * env) {
                static_assert(sizeof...(I) == 1, "To use this function as setter, you need to have exactly one parameter");
                return JNITypes<typename Function::template Parameter<sizeof...(EnvOrObjOrClass)>>::GetJNISignature(env);
            }
        };
        template<size_t...I> struct StaticBase<false, std::index_sequence<I...>> {
            static constexpr auto StaticInvoke(Funk& funk, ENV * env, Class* clazz, const jvalue* values) {
                return funk(AnotherHelper<EnvOrObjOrClass>::GetEnvOrClass(env, clazz)..., (JNITypes<typename Function::template Parameter<I+sizeof...(EnvOrObjOrClass)>>::JNICast(env, values[I]))...);
            }
            static constexpr auto StaticSet(Funk& funk, ENV * env, Class* clazz, const jvalue* values) {
                static_assert(sizeof...(I) == 1, "To use this function as setter, you need to have exactly one parameter");
                return funk(AnotherHelper<EnvOrObjOrClass>::GetEnvOrClass(env, clazz)..., (JNITypes<typename Function::template Parameter<I+sizeof...(EnvOrObjOrClass)>>::JNICast(env, values[I]))...);
            }
            static constexpr auto StaticGet(Funk& funk, ENV * env, Class* clazz, const jvalue* values) {
                static_assert(sizeof...(I) == 0, "To use this function as setter, you need to have exactly zero parameter");
                return funk(AnotherHelper<EnvOrObjOrClass>::GetEnvOrClass(env, clazz)...);
            }
            static std::string GetJNIStaticInvokeSignature(ENV * env) {
                return "(" + UnfoldJNISignature<typename Function::template Parameter<I+sizeof...(EnvOrObjOrClass)>...>::GetJNISignature(env) + ")" + std::string(JNITypes<typename Function::Return>::GetJNISignature(env));
            }
            static std::string GetJNIStaticGetterSignature(ENV * env) {
                static_assert(sizeof...(I) == 0, "To use this function as setter, you need to have exactly zero parameter");
                return JNITypes<typename Function::Return>::GetJNISignature(env);
            }
            static std::string GetJNIStaticSetterSignature(ENV * env) {
                static_assert(sizeof...(I) == 1, "To use this function as setter, you need to have exactly one parameter");
                return JNITypes<typename Function::template Parameter<sizeof...(EnvOrObjOrClass)>>::GetJNISignature(env);
            }
        };
    };

    template<class Funk, FunctionType type, class I> class OldWrapper;
    template<class Funk, FunctionType type, class...EnvOrObjOrClass> class BaseWrapper : public OldWrapper<Funk, type, std::make_index_sequence<jnivm::Function<Funk>::plength>> {
        using OldWrapper<Funk, type, std::make_index_sequence<jnivm::Function<Funk>::plength>>::OldWrapper;
    };
    template<class Funk>
    class Property {
        using Function = jnivm::Function<Funk>;
    public:
        static std::string GetJNIInstanceGetterSignature(ENV * env) {
            return JNITypes<typename Function::Return>::GetJNISignature(env);
        }
        static std::string GetJNIInstanceSetterSignature(ENV * env) {
            return GetJNIInstanceGetterSignature(env);
        }
        static std::string GetJNIStaticGetterSignature(ENV * env) {
            return GetJNIInstanceGetterSignature(env);
        }
        static std::string GetJNIStaticSetterSignature(ENV * env) {
            return GetJNIInstanceGetterSignature(env);
        }
        
        
    };
    template<class Funk, size_t...I> class OldWrapper<Funk, FunctionType::Property, std::index_sequence<I...>> : public Property<Funk> {
        using Function = jnivm::Function<Funk>;
        Funk handle;
    public:
        OldWrapper(Funk handle) : handle(handle) {}

        constexpr void StaticSet(ENV * env, Class* clazz, const jvalue* values) {
            *handle = (JNITypes<typename Function::Return>::JNICast(env, values[0]));
        }
        constexpr auto StaticGet(ENV * env, Class* clazz, const jvalue* values) {
            return *handle;
        }
        constexpr void InstanceSet(ENV * env, jobject obj, const jvalue* values) {
            *handle = (JNITypes<typename Function::Return>::JNICast(env, values[0]));
        }
        constexpr auto InstanceGet(ENV * env, jobject obj, const jvalue* values) {
            return *handle;
        }
    };

    template<class Funk, size_t...I> class OldWrapper<Funk, FunctionType::InstanceProperty, std::index_sequence<I...>> : public Property<Funk> {
        using Function = jnivm::Function<Funk>;
        Funk handle;
    public:
        OldWrapper(Funk handle) : handle(handle) {}

        constexpr void InstanceSet(ENV * env, jobject obj, const jvalue* values) {
            (JNITypes<std::shared_ptr<typename Function::This>>::JNICast(env, obj).get())->*handle = (JNITypes<typename Function::Return>::JNICast(env, values[0]));
        }
        constexpr auto InstanceGet(ENV * env, jobject obj, const jvalue* values) {
            return (JNITypes<std::shared_ptr<typename Function::This>>::JNICast(env, obj).get())->*handle;
        }
    };
    
    template<class Funk, class...EnvOrObjOrClass> class BaseWrapper<Funk, FunctionType::Instance, EnvOrObjOrClass...> : public Obj<Funk, EnvOrObjOrClass...>::template InstanceBase<std::make_index_sequence<Function<Funk>::plength - sizeof...(EnvOrObjOrClass)>> {
        using Function = jnivm::Function<Funk>;
        Funk handle;
        using BaseClass = typename Obj<Funk, EnvOrObjOrClass...>::template InstanceBase<std::make_index_sequence<Function::plength - sizeof...(EnvOrObjOrClass)>>;
    public:
        BaseWrapper(Funk handle) : handle(handle) {}

        constexpr auto InstanceInvoke(ENV * env, jobject obj, const jvalue* values) {
            return BaseClass::InstanceInvoke(handle, env, obj, values);
        }
        typename Function::Return NonVirtualInstanceInvoke(ENV * env, jobject obj, const jvalue* values) {
            throw std::runtime_error("Calling member function pointer non virtual is not supported in c++");
        }
        constexpr auto InstanceSet(ENV * env, jobject obj, const jvalue* values) {
            return BaseClass::InstanceSet(handle, env, obj, values);
        }
    };

    template<class Funk, class Base1, class Base2> class WrapperUtil : public Base1, public Base2 {
        Funk handle;
        using Function = jnivm::Function<Funk>;
    public:
        WrapperUtil(Funk handle) : handle(handle) {}

        constexpr auto InstanceInvoke(ENV * env, jobject obj, const jvalue* values) {
            return Base1::InstanceInvoke(handle, env, obj, values);
        }
        constexpr auto NonVirtualInstanceInvoke(ENV * env, jobject obj, const jvalue* values) {
            return Base1::NonVirtualInstanceInvoke(handle, env, obj, values);
        }
        constexpr auto InstanceGet(ENV * env, jobject obj, const jvalue* values) {
            return Base1::InstanceGet(handle, env, obj, values);
        }
        constexpr auto InstanceSet(ENV * env, jobject obj, const jvalue* values) {
            return Base1::InstanceSet(handle, env, obj, values);
        }
        auto StaticInvoke(ENV * env, Class* c, const jvalue* values) {
            static_assert(!std::is_same<Base2, Base1>::value, "What happend");
            return Base2::StaticInvoke(handle, env, c, values);
        }
        constexpr auto StaticGet(ENV * env, Class* c, const jvalue* values) {
            return Base2::StaticGet(handle, env, c, values);
        }
        constexpr auto StaticSet(ENV * env, Class* c, const jvalue* values) {
            return Base2::StaticSet(handle, env, c, values);
        }
    };

    template<class Funk, class ...EnvOrObjOrClass> class BaseWrapper<Funk, FunctionType::None, EnvOrObjOrClass...> : public WrapperUtil<Funk, std::remove_reference_t<decltype(std::declval<typename Obj<Funk, EnvOrObjOrClass...>::template StaticBase<true, std::make_index_sequence<Function<Funk>::plength - sizeof...(EnvOrObjOrClass)>>>())>, std::remove_reference_t<decltype(std::declval<typename Obj<Funk, EnvOrObjOrClass...>::template StaticBase<false, std::make_index_sequence<Function<Funk>::plength - sizeof...(EnvOrObjOrClass)>>>())>>{
    public:
        using WrapperUtil<Funk, typename Obj<Funk, EnvOrObjOrClass...>::template StaticBase<true, std::make_index_sequence<jnivm::Function<Funk>::plength - sizeof...(EnvOrObjOrClass)>>, typename Obj<Funk, EnvOrObjOrClass...>::template StaticBase<false, std::make_index_sequence<jnivm::Function<Funk>::plength - sizeof...(EnvOrObjOrClass)>>>::WrapperUtil;
    };

    template<class Funk, class ...EnvOrObjOrClass> struct Wrap {
        using T = Funk;
        using Function = jnivm::Function<Funk>;

        template<class T> 
        using __JNIType = std::conditional_t<std::is_same<bool, T>::value, jboolean, std::conditional_t<std::is_class<T>::value, jobject, T>>;
        template<class T, class ReturnType> struct WrapperClasses;

        template<class T> struct WrapperClasses<T, void> {
            using ReturnType = void;
            struct StaticFunction : public MethodHandle {
            public:
                StaticFunction(T&&t) : t(t) {}
                T t;
                virtual ReturnType StaticInvoke(ENV * env, Class* clazz, const jvalue* values, MethodHandleBase<ReturnType>) override {
                    t.StaticInvoke(env, clazz, values);
                }
            };
            struct StaticSetter : public MethodHandle {
            public:
                StaticSetter(T&&t) : t(t) {}
                T t;
                virtual ReturnType StaticSet(ENV * env, Class* clazz, const jvalue* values, MethodHandleBase<std::conditional_t<Function::plength == 0, __JNIType<typename Function::Return>, __JNIType<typename Function::template Parameter<Function::plength < 1 ? 0 : Function::plength - 1>>>>) override {
                    return t.StaticSet(env, clazz, values);
                }
            };
            struct InstanceFunction : public MethodHandle {
            public:
                InstanceFunction(T&&t) : t(t) {}
                T t;
                virtual ReturnType InstanceInvoke(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<ReturnType>) override {
                    t.InstanceInvoke(env, obj, values);
                }
                virtual ReturnType NonVirtualInstanceInvoke(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<ReturnType>) override {
                    t.NonVirtualInstanceInvoke(env, obj, values);
                }
            };
            struct InstanceSetter : public MethodHandle {
            public:
                InstanceSetter(T&&t) : t(t) {}
                T t;
                virtual ReturnType InstanceSet(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<std::conditional_t<Function::plength == 0, __JNIType<typename Function::Return>, __JNIType<typename Function::template Parameter<Function::plength < 1 ? 0 : Function::plength - 1>>>>) override {
                    t.InstanceSet(env, obj, values);
                }
            };
        };

        template<class T, class ReturnType = __JNIType<typename Function::Return>> struct WrapperClasses {
            struct StaticFunction : public MethodHandle {
            public:
                StaticFunction(T&&t) : t(t) {}
                T t;
                virtual ReturnType StaticInvoke(ENV * env, Class* clazz, const jvalue* values, MethodHandleBase<ReturnType>) override {
                    return JNITypes<typename Function::Return>::ToJNIReturnType(env, t.StaticInvoke(env, clazz, values));
                }
            };
            struct StaticGetter : public MethodHandle {
            public:
                StaticGetter(T&&t) : t(t) {}
                T t;
                virtual ReturnType StaticGet(ENV * env, Class* clazz, const jvalue* values, MethodHandleBase<ReturnType>) override {
                    return JNITypes<typename Function::Return>::ToJNIReturnType(env, t.StaticGet(env, clazz, values));
                }
            };
            struct InstanceFunction : public MethodHandle {
            public:
                InstanceFunction(T&&t) : t(t) {}
                T t;
                virtual ReturnType InstanceInvoke(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<ReturnType>) override {
                    return JNITypes<typename Function::Return>::ToJNIReturnType(env, t.InstanceInvoke(env, obj, values));
                }
                virtual ReturnType NonVirtualInstanceInvoke(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<ReturnType>) override {
                    return JNITypes<typename Function::Return>::ToJNIReturnType(env, t.NonVirtualInstanceInvoke(env, obj, values));
                }
            };
            struct InstanceGetter : public MethodHandle {
            public:
                InstanceGetter(T&&t) : t(t) {}
                T t;
                virtual ReturnType InstanceGet(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<ReturnType>) override {
                    return JNITypes<typename Function::Return>::ToJNIReturnType(env, t.InstanceGet(env, obj, values));
                }
            };
            // Workaround for Properties
            using InstanceSetter = typename WrapperClasses<T, void>::InstanceSetter;
            using StaticSetter = typename WrapperClasses<T, void>::StaticSetter;
        };
        
        using Wrapper = BaseWrapper<Funk, Function::type , EnvOrObjOrClass...>;
    };
}
