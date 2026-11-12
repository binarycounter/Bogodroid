#include <fake-jni/fake-jni.h>
#include "../jnivm/internal/method.h"

using namespace jnivm;

MethodProxy Class::getMethod(const char *sig, const char *name) {
    jmethodID _member = GetMethodID<false, true, false, false>(FakeJni::JniEnv::getCurrentEnv(), (jclass)(Object*)this, name, sig);
    jmethodID _static = GetMethodID<true, true, false, false>(FakeJni::JniEnv::getCurrentEnv(), (jclass)(Object*)this, name, sig);
    if(!_member && !_static) {
        return { nullptr, nullptr, GetMethodID<false, true, true>(FakeJni::JniEnv::getCurrentEnv(), (jclass)(Object*)this, name, sig)};
    }
    return { _member, _static, nullptr };
}