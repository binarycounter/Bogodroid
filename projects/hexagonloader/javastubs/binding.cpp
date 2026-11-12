#include "android.h"
#include "baron/baron.h"
#include "javac.h"
#include "ofandroid.h"

void InitJNIBinding(FakeJni::Jvm* vm)
{
    InitJNIJavaClasses(vm);
    InitJNIAndroidClasses(vm);

    vm->registerClass<jnivm::cc::openframeworks::OFAndroid>();
}