#pragma once
#include "object.h"
#include <jni.h>

namespace jnivm {
    class ByteBuffer : public Object {
    public:
        ByteBuffer(void* buffer, jlong capacity) : buffer(buffer), capacity(capacity) {

        }
        void* buffer;
        jlong capacity;
    };
}