#include <fake-jni/jvm.h>
#include <stdexcept>

using namespace FakeJni;

FakeJni::Jvm::Jvm(bool skipInit, bool returnNull) : VM(true, returnNull) {
    functions = GetJavaVM()->functions;
    if(!skipInit) {
        initialize();
    }
}

FakeJni::Jvm::Jvm() : Jvm(false, true) {

    // oldinterface = *GetJavaVM()->functions;
    // JNIInvokeInterface patchinterface = *GetJavaVM()->functions;
    // patchinterface.reserved1 = this;
    // // patchinterface.AttachCurrentThread = [](JavaVM *vm, JNIEnv **env, void *reserved) {
    // //     auto jvm = static_cast<Jvm*>(vm->functions->reserved1);
    // //     auto ret = jvm->oldinterface.AttachCurrentThread(vm, env, reserved);
    // //     return ret;
    // // };
    // OverrideJNIInvokeInterface(patchinterface);
}

std::shared_ptr<jnivm::ENV> FakeJni::Jvm::CreateEnv() {
    if(FakeJni::JniEnvContext::env.env.lock()) {
        throw std::runtime_error("Attempt to initialize a FakeJni::Env twice in one thread!");
    }
    auto tmpl = GetNativeInterfaceTemplate();
	for(auto && hook : jnienvhooks) {
        hook(tmpl);
    }
    auto ret = std::make_shared<Env>(*this, static_cast<jnivm::VM*>(this), tmpl);
    FakeJni::JniEnvContext::env.env = ret;
    return std::shared_ptr<jnivm::ENV>(ret, jnivm::ENV::FromJNIEnv(ret.get()));
}

std::vector<std::shared_ptr<jnivm::Class>> FakeJni::Jvm::getClasses() {
    std::vector<std::shared_ptr<jnivm::Class>> ret;
    for(auto&& c : classes) {
        ret.emplace_back(c.second);
    }
    return ret;
}