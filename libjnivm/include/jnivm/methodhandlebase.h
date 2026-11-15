#pragma once
#include <jni.h>
#include <stdexcept>

namespace jnivm {
    class Class;
    class ENV;

    namespace impl {
        template<class T> class MethodHandleBase {
        public:
            virtual T InstanceInvoke(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<T>) {
                throw std::runtime_error("Mismatched MethodHandle!");
            }
            virtual T InstanceGet(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<T>) {
                throw std::runtime_error("Mismatched MethodHandle!");
            }
            virtual T NonVirtualInstanceInvoke(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<T>) {
                throw std::runtime_error("Mismatched MethodHandle!");
            }
            virtual T NonVirtualInstanceGet(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<T>) {
                throw std::runtime_error("Mismatched MethodHandle!");
            }
            virtual T StaticInvoke(ENV * env, Class* clazz, const jvalue* values, MethodHandleBase<T>) {
                throw std::runtime_error("Mismatched MethodHandle!");
            }
            virtual T StaticGet(ENV * env, Class* clazz, const jvalue* values, MethodHandleBase<T>) {
                throw std::runtime_error("Mismatched MethodHandle!");
            }
            virtual void InstanceSet(ENV * env, jobject obj, const jvalue* values, MethodHandleBase<T>) {
                throw std::runtime_error("Mismatched MethodHandle!");
            }
            virtual void StaticSet(ENV * env, Class* clazz, const jvalue* values, MethodHandleBase<T>) {
                throw std::runtime_error("Mismatched MethodHandle!");
            }
        };

        template<class...T>
        class MethodHandle : public MethodHandleBase<T>... {
        public:

        };
    }
    using MethodHandle = impl::MethodHandle<jobject, jboolean, jbyte, jchar, jshort, jint, jlong, jfloat, jdouble, void>;
}