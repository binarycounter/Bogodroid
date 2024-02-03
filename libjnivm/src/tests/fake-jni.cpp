#include <gtest/gtest.h>
#include <fake-jni/fake-jni.h>

using namespace FakeJni;

class ClassWithNatives : public JObject {
public:
    DEFINE_CLASS_NAME("com/sample/ClassWithNatives")

    static JInt intField;
};

JInt ClassWithNatives::intField = 0;

BEGIN_NATIVE_DESCRIPTOR(ClassWithNatives)
{Field<&ClassWithNatives::intField>{}, "intField", JFieldID::PUBLIC | JFieldID::STATIC}
END_NATIVE_DESCRIPTOR

bool called;

void ClassWithNatives_Native_Method(JNIEnv* env, jclass c) {
    called = true;
}

TEST(FakeJni, GetAndCallNativeMethod) {
    Jvm vm;
    vm.registerClass<ClassWithNatives>();
    LocalFrame f;
    auto c = f.getJniEnv().FindClass("com/sample/ClassWithNatives");
    char name[] = "NativeMethod";
    char sig[] = "()V";
    JNINativeMethod m {name, sig, reinterpret_cast<void*>(&ClassWithNatives_Native_Method)};
    f.getJniEnv().RegisterNatives(c, &m, 1);
    called = false;
    auto cobj = vm.findClass("com/sample/ClassWithNatives");
    auto jm = cobj->getMethod(sig, name);
    jm->invoke(f.getJniEnv(), cobj.get());
    ASSERT_TRUE(called);
}

class ClassWithSuperClassNatives : public ClassWithNatives {
public:
    DEFINE_CLASS_NAME("com/sample/ClassWithSuperClassNatives", ClassWithNatives)
    jint intField;
};

BEGIN_NATIVE_DESCRIPTOR(ClassWithSuperClassNatives)
{Constructor<ClassWithSuperClassNatives>{}},
{Field<&ClassWithSuperClassNatives::intField>{}, "intField"}
END_NATIVE_DESCRIPTOR

TEST(FakeJni, GetAndCallSuperClassNativeMethod) {
    Jvm vm;
    vm.registerClass<ClassWithNatives>();
    vm.registerClass<ClassWithSuperClassNatives>();
    LocalFrame f;
    auto c = f.getJniEnv().FindClass("com/sample/ClassWithNatives");
    char name[] = "NativeMethod";
    char sig[] = "()V";
    JNINativeMethod m {name, sig, reinterpret_cast<void*>(&ClassWithNatives_Native_Method)};
    f.getJniEnv().RegisterNatives(c, &m, 1);
    called = false;
    auto cobj = vm.findClass("com/sample/ClassWithSuperClassNatives");
    auto jm = cobj->getMethod(sig, name);
    jm->invoke(f.getJniEnv(), cobj.get());
    ASSERT_TRUE(called);
}

TEST(FakeJni, InheritStaticField) {
    Jvm vm;
    vm.registerClass<ClassWithNatives>();
    vm.registerClass<ClassWithSuperClassNatives>();
    LocalFrame f;
    auto& env = f.getJniEnv();
    auto c = env.FindClass("com/sample/ClassWithSuperClassNatives");
    ASSERT_TRUE(c);
    auto intField = env.GetFieldID(c, "intField", "I");
    ASSERT_TRUE(intField);
    auto staticIntField = env.GetStaticFieldID(c, "intField", "I");
    ASSERT_TRUE(staticIntField);
    ASSERT_NE(intField, staticIntField);
    env.SetStaticIntField(c, staticIntField, 42);
    auto ctr = env.GetMethodID(c, "<init>", "()V");
    auto o = env.NewObject(c, ctr);
    env.SetIntField(o, intField, 43);
    ASSERT_EQ(env.GetStaticIntField(c, staticIntField), 42);
    ASSERT_EQ(env.GetIntField(o, intField), 43);
}

TEST(FakeJni, GetNameReturnsFakeJniOldPrototypeLikeValue) {
    Jvm vm;
    vm.registerClass<ClassWithNatives>();
    vm.registerClass<ClassWithSuperClassNatives>();
    auto c = vm.findClass("com/sample/ClassWithSuperClassNatives");
    ASSERT_EQ(c->getName(), "com/sample/ClassWithSuperClassNatives");
}

extern "C" JNIEXPORT jint JNI_OnLoad_jnistatic(JavaVM*vm, void*reserved) {
    called = true;
    return JNI_VERSION_1_8;
}

extern "C" JNIEXPORT void JNI_OnUnload_jnistatic(JavaVM*vm, void*reserved) {
    called = true;
}

TEST(FakeJni, LoadStaticLibrary) {
    {
        Jvm vm;
        called = false;
        vm.attachLibrary("jnistatic");
        ASSERT_TRUE(called);
        called = false;
    }
    ASSERT_TRUE(called);
}
