#include <jnivm.h>
#include <iostream>

using namespace jnivm;

#include <jnivm.h>
namespace jnivm {
    class SampleClass;
}class jnivm::SampleClass : public jnivm::Object {
public:
    static void Test(jnivm::ENV *, jnivm::Class* cl);
};

void jnivm::SampleClass::Test(jnivm::ENV *, jnivm::Class* cl) {
    
}

void InitJNIBinding(jnivm::ENV* env) {
env->GetClass<jnivm::SampleClass>("SampleClass");
env->GetClass<jnivm::java::lang::Object>("java/lang/Object");
env->GetClass<jnivm::java::lang::Class>("java/lang/Class");
env->GetClass<jnivm::java::lang::String>("java/lang/String");
env->GetClass<jnivm::java::lang::Throwable>("java/lang/Throwable");
env->GetClass<jnivm::java::lang::reflect::Method>("java/lang/reflect/Method");
env->GetClass<jnivm::java::lang::reflect::Field>("java/lang/reflect/Field");
env->GetClass<jnivm::java::nio::ByteBuffer>("java/nio/ByteBuffer");
env->GetClass<jnivm::internal::lang::Weak>("java/lang/ref/WeakReference");
env->GetClass<jnivm::internal::lang::Global>("internal/lang/Global");
{
auto c = env->GetClass("SampleClass");
c->Hook(env, "Test", &jnivm::SampleClass::Test);
}

}

int main(int argc, char** argv) {
    VM vm;
    auto SampleClass = vm.GetEnv()->GetClass("SampleClass");
    SampleClass->Hook(vm.GetEnv().get(), "Test", [](ENV*env, Class* c) {
        std::cout << "Static function of " << c->getName() << " called\n";
    });
    JNIEnv* env = vm.GetJNIEnv();
    jclass sc = env->FindClass("SampleClass");
    jmethodID id = env->GetStaticMethodID(sc, "Test", "()V");
    env->CallStaticVoidMethod(sc, id);
    return 0;
}