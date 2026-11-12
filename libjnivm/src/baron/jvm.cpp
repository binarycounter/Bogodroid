#include <baron/baron.h>
#include <iostream>
#include <stdexcept>

using namespace Baron;

Baron::Jvm::Jvm() : FakeJni::Jvm(true, false) {
    initialize();
}

Baron::Jvm::Jvm(std::function<void(Jvm*)> setup) : FakeJni::Jvm(true, false) {
    setup(this);
    initialize();
}

std::shared_ptr<jnivm::ENV> Baron::Jvm::CreateEnv() {
    if(FakeJni::JniEnvContext::env.env.lock()) {
        throw std::runtime_error("Attempt to initialize a FakeJni::Env twice in one thread!");
    }
    auto tmpl = GetNativeInterfaceTemplate();
    for(auto && hook : jnienvhooks) {
        hook(tmpl);
    }
    tmpl.FindClass = [](JNIEnv *env, const char *name) -> jclass {
        jclass ret = GetNativeInterfaceTemplate<true>().FindClass(env, name);
        if(ret)
            return ret;
        if(!static_cast<Baron::Jvm*>(jnivm::ENV::FromJNIEnv(env)->GetVM())->isClassDenied(name)) {
            return GetNativeInterfaceTemplate<false>().FindClass(env, name);
            // return jnivm::ENV::FromJNIEnv(env)->GetNativeInterface().FindClass(env, name);
        }
        return nullptr;
    };
    tmpl.GetMethodID = [](JNIEnv *env, jclass c, const char *n, const char *s) -> jmethodID {
        jmethodID ret = GetNativeInterfaceTemplate<true>().GetMethodID(env, c, n, s);
        if(ret)
            return ret;
        auto cl = jnivm::JNITypes<std::shared_ptr<jnivm::Class>>::JNICast(jnivm::ENV::FromJNIEnv(env), c);
        if(!static_cast<Baron::Jvm*>(jnivm::ENV::FromJNIEnv(env)->GetVM())->isMethodDenied(n, s, cl ? cl->nativeprefix.data() : "")) {
            return GetNativeInterfaceTemplate<false>().GetMethodID(env, c, n, s);
        }
        return nullptr;
    };
    tmpl.GetStaticMethodID = [](JNIEnv *env, jclass c, const char *n, const char *s) -> jmethodID {
        jmethodID ret = GetNativeInterfaceTemplate<true>().GetStaticMethodID(env, c, n, s);
        if(ret)
            return ret;
        auto cl = jnivm::JNITypes<std::shared_ptr<jnivm::Class>>::JNICast(jnivm::ENV::FromJNIEnv(env), c);
        if(!static_cast<Baron::Jvm*>(jnivm::ENV::FromJNIEnv(env)->GetVM())->isMethodDenied(n, s, cl ? cl->nativeprefix.data() : "")) {
            return GetNativeInterfaceTemplate<false>().GetStaticMethodID(env, c, n, s);
        }
        return nullptr;
    };
    tmpl.GetFieldID = [](JNIEnv *env, jclass c, const char *n, const char *s) -> jfieldID  {
        jfieldID  ret = GetNativeInterfaceTemplate<true>().GetFieldID(env, c, n, s);
        if(ret)
            return ret;
        auto cl = jnivm::JNITypes<std::shared_ptr<jnivm::Class>>::JNICast(jnivm::ENV::FromJNIEnv(env), c);
        if(!static_cast<Baron::Jvm*>(jnivm::ENV::FromJNIEnv(env)->GetVM())->isFieldDenied(n, s, cl ? cl->nativeprefix.data() : "")) {
            return GetNativeInterfaceTemplate<false>().GetFieldID(env, c, n, s);
        }
        return nullptr;
    };
    tmpl.GetStaticFieldID = [](JNIEnv *env, jclass c, const char *n, const char *s) -> jfieldID  {
        jfieldID  ret = GetNativeInterfaceTemplate<true>().GetStaticFieldID(env, c, n, s);
        if(ret)
            return ret;
        auto cl = jnivm::JNITypes<std::shared_ptr<jnivm::Class>>::JNICast(jnivm::ENV::FromJNIEnv(env), c);
        if(!static_cast<Baron::Jvm*>(jnivm::ENV::FromJNIEnv(env)->GetVM())->isFieldDenied(n, s, cl ? cl->nativeprefix.data() : "")) {
            return GetNativeInterfaceTemplate<false>().GetStaticFieldID(env, c, n, s);
        }
        return nullptr;
    };
    
    auto ret = std::make_shared<FakeJni::Env>(*this, static_cast<jnivm::VM*>(this), tmpl);
    FakeJni::JniEnvContext::env.env = ret;
    return std::shared_ptr<jnivm::ENV>(ret, jnivm::ENV::FromJNIEnv(ret.get()));
}

