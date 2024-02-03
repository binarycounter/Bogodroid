#ifdef EXTRA_INCLUDE
#include EXTRA_INCLUDE
#endif
#include <jni.h>

void callSomeJNIStuff(JNIEnv * env) {
    auto cl = env->FindClass("_unsigned/java/is/bad");
    env->CallStaticObjectMethod(cl, env->GetStaticMethodID(cl, "AnotherBadMethod", "([[[[Ljava/lang/String;)Ljava/lang/Class;"), (jobject)nullptr);
    env->GetMethodID(cl, "AnotherMethod", "([[[[Ljava/lang/String;)Ljava/lang/Class;");
    env->GetStaticFieldID(cl, "AnotherBadField", "[[[[[Ljava/lang/String;");
    env->GetFieldID(cl, "AnotherField", "[[[[Ljava/lang/String;");

    // Inner Class
    auto cl2 = env->FindClass("_unsigned/java/is/bad$inner");
    auto mid0 = env->GetMethodID((jclass)env->NewGlobalRef(cl2), "<init>", "(L_unsigned/java/is/bad;)V");
    auto obj = env->NewObject(cl2, mid0, (jobject)nullptr);
    auto mid1 = env->GetMethodID((jclass)env->NewWeakGlobalRef(cl2), "<init>", "()V");
    auto obj2 = env->NewObject((jclass)env->NewWeakGlobalRef(cl2), mid1);
    auto Hboken = env->GetMethodID((jclass)env->NewWeakGlobalRef(cl2), "Hboken", "()L_unsigned/java/is/bad;");
    auto ret = env->CallObjectMethod(obj2, Hboken);
}

#if JNIVM_FAKE_JNI_SYNTAX == 1 && defined(EXTRA_INCLUDE)
#include <fake-jni/fake-jni.h>
using namespace FakeJni;
#else
#include <jnivm.h>
using namespace jnivm;
#endif
#include <iostream>


int main(int argc, const char** argv) {
#if JNIVM_FAKE_JNI_SYNTAX == 1 && defined(EXTRA_INCLUDE)
    Jvm jvm;
    LocalFrame f;
    InitJNIBinding(&jvm);
    callSomeJNIStuff(&f.getJniEnv());
#else
    VM vm;
#ifdef EXTRA_INCLUDE
    InitJNIBinding(vm.GetEnv().get());
#endif
    callSomeJNIStuff(vm.GetJNIEnv());
    const char* path;
    if( argc != 2) {
        std::cout << "no path specified use a.hpp as file\n";
        path = "a.hpp";
    } else {
        path = argv[1];
    }
    vm.GenerateClassDump(path);
#endif
    return 0;
}