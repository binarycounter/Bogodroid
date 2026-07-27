#include "baron/baron.h"
#include "android.h"
#include "javac.h"
#include "layton.h"

void InitJNIBinding(FakeJni::Jvm *vm)
{
    InitJNIJavaClasses(vm);
    InitJNIAndroidClasses(vm);

    vm->registerClass<jnivm::com::Level5::LT1R::MainActivity>();

    HookStringExtensions(vm);
    HookClassExtensions(vm);
    HookObjectExtensions(vm);
}
