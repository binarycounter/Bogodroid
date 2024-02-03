#include "baron/baron.h"
#include "unity.h"




std::shared_ptr<jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper> jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper::init(std::shared_ptr<jnivm::android::content::Context> context) {
    return std::make_shared<jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper>();
}

bool jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper::playCoreApiMissing() {
    printf("[NATIVE] We don't have Google Play Core APIs, don't even try. \n");
    return true;
}

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper)
{FakeJni::Function<&PlayAssetDeliveryUnityWrapper::init>{}, "init", FakeJni::JMethodID::STATIC },
{FakeJni::Function<&PlayAssetDeliveryUnityWrapper::playCoreApiMissing>{}, "playCoreApiMissing", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR


