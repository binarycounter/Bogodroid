#include <jnivm/class.h>
#include "array.hpp"

using namespace jnivm;

jsize jnivm::GetArrayLength(JNIEnv *env, jarray a) {
    return a ? JNITypes<std::shared_ptr<Array<void>>>::JNICast(ENV::FromJNIEnv(env), a)->getSize() : 0;
}
jobjectArray jnivm::NewObjectArray(JNIEnv * env, jsize length, jclass c, jobject init) {
    auto cl0 = JNITypes<std::shared_ptr<Class>>::JNICast(ENV::FromJNIEnv(env), c);
    auto classname = cl0->nativeprefix[0] == '[' ? "[" + cl0->nativeprefix : "[L" + cl0->nativeprefix + ";";
    auto cl = InternalFindClass(ENV::FromJNIEnv(env), classname.data());
    // auto arr = std::make_shared<Array<Object>>(new std::shared_ptr<Object>[length], length);
    auto arr = cl->InstantiateArray ? cl->InstantiateArray(ENV::FromJNIEnv(env), length) : std::make_shared<Array<Object>>(length);
    arr->clazz = std::move(cl);
    if(init) {
        for (jsize i = 0; i < length; i++) {
            (*arr)[i] = (*(Object*)init).shared_from_this();
        }
    }
    return JNITypes<std::shared_ptr<Array<Object>>>::ToJNIType(ENV::FromJNIEnv(env), arr);
}
jobject jnivm::GetObjectArrayElement(JNIEnv *env, jobjectArray a, jsize i ) {
    return JNITypes<std::shared_ptr<Object>>::ToJNIType(ENV::FromJNIEnv(env), (std::shared_ptr<Object>) (*JNITypes<std::shared_ptr<Array<Object>>>::JNICast(ENV::FromJNIEnv(env), a))[i]);
}
void jnivm::SetObjectArrayElement(JNIEnv *env, jobjectArray a, jsize i, jobject v) {
    (*JNITypes<std::shared_ptr<Array<Object>>>::JNICast(ENV::FromJNIEnv(env), a))[i] = v ? JNITypes<std::shared_ptr<Object>>::JNICast(ENV::FromJNIEnv(env), v) : nullptr;
}

template <class T> typename JNITypes<T>::Array jnivm::NewArray(JNIEnv * env, jsize length) {
    auto arr = length ? std::make_shared<Array<T>>(new T[length] {0}, length) : std::make_shared<Array<T>>(0);
    arr->clazz = InternalFindClass(ENV::FromJNIEnv(env), (std::string("[") + JNITypes<T>::GetJNISignature(ENV::FromJNIEnv(env))).data());
    return JNITypes<std::shared_ptr<Array<T>>>::ToJNIType(ENV::FromJNIEnv(env), arr);
}

template <class T>
T *jnivm::GetArrayElements(JNIEnv *env, typename JNITypes<T>::Array a, jboolean *iscopy) {
    if (iscopy) {
        *iscopy = false;
    }
    return a ? JNITypes<std::shared_ptr<Array<T>>>::JNICast(ENV::FromJNIEnv(env), a)->getArray() : nullptr;
}

template <class T>
void jnivm::ReleaseArrayElements(JNIEnv *, typename JNITypes<T>::Array a, T *carr, jint) {
    // Never copied, never free
}

template <class T>
void jnivm::GetArrayRegion(JNIEnv *env, typename JNITypes<T>::Array a, jsize start, jsize len, T * buf) {
    auto ja = JNITypes<std::shared_ptr<Array<T>>>::JNICast(ENV::FromJNIEnv(env), a);
    memcpy(buf, ja->getArray() + start, sizeof(T)* len);
}

template <class T>
void jnivm::SetArrayRegion(JNIEnv *env, typename JNITypes<T>::Array a, jsize start, jsize len, const T * buf) {
    auto ja = JNITypes<std::shared_ptr<Array<T>>>::JNICast(ENV::FromJNIEnv(env), a);
    memcpy(ja->getArray() + start, buf, sizeof(T)* len);
}

#define DeclareTemplate(type) template j ## type ## Array jnivm::NewArray<j ## type>(JNIEnv *env, jsize length)
DeclareTemplate(boolean);
DeclareTemplate(byte);
DeclareTemplate(short);
DeclareTemplate(int);
DeclareTemplate(long);
DeclareTemplate(float);
DeclareTemplate(double);
DeclareTemplate(char);
#undef DeclareTemplate

#define DeclareTemplate(T) template T *jnivm::GetArrayElements(JNIEnv *, typename JNITypes<T>::Array a, jboolean *iscopy)
DeclareTemplate(jboolean);
DeclareTemplate(jbyte);
DeclareTemplate(jshort);
DeclareTemplate(jint);
DeclareTemplate(jlong);
DeclareTemplate(jfloat);
DeclareTemplate(jdouble);
DeclareTemplate(jchar);
DeclareTemplate(void);
#undef DeclareTemplate

#define DeclareTemplate(T) template void jnivm::ReleaseArrayElements(JNIEnv *, typename JNITypes<T>::Array a, T *carr, jint)
DeclareTemplate(jboolean);
DeclareTemplate(jbyte);
DeclareTemplate(jshort);
DeclareTemplate(jint);
DeclareTemplate(jlong);
DeclareTemplate(jfloat);
DeclareTemplate(jdouble);
DeclareTemplate(jchar);
DeclareTemplate(void);
#undef DeclareTemplate

template void jnivm::GetArrayRegion(JNIEnv *, jbooleanArray a, jsize start, jsize len, jboolean *buf);
template void jnivm::GetArrayRegion(JNIEnv *, jbyteArray a, jsize start, jsize len, jbyte *buf);
template void jnivm::GetArrayRegion(JNIEnv *, jshortArray a, jsize start, jsize len, jshort *buf);
template void jnivm::GetArrayRegion(JNIEnv *, jintArray a, jsize start, jsize len, jint *buf);
template void jnivm::GetArrayRegion(JNIEnv *, jlongArray a, jsize start, jsize len, jlong *buf);
template void jnivm::GetArrayRegion(JNIEnv *, jfloatArray a, jsize start, jsize len, jfloat *buf);
template void jnivm::GetArrayRegion(JNIEnv *, jdoubleArray a, jsize start, jsize len, jdouble *buf);
template void jnivm::GetArrayRegion(JNIEnv *, jcharArray a, jsize start, jsize len, jchar *buf);

template void jnivm::SetArrayRegion(JNIEnv *, jbooleanArray a, jsize start, jsize len, const jboolean *buf);
template void jnivm::SetArrayRegion(JNIEnv *, jbyteArray a, jsize start, jsize len, const jbyte *buf);
template void jnivm::SetArrayRegion(JNIEnv *, jshortArray a, jsize start, jsize len, const jshort *buf);
template void jnivm::SetArrayRegion(JNIEnv *, jintArray a, jsize start, jsize len, const jint *buf);
template void jnivm::SetArrayRegion(JNIEnv *, jlongArray a, jsize start, jsize len, const jlong *buf);
template void jnivm::SetArrayRegion(JNIEnv *, jfloatArray a, jsize start, jsize len, const jfloat *buf);
template void jnivm::SetArrayRegion(JNIEnv *, jdoubleArray a, jsize start, jsize len, const jdouble *buf);
template void jnivm::SetArrayRegion(JNIEnv *, jcharArray a, jsize start, jsize len, const jchar *buf);