#include "baron/baron.h"
#include "unity.h"
#include "logging.h"
#include "../globals.h"

///// UnityPlayerActivity

bool jnivm::com::unity3d::player::UnityPlayerActivity::injectEvent(std::shared_ptr<android::view::InputEvent> event) {
    // This is the C++ equivalent of the `mUnityPlayer.injectEvent(event)` call,
    // which in turn calls the native function.
    
    verbose("UnityPlayerActivity", "Injecting input event into native engine.");

    FakeJni::LocalFrame frame(vm);
    
    auto unityPlayerClass = vm.findClass("com/unity3d/player/UnityPlayer").get();
    if (!unityPlayerClass) {
        verbose("UnityPlayerActivity", "Could not find class com/unity3d/player/UnityPlayer");
        return false;
    }

    // Find the static native method
    auto method = unityPlayerClass->getMethod("(Landroid/view/InputEvent;)Z", "nativeInjectEvent");
    if(!method) {
        verbose("UnityPlayerActivity", "Could not find native method nativeInjectEvent");
        return false;
    }
    
    // Call the static native method, passing the event object.
    auto result = method.invoke(frame.getJniEnv(), unityPlayerClass, event).z;
    verbose("UnityPlayerActivity", "Result: %d",result);
    return result == JNI_TRUE;
}

///// PlayAssetDeliveryUnityWrapper

std::shared_ptr<jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper> jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper::init(std::shared_ptr<jnivm::android::content::Context> context) {
    return std::make_shared<jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper>();
}

bool jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper::playCoreApiMissing() {
    printf("[NATIVE] We don't have Google Play Core APIs, don't even try. \n");
    return true;
}

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper){ FakeJni::Constructor<PlayAssetDeliveryUnityWrapper> {} },
{FakeJni::Function<&PlayAssetDeliveryUnityWrapper::init>{}, "init", FakeJni::JMethodID::STATIC },
{FakeJni::Function<&PlayAssetDeliveryUnityWrapper::playCoreApiMissing>{}, "playCoreApiMissing", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::UnityPlayerActivity)
END_NATIVE_DESCRIPTOR


