#pragma once
#include <jnivm/array.h>
#include <jnivm/jnitypes.h>

namespace jnivm {

    jsize GetArrayLength(JNIEnv *, jarray a);
    jobjectArray NewObjectArray(JNIEnv * env, jsize length, jclass c, jobject init);
    jobject GetObjectArrayElement(JNIEnv *env, jobjectArray a, jsize i );
    void SetObjectArrayElement(JNIEnv *, jobjectArray a, jsize i, jobject v);
    template <class T> typename JNITypes<T>::Array NewArray(JNIEnv * env, jsize length);
    template <class T>
    T *GetArrayElements(JNIEnv *, typename JNITypes<T>::Array a, jboolean *iscopy);
    template <class T>
    void ReleaseArrayElements(JNIEnv *, typename JNITypes<T>::Array a, T *carr, jint);
    template <class T>
    void GetArrayRegion(JNIEnv *, typename JNITypes<T>::Array a, jsize start, jsize len, T * buf);
    template <class T>
    void SetArrayRegion(JNIEnv *, typename JNITypes<T>::Array a, jsize start, jsize len, const T * buf);
}