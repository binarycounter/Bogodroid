#include "android.h"
#include "baron/baron.h"
#include "fakefmod.h"
#include "gplay.h"
#include "javac.h"
#include "jnibridge.h"
#include "unity.h"

void InitJNIBinding(FakeJni::Jvm* vm)
{

    InitJNIJavaClasses(vm);
    InitJNIAndroidClasses(vm);
    InitJNIGooglePlayClasses(vm);

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

    vm->registerClass<jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper>();
    vm->registerClass<jnivm::com::unity3d::player::UnityPlayerActivity>();
    vm->registerClass<jnivm::com::unity3d::player::UnityPlayer>();
    vm->registerClass<jnivm::com::unity3d::player::ReflectionHelper>();
    vm->registerClass<jnivm::com::unity3d::player::ReflectionHelper::InvocationError>();
    vm->registerClass<jnivm::bitter::jnibridge::JNIBridge>();

    // Fake FMOD

    vm->registerClass<jnivm::org::fmod::FMODAudioDevice>();

    HookStringExtensions(vm);
    HookClassExtensions(vm);
    HookIntExtensions(vm);
    HookObjectExtensions(vm);

    FakeJni::LocalFrame frame(*vm);
    auto classClass = vm->findClass("java/lang/Class");

    // Some Unity specific thing. Should really be in JNIBridge, is cast to Class for some reason... Just return false.
    classClass->HookInstanceFunction(&frame.getJniEnv(), "initializeGoogleAr", [](jnivm::ENV* env, jnivm::Object* self) {
        return false;
    });

    // No idea why this is in Class either....
    classClass->HookInstanceFunction(&frame.getJniEnv(), "getLaunchURL", [](jnivm::ENV* env, jnivm::Object* self) {
        return std::make_shared<FakeJni::JString>("");
    });

    // libjnivm does not have an implementation of Field.getDeclaringClass, and adding one is not trivial. So we hardcode a couple of classes here. Bad hack, but eh.
    auto fieldClass = vm->findClass("java/lang/reflect/Field");
    fieldClass->HookInstanceFunction(&frame.getJniEnv(), "getDeclaringClass", [](jnivm::ENV* env, jnivm::Object* self) -> std::shared_ptr<jnivm::java::lang::Class> {
        if (self == nullptr)
            return nullptr;
        auto selfField = dynamic_cast<jnivm::java::lang::reflect::Field*>(self);
        verbose("getDeclaringClass","%s - %s", selfField->name.c_str(), selfField->type.c_str());
        return nullptr; // TODO: Implement this beyond just logging. Unity looks for it, but doesn't actually call it.
    });
}