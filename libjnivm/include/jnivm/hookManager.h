#pragma once
#include "function.h"
#include "class.h"
#include "method.h"
#include "field.h"
#include <algorithm>
#include <functional>

namespace jnivm {
    template<bool isStatic, class Wrapper> struct InvokeSignature {
        static std::string Get(ENV* env) {
            return Wrapper::GetJNIInstanceInvokeSignature(env);
        }
    };
    template<class Wrapper> struct InvokeSignature<true, Wrapper> {
        static std::string Get(ENV* env) {
            return Wrapper::GetJNIStaticInvokeSignature(env);
        }
    };

    template<bool isStatic, bool isGetter, class Wrapper> struct PropertySignature;
    template<class Wrapper> struct PropertySignature<true, true, Wrapper> {
        static std::string Get(ENV* env) {
            return Wrapper::GetJNIStaticGetterSignature(env);
        }
    };

    template<class Wrapper> struct PropertySignature<true, false, Wrapper> {
        static std::string Get(ENV* env) {
            return Wrapper::GetJNIStaticSetterSignature(env);
        }
    };

    template<class Wrapper> struct PropertySignature<false, true, Wrapper> {
        static std::string Get(ENV* env) {
            return Wrapper::GetJNIInstanceGetterSignature(env);
        }
    };

    template<class Wrapper> struct PropertySignature<false, false, Wrapper> {
        static std::string Get(ENV* env) {
            return Wrapper::GetJNIInstanceSetterSignature(env);
        }
    };

    template<class w, class W, bool isStatic> struct FunctionBase {
        template<class T> static void install(ENV* env, Class * cl, const std::string& id, T&& t) {
            auto ssig = InvokeSignature<isStatic, typename w::Wrapper>::Get(env);
            auto ccl =
                    std::find_if(cl->methods.begin(), cl->methods.end(),
                                            [&id, &ssig](std::shared_ptr<Method> &m) {
                                                return m->_static == isStatic && m->name == id && m->signature == ssig;
                                            });
            std::shared_ptr<Method> method;
            if (ccl != cl->methods.end()) {
                method = *ccl;
            } else {
                method = std::make_shared<Method>();
                method->name = id;
                method->_static = isStatic;
                method->signature = std::move(ssig);
                cl->methods.push_back(method);
            }
            method->nativehandle = std::make_shared<W>(typename w::Wrapper {t});
        }

        template<class T> static void install(ENV* env, Class * cl, const std::string& id, const std::string& signature, T&& t) {
            auto ssig = signature;
            static_assert(Function<T>::plength == 3 && std::is_same<typename Function<T>::Return, jvalue>::value  && std::is_same<typename Function<T>::template Parameter<0>,JNIEnv*>::value && std::is_same<typename Function<T>::template Parameter<2>,jvalue*>::value, "Invalid arbitary function");
            auto ccl =
                    std::find_if(cl->methods.begin(), cl->methods.end(),
                                            [&id, &ssig](std::shared_ptr<Method> &m) {
                                                return m->_static == isStatic && m->name == id && m->signature == ssig;
                                            });
            std::shared_ptr<Method> method;
            if (ccl != cl->methods.end()) {
                method = *ccl;
            } else {
                method = std::make_shared<Method>();
                method->name = id;
                method->_static = isStatic;
                method->signature = std::move(ssig);
                cl->methods.push_back(method);
            }
            method->nativehandle = std::make_shared<W>(typename w::Wrapper {t});
        }
    };

    template<class w> struct HookManager<FunctionType::None, w> : FunctionBase<w, typename w::template WrapperClasses<typename w::Wrapper>::StaticFunction, true> {
        
    };

    template<class w> struct HookManager<FunctionType::Instance, w> {
        template<class T> static void install(ENV* env, Class * cl, const std::string& id, T&& t) {
            FunctionBase<w, typename w::template WrapperClasses<typename w::Wrapper>::InstanceFunction, false>::install(env, cl, id, t);
        }
    };

