#ifndef JNIVM_METHOD_H_1
#define JNIVM_METHOD_H_1
#include "object.h"
#include <string>
#include <stdexcept>
#include <jni.h>

#include "methodhandlebase.h"

namespace jnivm {
    class Class;
    class ENV;

    class Method : public Object {
        jvalue jinvoke(ENV& env, jclass cl, jvalue*);
        jvalue jinvoke(ENV& env, jobject obj, jvalue*);
        template<class T, class... param>
        jvalue j2invoke(JNIEnv& env, T clorObj, param... params);
    public:
        std::string name;
        std::string signature;
        bool _static = false;
        void* native = nullptr;
        std::shared_ptr<MethodHandle> nativehandle;

#ifdef JNI_DEBUG
        std::string GenerateHeader(const std::string &cname);
        std::string GenerateStubs(std::string scope, const std::string &cname);
        std::string GenerateJNIBinding(std::string scope, const std::string &cname);
#endif
        template<class... param>
        jvalue invoke(JNIEnv& env, jnivm::Class* cl, param... params);
        template<class... param>
        jvalue invoke(JNIEnv& env, jnivm::Object* obj, param... params);
    };

    class MethodProxy {
        jmethodID _member;
        jmethodID _static;
        jmethodID _native;
    public:
        MethodProxy(jmethodID _member, jmethodID _static, jmethodID _native) {
            this->_member = _member;
            this->_static = _static;
            this->_native = _native;
        }
        template<class... param>
        jvalue invoke(JNIEnv& env, jnivm::Class* cl, param... params) {
            if(!_static && !_native) throw std::runtime_error("No such Method!");
            return ((Method*) (_static ? _static : _native))->invoke(env, cl, params...);
        }
        template<class... param>
        jvalue invoke(JNIEnv& env, jnivm::Object* obj, param... params) {
            if(!_member && !_native) throw std::runtime_error("No such Method!");
            return ((Method*) (_member ? _member : _native))->invoke(env, obj, params...);
        }
        MethodProxy* operator->() {
            return this;
        }
        operator bool() const {
            return (bool)_member ^ (bool)_static || (bool)_native;
        }
        operator Method*() {
            if(_member && _static) {
                throw std::runtime_error("Fake-jni Api Bug, both static and non static functions found");
            }
            if(_member) {
                return (Method*) _member;
            }
            if(_static) {
                return (Method*) _static;
            }
            if(_native) {
                return (Method*) _native;
            }
            return nullptr;
        }
    };
}
#endif

#ifndef JNIVM_METHOD_H_2
#define JNIVM_METHOD_H_2
#include "env.h"
#include "jnitypes.h"
#include "throwable.h"

template<class T> jvalue toJValue(T val);

namespace jnivm {
    namespace impl {
        template<class T> using JNINativeMethodHelper = decltype(JNITypes<T>::ToJNIReturnType( std::declval<ENV*>(), std::declval<T>()));
    }
}

template<class T, class... param> jvalue jnivm::Method::j2invoke(JNIEnv &env, T cl, param ...params) {
    jvalue ret;
    if(native) {
        auto type = signature[signature.find_last_of(')') + 1];
        switch (type) {
        case 'V':
            ((void(*)(JNIEnv*, T, impl::JNINativeMethodHelper<param>...))native)(&env, cl, JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params)...);
            ret = {};
        break;
        case 'Z':
            ret = toJValue(((jboolean(*)(JNIEnv*, T, impl::JNINativeMethodHelper<param>...))native)(&env, cl, JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params)...));
        break;
        case 'B':
            ret = toJValue(((jbyte(*)(JNIEnv*, T, impl::JNINativeMethodHelper<param>...))native)(&env, cl, JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params)...));
        break;
        case 'S':
            ret = toJValue(((jshort(*)(JNIEnv*, T, impl::JNINativeMethodHelper<param>...))native)(&env, cl, JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params)...));
        break;
        case 'C':
            ret = toJValue(((jchar(*)(JNIEnv*, T, impl::JNINativeMethodHelper<param>...))native)(&env, cl, JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params)...));
        break;
        case 'I':
            ret = toJValue(((jint(*)(JNIEnv*, T, impl::JNINativeMethodHelper<param>...))native)(&env, cl, JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params)...));
        break;
        case 'J':
            ret = toJValue(((jlong(*)(JNIEnv*, T, impl::JNINativeMethodHelper<param>...))native)(&env, cl, JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params)...));
        break;
        case 'F':
            ret = toJValue(((jfloat(*)(JNIEnv*, T, impl::JNINativeMethodHelper<param>...))native)(&env, cl, JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params)...));
        break;
        case 'D':
            ret = toJValue(((jdouble(*)(JNIEnv*, T, impl::JNINativeMethodHelper<param>...))native)(&env, cl, JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params)...));
        break;
        case '[':
        case 'L':
            ret = toJValue(((jobject(*)(JNIEnv*, T, impl::JNINativeMethodHelper<param>...))native)(&env, cl, JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params)...));
        break;
        default:
            throw std::runtime_error("Unsupported signature");
        }
    } else {
        jvalue args[sizeof...(params) > 0 ? sizeof...(params) : 1] = { toJValue(JNITypes<param>::ToJNIReturnType(ENV::FromJNIEnv(&env), params))... };
        ret = jinvoke(*ENV::FromJNIEnv(&env), cl, args);
    }
    if((ENV::FromJNIEnv(&env))->current_exception) {
        std::rethrow_exception((ENV::FromJNIEnv(&env))->current_exception->except);
    }
    return ret;
}

template<class... param> jvalue jnivm::Method::invoke(JNIEnv &env, jnivm::Class* cl, param ...params) {
    return j2invoke<jclass>(env, JNITypes<jclass>::ToJNIType(ENV::FromJNIEnv(&env), std::shared_ptr<Class>(cl->shared_from_this(), cl)), params...);
}

template<class... param> jvalue jnivm::Method::invoke(JNIEnv &env, jnivm::Object* obj, param ...params) {
    return j2invoke<jobject>(env, JNITypes<jnivm::Object>::ToJNIType(ENV::FromJNIEnv(&env), obj->shared_from_this()), params...);
}
#endif