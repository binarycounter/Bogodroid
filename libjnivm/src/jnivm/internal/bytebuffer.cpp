#include <jnivm/class.h>
#include "bytebuffer.hpp"
#include <jnivm/bytebuffer.h>
#include <jnivm/jnitypes.h>

using namespace jnivm;

jobject jnivm::NewDirectByteBuffer(JNIEnv *env, void *buffer, jlong capacity) {
    return JNITypes<std::shared_ptr<ByteBuffer>>::ToJNIType(ENV::FromJNIEnv(env), std::make_shared<ByteBuffer>(buffer, capacity));
}
void *jnivm::GetDirectBufferAddress(JNIEnv *env, jobject bytebuffer) {
    return JNITypes<std::shared_ptr<ByteBuffer>>::JNICast(ENV::FromJNIEnv(env), bytebuffer)->buffer;
}
jlong jnivm::GetDirectBufferCapacity(JNIEnv *env, jobject bytebuffer) {
    return JNITypes<std::shared_ptr<ByteBuffer>>::JNICast(ENV::FromJNIEnv(env), bytebuffer)->capacity;
}