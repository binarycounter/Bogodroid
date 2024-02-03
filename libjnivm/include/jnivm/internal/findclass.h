#pragma once
#include <jni.h>
#include <memory>

namespace jnivm {
    class Class;
    class ENV;
    jclass InternalFindClass(JNIEnv *env, const char *name, bool returnZero = false, bool trace = false);
    std::shared_ptr<Class> InternalFindClass(ENV *env, const char *name, bool returnZero = false, bool trace = false);
    void Declare(JNIEnv *env, const char *signature);
}