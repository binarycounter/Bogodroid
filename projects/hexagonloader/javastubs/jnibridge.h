#ifndef __JNIBRIDGE_H__
#define __JNIBRIDGE_H__

#include "baron/baron.h"

namespace jnivm
{
    namespace bitter
    {
        namespace jnibridge
        {
            class JNIBridge : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("bitter/jnibridge/JNIBridge")
                static std::shared_ptr<jnivm::java::lang::Object> newInterfaceProxy(long j, std::shared_ptr<FakeJni::JArray<FakeJni::JClass>> classes);
            };
        }
    }
}
#endif