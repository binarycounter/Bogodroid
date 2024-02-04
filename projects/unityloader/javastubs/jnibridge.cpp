#include "baron/baron.h"
#include "platform.h"
#include "jnibridge.h"



std::shared_ptr<jnivm::java::lang::Object> jnivm::bitter::jnibridge::JNIBridge::newInterfaceProxy(FakeJni::JLong j, std::shared_ptr<jnivm::Array<FakeJni::JClass>> classes)
{

    verbose("JBRIDGE", "JNIBridge newInterfaceProxy %d classes",classes.get()->getSize());
    for(int i=0; i<classes.get()->getSize(); i++)
    {
       verbose("JBRIDGE", "JNIBridge newInterfaceProxy %s",(*classes)[i]->getName().c_str()); 
    }

    return nullptr;
}


BEGIN_NATIVE_DESCRIPTOR(jnivm::bitter::jnibridge::JNIBridge)
{FakeJni::Function<&JNIBridge::newInterfaceProxy>{}, "newInterfaceProxy", FakeJni::JMethodID::STATIC },
END_NATIVE_DESCRIPTOR


