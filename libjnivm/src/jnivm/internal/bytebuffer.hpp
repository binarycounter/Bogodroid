#pragma once
#include <jni.h>

namespace jnivm {
    jobject NewDirectByteBuffer(JNIEnv *env, void *buffer, jlong capacity);
    void *GetDirectBufferAddress(JNIEnv *, jobject bytebuffer);
    jlong GetDirectBufferCapacity(JNIEnv *, jobject bytebuffer);
}