void Jvm::printStatistics() {
#if defined(JNI_DEBUG) && defined(JNIVM_FAKE_JNI_SYNTAX) && JNIVM_FAKE_JNI_SYNTAX == 1
    std::cout << GeneratePreDeclaration() << "\n"
              << GenerateHeader()
              << GenerateStubs()
              << GenerateJNIBinding()
              << "\n"
              << "void InitJNIBinding(FakeJni::Jvm* vm) {\n"
              << GenerateJNIPreDeclaration()
              << "\n}"
              << "\n";
#else
    std::cout << "Build jnivm with `cmake -DJNIVM_ENABLE_DEBUG=ON -DJNIVM_USE_FAKE_JNI_CODEGEN=ON` to enable this feature, it will print a stub implementation of used Namespaces, Classes, Properties and Functions\n";
#endif
}

bool Baron::Jvm::isClassDenied(const char *name) const {
    return denyClasses.find(name) != denyClasses.end();
}

bool Baron::Jvm::isMethodDenied(const char *name, const char *sig, const char *clazz) const {
    auto results = { denyMethods.equal_range(""), denyMethods.equal_range(name) };
    for(auto && res : results) {
        for (auto i = res.first; i != res.second; ++i) {
            if(name && name[0] != '\0' && i->first != "" && i->first != name) {
                continue;
            }
            if(sig && sig[0] != '\0' && i->second.signature != "" && i->second.signature != sig) {
                continue;
            }
            if(!clazz || clazz[0] == '\0' || i->second.classname.empty() || i->second.classname == clazz) {
                return true;
            }
        }
    }
    return false;
}

bool Baron::Jvm::isFieldDenied(const char *name, const char *sig, const char *clazz) const {
    auto results = { denyFields.equal_range(""), denyFields.equal_range(name) };
    for(auto && res : results) {
        for (auto i = res.first; i != res.second; ++i) {
            if(name && name[0] != '\0' && i->first != "" && i->first != name) {
                continue;
            }
            if(sig && sig[0] != '\0' && i->second.signature != "" && i->second.signature != sig) {
                continue;
            }
            if(!clazz || clazz[0] == '\0' || i->second.classname.empty() || i->second.classname == clazz) {
                return true;
            }
        }
    }
    return false;
}

void Baron::Jvm::denyClass(const char *name) {
    denyClasses.insert(name);
}

void Baron::Jvm::denyMethod(const char *name, const char *sig, const char *clazz) {
    denyMethods.insert({name, { clazz, sig }});
}

void Baron::Jvm::denyField(const char *name, const char *sig, const char *clazz) {
    denyFields.insert({name, { clazz, sig }});
}

#include <jnivm/internal/findclass.h>

std::shared_ptr<jnivm::Class> Baron::Jvm::findClass(const char *name) {
    auto ret = FakeJni::Jvm::findClass(name);
    if(ret)
        return ret;
    if(!isClassDenied(name)) {
        return jnivm::InternalFindClass(jnivm::VM::GetEnv().get(), name, false);
    }
    return nullptr;
}