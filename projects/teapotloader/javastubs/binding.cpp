#include "baron/baron.h"
#include "android.h"
#include "javac.h"
#include "teapot.h"


void InitJNIBinding(FakeJni::Jvm *vm)
{

    InitJNIJavaClasses(vm);
    InitJNIAndroidClasses(vm);

    vm->registerClass<jnivm::com::sample::teapot::TeapotNativeActivity>();

    HookStringExtensions(vm);
    HookClassExtensions(vm);
    HookObjectExtensions(vm);
}