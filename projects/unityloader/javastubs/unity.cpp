#include "unity.h"
#include "../globals.h"
#include "baron/baron.h"
#include "logging.h"

///// UnityPlayer
std::shared_ptr<jnivm::com::unity3d::player::UnityPlayerActivity> jnivm::com::unity3d::player::UnityPlayer::currentActivity = nullptr;

bool jnivm::com::unity3d::player::UnityPlayerActivity::injectEvent(std::shared_ptr<android::view::InputEvent> event)
{
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
    if (!method) {
        verbose("UnityPlayerActivity", "Could not find native method nativeInjectEvent");
        return false;
    }

    // Call the static native method, passing the event object.
    auto result = method.invoke(frame.getJniEnv(), unityPlayerClass, event).z;
    verbose("UnityPlayerActivity", "Result: %d", result);
    return result == JNI_TRUE;
}

///// PlayAssetDeliveryUnityWrapper

std::shared_ptr<jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper> jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper::init(std::shared_ptr<jnivm::android::content::Context> context)
{
    return std::make_shared<jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper>();
}

bool jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper::playCoreApiMissing()
{
    printf("[UNITYJNI] We don't have Google Play Core APIs, don't even try. \n");
    return true;
}

///// UnityPlayer

bool jnivm::com::unity3d::player::UnityPlayer::initializeGoogleAr()
{
    return false; 
}

std::shared_ptr<FakeJni::JString> jnivm::com::unity3d::player::UnityPlayer::getLaunchURL()
{
    return std::make_shared<FakeJni::JString>("");
}



///// ReflectionHelper

std::shared_ptr<jnivm::java::lang::reflect::Constructor> jnivm::com::unity3d::player::ReflectionHelper::getConstructorID(std::shared_ptr<jnivm::java::lang::Class> clazz, std::shared_ptr<FakeJni::JString> signature)
{
    printf("[UNITYJNI] getConstructorID(%s, %s) \n", clazz->getName().c_str(), signature.get()->c_str());
    return nullptr;
}

std::shared_ptr<jnivm::java::lang::reflect::Method> jnivm::com::unity3d::player::ReflectionHelper::getMethodID(std::shared_ptr<jnivm::java::lang::Class> clazz, std::shared_ptr<FakeJni::JString> methodName, std::shared_ptr<FakeJni::JString> signature, bool isStatic)
{
    const char* name = methodName.get()->c_str();
    const char* sig;

    if (strcmp("initialize", name) == 0 && strcmp(clazz->getName().c_str(), "com/google/android/gms/games/PlayGamesSdk") == 0)
        sig = "(Landroid/content/Context;)V";
    else if (strcmp("create", name) == 0 && strcmp(clazz->getName().c_str(), "com/google/android/play/core/review/ReviewManagerFactory") == 0)
        sig = "(Landroid/content/Context;)Lcom/google/android/play/core/review/ReviewManager;";
    else if (strcmp("getClass", name) == 0)
        sig = "()Ljava/lang/Class;";
    else
        sig = signature.get()->c_str();

    auto method = std::shared_ptr<Method>(
        (Method*)clazz->getMethod(sig, name),
        [](Method*) { } // No-op deleter
    );
    printf("[UNITYJNI] getMethodID(%s, %s, %s, %d) = 0x%p \n", clazz->getName().c_str(), name, sig, isStatic, method.get());
    return method;
}
std::shared_ptr<jnivm::java::lang::reflect::Field> jnivm::com::unity3d::player::ReflectionHelper::getFieldID(std::shared_ptr<jnivm::java::lang::Class> clazz, std::shared_ptr<FakeJni::JString> fieldName, std::shared_ptr<FakeJni::JString> signature, bool isStatic)
{
    const char* name = fieldName.get()->c_str();
    const char* sig;

    if (strcmp("currentActivity", name) == 0 && strcmp(clazz->getName().c_str(), "com/unity3d/player/UnityPlayer") == 0)
        sig = "Lcom/unity3d/player/UnityPlayerActivity;";
    else
        sig = signature.get()->c_str();

    for (auto field : clazz->fields) {
        if (field->name == name && field->type == sig) {
            printf("[UNITYJNI] getFieldID(%s, %s, %s, %d) = %p \n", clazz->getName().c_str(), fieldName.get()->c_str(), sig, isStatic, field);
            return field;
        }
    }

    printf("[UNITYJNI] getFieldID(%s, %s, %s, %d) = null \n", clazz->getName().c_str(), fieldName.get()->c_str(), sig, isStatic);
    return nullptr;
}

std::shared_ptr<FakeJni::JString> jnivm::com::unity3d::player::ReflectionHelper::getFieldSignature(std::shared_ptr<jnivm::java::lang::reflect::Field> field)
{
    if(field == nullptr)
        return nullptr;
    return std::make_shared<FakeJni::JString>(field->type);
}

std::shared_ptr<jnivm::Object> jnivm::com::unity3d::player::ReflectionHelper::newProxyInstance(std::shared_ptr<UnityPlayer> player, long nativeHandle, std::shared_ptr<jnivm::Class> interface)
{
    printf("[UNITYJNI] newProxyInstance(%p, %ld, %s) \n", player.get(), nativeHandle, interface.get()->getName().c_str());
    return nullptr;
}

std::shared_ptr<jnivm::Object> jnivm::com::unity3d::player::ReflectionHelper::createInvocationError(long nativeHandle, bool toggle)
{
    printf("[UNITYJNI] createInvocationError(%ld, %d)\n", nativeHandle, toggle);
    return std::make_shared<jnivm::com::unity3d::player::ReflectionHelper::InvocationError>(nativeHandle, toggle);
}

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper) { FakeJni::Constructor<PlayAssetDeliveryUnityWrapper> {} },
    { FakeJni::Function<&PlayAssetDeliveryUnityWrapper::init> {}, "init", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&PlayAssetDeliveryUnityWrapper::playCoreApiMissing> {}, "playCoreApiMissing", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::UnityPlayerActivity) { FakeJni::Constructor<UnityPlayerActivity> {} },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::UnityPlayer) { FakeJni::Constructor<UnityPlayer> {} },
    { FakeJni::Field<&UnityPlayer::currentActivity> {}, "currentActivity", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&UnityPlayer::initializeGoogleAr> {}, "initializeGoogleAr", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&UnityPlayer::getLaunchURL> {}, "getLaunchURL", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::ReflectionHelper) { FakeJni::Constructor<ReflectionHelper> {} },
    { FakeJni::Function<&ReflectionHelper::getConstructorID> {}, "getConstructorID", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&ReflectionHelper::getMethodID> {}, "getMethodID", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&ReflectionHelper::getFieldID> {}, "getFieldID", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&ReflectionHelper::getFieldSignature> {}, "getFieldSignature", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&ReflectionHelper::newProxyInstance> {}, "newProxyInstance", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&ReflectionHelper::createInvocationError> {}, "createInvocationError", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::ReflectionHelper::InvocationError) { FakeJni::Constructor<InvocationError, long, bool> {} },
    END_NATIVE_DESCRIPTOR