    template<class w, class W, bool isStatic, bool isGetter, class handle_t, handle_t handle> struct PropertyBase {
        template<class T> static void install(ENV* env, Class * cl, const std::string& id, T&& t) {
            auto ssig = PropertySignature<isStatic, isGetter, typename w::Wrapper>::Get(env);
            auto ccl =
                    std::find_if(cl->fields.begin(), cl->fields.end(),
                                            [&id, &ssig](std::shared_ptr<Field> &f) {
                                                return f->_static == isStatic && f->name == id && f->type == ssig;
                                            });
            std::shared_ptr<Field> field;
            if (ccl != cl->fields.end()) {
                field = *ccl;
            } else {
                field = std::make_shared<Field>();
                field->name = id;
                field->_static = isStatic;
                field->type = std::move(ssig);
                cl->fields.push_back(field);
            }
            field.get()->*handle = std::make_shared<W>(typename w::Wrapper {t});
        }

        template<class T> static void install(ENV* env, Class * cl, const std::string& id, const std::string& signature, T&& t) {
            static_assert(Function<T>::plength == 3 && std::is_same<typename Function<T>::Return, jvalue>::value && std::is_same<typename Function<T>::template Parameter<0>,JNIEnv*>::value && std::is_same<typename Function<T>::template Parameter<2>,jvalue*>::value, "Invalid arbitary function");
            auto ssig = signature;
            auto ccl =
                    std::find_if(cl->fields.begin(), cl->fields.end(),
                                            [&id, &ssig](std::shared_ptr<Field> &f) {
                                                return f->_static == isStatic && f->name == id && f->type == ssig;
                                            });
            std::shared_ptr<Field> field;
            if (ccl != cl->fields.end()) {
                field = *ccl;
            } else {
                field = std::make_shared<Field>();
                field->name = id;
                field->_static = isStatic;
                field->type = std::move(ssig);
                cl->fields.push_back(field);
            }
            field.get()->*handle = std::make_shared<W>(typename w::Wrapper {t});
        }
    };

    template<class w, class W, bool isStatic> struct GetterBase : PropertyBase<w, W, isStatic, true, decltype(&Field::getnativehandle), &Field::getnativehandle> {

    };

    template<class w, class W, bool isStatic> struct SetterBase : PropertyBase<w, W, isStatic, false, decltype(&Field::setnativehandle), &Field::setnativehandle> {
    };

    template<class w> struct HookManager<FunctionType::Getter, w> : GetterBase<w, typename w::template WrapperClasses<typename w::Wrapper>::StaticGetter, true> {

    };

    template<class w> struct HookManager<FunctionType::Setter, w> : SetterBase<w, typename w::template WrapperClasses<typename w::Wrapper>::StaticSetter, true> {
        
    };

    template<class w> struct HookManager<FunctionType::InstanceGetter, w> : GetterBase<w, typename w::template WrapperClasses<typename w::Wrapper>::InstanceGetter, false> {

    };

    template<class w> struct HookManager<FunctionType::InstanceSetter, w> : SetterBase<w, typename w::template WrapperClasses<typename w::Wrapper>::InstanceSetter, false> {
        
    };

    template<class w> struct HookManager<FunctionType::InstanceProperty, w> {
        template<class T> static void install(ENV* env, Class * cl, const std::string& id, T&& t) {
            HookManager<FunctionType::InstanceGetter, w>::install(env, cl, id, t);
            HookManager<FunctionType::InstanceSetter, w>::install(env, cl, id, t);
        }
    };

    template<class w> struct HookManager<FunctionType::Property, w> {
        template<class T> static void install(ENV* env, Class * cl, const std::string& id, T&& t) {
            HookManager<FunctionType::Getter, w>::install(env, cl, id, t);
            HookManager<FunctionType::Setter, w>::install(env, cl, id, t);
        }
    };
}
