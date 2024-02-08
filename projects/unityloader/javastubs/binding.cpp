#include "baron/baron.h"
#include "android.h"
#include "javac.h"
#include "unity.h"
#include "jnibridge.h"

void InitJNIBinding(FakeJni::Jvm *vm)
{
    vm->registerClass<jnivm::java::lang::ClassLoader>();
    vm->registerClass<jnivm::java::lang::StringBuilder>();
    vm->registerClass<jnivm::java::io::InputStream>();
    vm->registerClass<jnivm::java::io::File>();
    vm->registerClass<jnivm::java::util::Map>();
    vm->registerClass<jnivm::java::util::Set>();
    vm->registerClass<jnivm::java::util::Iterator>();
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
    vm->registerClass<jnivm::android::view::Display>();
    vm->registerClass<jnivm::android::hardware::display::DisplayManager>();
    vm->registerClass<jnivm::android::os::Environment>();
    vm->registerClass<jnivm::android::os::Looper>();
    vm->registerClass<jnivm::android::os::Handler>();
    vm->registerClass<jnivm::android::os::Process>();
    vm->registerClass<jnivm::android::os::Bundle>();
    vm->registerClass<jnivm::android::app::Activity>();
    vm->registerClass<jnivm::android::content::Context>();
    vm->registerClass<jnivm::android::content::SharedPreferences>();
    vm->registerClass<jnivm::android::content::SharedPreferencesEditor>();
    vm->registerClass<jnivm::android::content::Intent>();
    vm->registerClass<jnivm::android::content::res::AssetManager>();
    vm->registerClass<jnivm::android::content::pm::PackageManager>();
    vm->registerClass<jnivm::android::content::pm::PackageInfo>();
    vm->registerClass<jnivm::android::content::pm::ApplicationInfo>();

    vm->registerClass<jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper>();
    vm->registerClass<jnivm::bitter::jnibridge::JNIBridge>();
    


    HookStringExtensions(vm);
    HookClassExtensions(vm);
    HookObjectExtensions(vm);

    FakeJni::LocalFrame frame(*vm);
    auto classClass = vm->findClass("java/lang/Class");

    // Some Unity specific thing. Should really be in JNIBridge, is cast to Class for some reason... Just return false.
    classClass->HookInstanceFunction(&frame.getJniEnv(), "initializeGoogleAr", [](jnivm::ENV*env, jnivm::Object*self)
    {
        return false;
    });
}