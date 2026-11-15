#pragma once
#include <jni.h>

namespace jnivm {

    template<bool isStatic, bool ReturnNull=false, bool trace = true>
    jfieldID GetFieldID(JNIEnv *env, jclass cl, const char *name, const char *type);

    template <bool RetNull, class T, class O> T GetField(JNIEnv *env, O obj, jfieldID id);
    template <class T, class O> void SetField(JNIEnv *env, O obj, jfieldID id, T value);
}