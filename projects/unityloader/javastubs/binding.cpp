#include "baron/baron.h"
#include "android.h"
#include "javac.h"
#include "unity.h"

void InitJNIBinding(FakeJni::Jvm *vm)
{
    vm->registerClass<jnivm::java::lang::ClassLoader>();
    vm->registerClass<jnivm::java::lang::StringBuilder>();
    vm->registerClass<jnivm::java::io::InputStream>();
    vm->registerClass<jnivm::java::io::File>();
    // vm->registerClass<jnivm::java::io::IOException>();
    vm->registerClass<jnivm::java::util::Scanner>();
    // vm->registerClass<jnivm::java::util::NoSuchElementException>();
    // vm->registerClass<jnivm::com::unity3d::player::NativeLoader>();
    // vm->registerClass<jnivm::com::unity3d::player::UnityPlayer>();
    // vm->registerClass<jnivm::com::unity3d::player::GoogleARCoreApi>();
    // vm->registerClass<jnivm::com::unity3d::player::Camera2Wrapper>();
    // vm->registerClass<jnivm::com::unity3d::player::HFPStatus>();
    // vm->registerClass<jnivm::com::unity3d::player::AudioVolumeHandler>();
    // vm->registerClass<jnivm::com::unity3d::player::UnityCoreAssetPacksStatusCallbacks>();
    // vm->registerClass<jnivm::com::unity3d::player::OrientationLockListener>();
    // vm->registerClass<jnivm::com::google::androidgamesdk::ChoreographerCallback>();
    // vm->registerClass<jnivm::com::google::androidgamesdk::SwappyDisplayManager>();
    // vm->registerClass<jnivm::bitter::jnibridge::JNIBridge>();
    vm->registerClass<jnivm::android::os::Process>();
    vm->registerClass<jnivm::android::os::Bundle>();
    vm->registerClass<jnivm::android::app::Activity>();
    vm->registerClass<jnivm::android::content::Context>();
    vm->registerClass<jnivm::android::content::Intent>();
    vm->registerClass<jnivm::android::content::res::AssetManager>();

    vm->registerClass<jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper>();

    FakeJni::LocalFrame frame(*vm);
    // java.lang.Class hooks
    auto classClass = vm->findClass("java/lang/Class");
    classClass->HookInstanceFunction(&frame.getJniEnv(), "getClassLoader", [classClass]()
                                     {
        printf("getClassloader called for class %s\n",classClass->getName().c_str());
        return std::make_shared<jnivm::java::lang::ClassLoader>(); });
    classClass->Hook(&frame.getJniEnv(), "forName", [](std::shared_ptr<FakeJni::JString> name, bool init, std::shared_ptr<jnivm::java::lang::ClassLoader> loader)
                     { return std::shared_ptr<FakeJni::JClass>(); });

    // // java.lang.Object hooks
    // auto objectClass=vm->findClass("java/lang/Object");
    // objectClass->HookInstanceFunction(&frame.getJniEnv(),"toString",&jnivm::java::lang::EObject::toString);
    // // java.lang.Throwable hooks
    // auto throwableClass=vm->findClass("java/lang/Throwable");
    // throwableClass->HookInstanceFunction(&frame.getJniEnv(),"toString",&jnivm::java::lang::EThrowable::toString);
}