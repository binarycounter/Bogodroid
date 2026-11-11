#include "so_util.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <mutex>
#include <set>
#include "unity.h"
#include "../globals.h"
#include "baron/baron.h"
#include "jnibridge.h"
#include "logging.h"

///// UnityPlayer
std::shared_ptr<jnivm::android::app::Activity> jnivm::com::unity3d::player::UnityPlayer::currentActivity = nullptr;

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

    // Newer Unity: nativeInjectEvent(InputEvent, int). The int is passed as 0 (probably a display/window index).
    if (auto method2 = unityPlayerClass->getMethod("(Landroid/view/InputEvent;I)Z", "nativeInjectEvent")) {
        auto result = method2.invoke(frame.getJniEnv(), unityPlayerClass, event, 0).z;
        verbose("UnityPlayerActivity", "Result: %d", result);
        return result == JNI_TRUE;
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
    verbose("PlayAssetDeliveryUnityWrapper", "We don't have Google Play Core APIs, don't even try. \n");
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

void jnivm::com::unity3d::player::UnityPlayer::executeMainThreadJobs()
{
    // The loader's own loop already runs the main thread; there are no queued Java-side jobs.
}

bool jnivm::com::unity3d::player::UnityPlayer::shouldSetGameState()
{
    return false;
}

bool jnivm::com::unity3d::player::UnityPlayer::loadLibrary(std::shared_ptr<FakeJni::JString> path)
{
    // A library name ("_burst_generated", like System.loadLibrary) or a path (like System.load). Loads it if the
    // game ships it and it isn't loaded yet, and calls its JNI_OnLoad (Unity's own libraries are already set up).
    if (!path) return false;
    so_module* mod = so_load_library(path->c_str(), &vm);
    if (!mod) verbose("JBRIDGE", "UnityPlayer.loadLibrary(%s): not shipped with the game", path->c_str());
    return mod != nullptr;
}



///// ReflectionHelper

std::shared_ptr<jnivm::java::lang::reflect::Constructor> jnivm::com::unity3d::player::ReflectionHelper::getConstructorID(std::shared_ptr<jnivm::java::lang::Class> clazz, std::shared_ptr<FakeJni::JString> signature)
{
    if (clazz == nullptr || signature == nullptr)
        return nullptr;
    verbose("UnityReflection", "getConstructorID(%s, %s) \n", clazz->getName().c_str(), signature.get()->c_str());
    // Constructors are registered as "<init>" methods. As with getMethodID, the jnivm Method object itself stands in
    // for the reflection object: FromReflectedMethod() turns the handle straight back into the method.
    auto ctor = (Method*)clazz->getMethod(signature.get()->c_str(), "<init>");
    if (ctor == nullptr)
        return nullptr;
    return std::shared_ptr<jnivm::java::lang::reflect::Constructor>(
        reinterpret_cast<jnivm::java::lang::reflect::Constructor*>(ctor),
        [](jnivm::java::lang::reflect::Constructor*) { } // No-op deleter: the class owns the method
    );
}

std::shared_ptr<jnivm::java::lang::reflect::Method> jnivm::com::unity3d::player::ReflectionHelper::getMethodID(std::shared_ptr<jnivm::java::lang::Class> clazz, std::shared_ptr<FakeJni::JString> methodName, std::shared_ptr<FakeJni::JString> signature, bool isStatic)
{
    if(clazz == nullptr)
        return nullptr;

    const char* name = methodName.get()->c_str();
    const char* sig;

    // Method 1: Search for matching methods in class by name only (bail out if there is ambiguity due to duplicates)

    // The class and its ancestors (e.g. UnityPlayerActivity -> Activity -> Context), most derived first, so inherited
    // methods are found too; an override with the same signature isn't an ambiguity.
    std::shared_ptr<Method> foundMethod=nullptr;
    std::vector<std::shared_ptr<Method>> sameName; // every method of that name in the class and its ancestors
    bool duplicate=false;
    {
        auto* env = jnivm::ENV::FromJNIEnv(FakeJni::JniEnv::getCurrentEnv());
        std::vector<std::shared_ptr<jnivm::Class>> pending { std::static_pointer_cast<jnivm::Class>(clazz) };
        std::set<jnivm::Class*> seen;
        while (!pending.empty()) {
            auto c = pending.front();
            pending.erase(pending.begin());
            if (!c || !seen.insert(c.get()).second)
                continue;
            for(std::shared_ptr<Method> method : c->methods)
            {
                if(strcmp(method->name.c_str(), name) != 0)
                    continue;
                sameName.push_back(method);
                if(foundMethod == nullptr)
                    foundMethod=method;
                else if(foundMethod->signature != method->signature)
                    duplicate=true;
            }
            if (c->baseclasses)
                for (auto& b : c->baseclasses(env))
                    pending.push_back(b);
        }
    }

    if(foundMethod != nullptr && !duplicate)
    {
        auto method = std::shared_ptr<Method>(
        (Method*)clazz->getMethod(foundMethod->signature.c_str(), name),
        [](Method*) { } // No-op deleter
        );
        verbose("UnityReflection", "getMethodID(type 1, %s, %s, %s, %d) = 0x%p \n", clazz->getName().c_str(), name, foundMethod->signature.c_str(), isStatic, method.get());
        return method;
    }

    // Method 2: Hardcoded fixes for the signature inaccuracies, then use getMethod

    if (strcmp("initialize", name) == 0 && strcmp(clazz->getName().c_str(), "com/google/android/gms/games/PlayGamesSdk") == 0)
        sig = "(Landroid/content/Context;)V";
    else if (strcmp("getGamesSignInClient", name) == 0)
        sig = "(Landroid/app/Activity;)Lcom/google/android/gms/games/GamesSignInClient;";
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
    if (!method.get() && !sameName.empty()) {
        // Unity builds the signature from the runtime classes of the arguments and a generic return type, so it rarely
        // matches a registered overload exactly. Fall back to: same parameter list (ignoring the return type), else
        // the only overload with that many parameters.
        auto params = [](const std::string& s) { size_t e = s.find(')'); return e == std::string::npos ? s : s.substr(0, e + 1); };
        auto paramCount = [&](const std::string& s) {
            int n = 0;
            for (size_t i = 1; i < s.size() && s[i] != ')'; i++) {
                while (s[i] == '[') i++;
                if (s[i] == 'L') i = s.find(';', i);
                if (i == std::string::npos) return -1;
                n++;
            }
            return n;
        };
        std::string want = sig;
        std::shared_ptr<Method> pick;
        int exact = 0, sameCount = 0;
        std::shared_ptr<Method> countPick;
        for (auto& m : sameName) {
            if (params(m->signature) == params(want)) { pick = m; exact++; }
            if (paramCount(m->signature) == paramCount(want)) { countPick = m; sameCount++; }
        }
        if (exact == 1 || (exact == 0 && sameCount == 1)) {
            auto chosen = exact == 1 ? pick : countPick;
            method = std::shared_ptr<Method>(
                (Method*)clazz->getMethod(chosen->signature.c_str(), name),
                [](Method*) { });
            verbose("UnityReflection", "getMethodID: %s%s not registered exactly, using %s", name, sig, chosen->signature.c_str());
        }
    }
    verbose("UnityReflection", "getMethodID(type 2, %s, %s, %s, %d) = 0x%p \n", clazz->getName().c_str(), name, sig, isStatic, method.get());
    return method;
}
// Field -> the class it was looked up on, for Field.getDeclaringClass() (jnivm's Field doesn't know its class).
static std::mutex g_fieldOwnersMutex;
static std::unordered_map<const jnivm::Field*, std::weak_ptr<jnivm::java::lang::Class>> g_fieldOwners;

std::shared_ptr<jnivm::java::lang::Class> unity_field_declaring_class(const jnivm::Field* field)
{
    std::lock_guard<std::mutex> lock(g_fieldOwnersMutex);
    auto it = g_fieldOwners.find(field);
    return it == g_fieldOwners.end() ? nullptr : it->second.lock();
}

std::shared_ptr<jnivm::java::lang::reflect::Field> jnivm::com::unity3d::player::ReflectionHelper::getFieldID(std::shared_ptr<jnivm::java::lang::Class> clazz, std::shared_ptr<FakeJni::JString> fieldName, std::shared_ptr<FakeJni::JString> signature, bool isStatic)
{
    const char* name = fieldName.get()->c_str();
    const char* sig;

    if (strcmp("currentActivity", name) == 0 && strcmp(clazz->getName().c_str(), "com/unity3d/player/UnityPlayer") == 0)
        sig = "Lcom/unity3d/player/UnityPlayerActivity;";
    else
        sig = signature.get()->c_str();

    // UnityPlayer.currentActivity is registered as android/app/Activity; Unity 7 asks for it as UnityPlayerActivity.
    // Match by name alone: getFieldSignature() then hands back the registered type, which the later
    // GetStaticFieldID lookup accepts.
    const bool anyType = strcmp("currentActivity", name) == 0 && strcmp(clazz->getName().c_str(), "com/unity3d/player/UnityPlayer") == 0;
    for (auto field : clazz->fields) {
        if (field->name == name && (anyType || field->type == sig)) {
            {
                std::lock_guard<std::mutex> lock(g_fieldOwnersMutex);
                g_fieldOwners[field.get()] = clazz;
            }
            verbose("UnityReflection", "getFieldID(%s, %s, %s, %d) = %p \n", clazz->getName().c_str(), fieldName.get()->c_str(), sig, isStatic, field);
            return field;
        }
    }

    verbose("UnityReflection", "getFieldID(%s, %s, %s, %d) = null \n", clazz->getName().c_str(), fieldName.get()->c_str(), sig, isStatic);
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
    if (interface == nullptr)
        return nullptr;
    verbose("UnityReflection", "newProxyInstance(%p, %ld, %s) \n", player.get(), nativeHandle, interface.get()->getName().c_str());
    // Same proxy type JNIBridge.newInterfaceProxy hands out: native callbacks arrive through JNIBridge.invoke.
    return std::make_shared<jnivm::bitter::jnibridge::JNIBridgeProxy>(nativeHandle, std::set<std::string>{interface->getName()});
}

std::shared_ptr<jnivm::Object> jnivm::com::unity3d::player::ReflectionHelper::createInvocationError(long nativeHandle, bool toggle)
{
    verbose("UnityReflection", "createInvocationError(%ld, %d)\n", nativeHandle, toggle);
    return std::make_shared<jnivm::com::unity3d::player::ReflectionHelper::InvocationError>(nativeHandle, toggle);
}

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::PlayAssetDeliveryUnityWrapper) { FakeJni::Constructor<PlayAssetDeliveryUnityWrapper> {} },
    { FakeJni::Function<&PlayAssetDeliveryUnityWrapper::init> {}, "init", FakeJni::JMethodID::STATIC },
    { FakeJni::Function<&PlayAssetDeliveryUnityWrapper::playCoreApiMissing> {}, "playCoreApiMissing", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::UnityPlayerUtilities) { FakeJni::Constructor<UnityPlayerUtilities> {} },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::UnityPlayerActivity) { FakeJni::Constructor<UnityPlayerActivity> {} },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::UnityPlayer) { FakeJni::Constructor<UnityPlayer> {} },
    { FakeJni::Field<&UnityPlayer::currentActivity> {}, "currentActivity", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&UnityPlayer::initializeGoogleAr> {}, "initializeGoogleAr", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&UnityPlayer::getLaunchURL> {}, "getLaunchURL", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&UnityPlayer::executeMainThreadJobs> {}, "executeMainThreadJobs", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&UnityPlayer::shouldSetGameState> {}, "shouldSetGameState", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&UnityPlayer::loadLibrary> {}, "loadLibrary", FakeJni::JMethodID::PUBLIC },
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
