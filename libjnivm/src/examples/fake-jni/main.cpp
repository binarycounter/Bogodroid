#include <fake-jni/fake-jni.h>
#include <iostream>

using namespace FakeJni;

class SampleClass : public JObject {
public:
    DEFINE_CLASS_NAME("com/example/SampleClass")

    SampleClass() {
        booleanfield = true;
    }

    JBoolean booleanfield;
    JByte bytefield;
    JShort shortfield;
    JInt intfield;
    JLong longfield;
    JFloat floatfield;
    JDouble doublefield;

    static JBoolean staticbooleanfield;

    std::shared_ptr<JByteArray> bytearrayfield;
    std::shared_ptr<JShortArray> shortarrayfield;
    std::shared_ptr<JIntArray> intarrayfield;
    std::shared_ptr<JLongArray> longarrayfield;
    std::shared_ptr<JFloatArray> floatarrayfield;
    std::shared_ptr<JDoubleArray> doublearrayfield;

    JDouble JustAMemberFunction(std::shared_ptr<JIntArray> array) {
        for (jsize i = 0; i < array->getSize(); i++) {
            std::cout << "Value of (*array)[" << i << "] = " << (*array)[i] << "\n"; 
        }
        return 3.6;
    }

    inline static void exampleStaticFunction(JDouble d) {
        std::cout << "From ExampleClass: " << d << std::endl;
    }
};

class DerivedClass : public SampleClass {
public:
    DEFINE_CLASS_NAME("com/example/DerivedClass", SampleClass)


};

FakeJni::JBoolean SampleClass::staticbooleanfield = false;

BEGIN_NATIVE_DESCRIPTOR(SampleClass)
// some fields as member fields, not static
{ Field<&SampleClass::booleanfield>{}, "booleanfield" },
{ Field<&SampleClass::bytefield>{}, "bytefield" },
{ Field<&SampleClass::shortfield>{}, "shortfield" },
{ Field<&SampleClass::intfield>{}, "intfield" },
{ Field<&SampleClass::longfield>{}, "longfield" },
{ Field<&SampleClass::floatfield>{}, "floatfield" },
{ Field<&SampleClass::doublefield>{}, "doublefield" },
{ Field<&SampleClass::bytearrayfield>{}, "bytearrayfield" },
{ Field<&SampleClass::shortarrayfield>{}, "shortarrayfield" },
{ Field<&SampleClass::intarrayfield>{}, "intarrayfield" },
{ Field<&SampleClass::longarrayfield>{}, "longarrayfield" },
// staticbooleanfield explicitly as static field
{ Field<&staticbooleanfield>{}, "staticbooleanfield", JFieldID::PUBLIC | JFieldID::STATIC },
// staticbooleanfield as member field
{ Field<&staticbooleanfield>{}, "staticbooleanfield2" },
// staticbooleanfield explicitly as member field
{ Field<&staticbooleanfield>{}, "booleanfield2", JFieldID::PUBLIC },
{ Function<&SampleClass::JustAMemberFunction>{}, "JustAMemberFunction" },
// exampleStaticFunction as member function, not static, this is different to fields
{ Function<&exampleStaticFunction>{}, "exampleStaticMemberFunction" },
// exampleStaticFunction explicitly as member function, not static
{ Function<&exampleStaticFunction>{}, "exampleStaticMemberFunction2", JMethodID::PUBLIC },
// exampleStaticFunction explicitly as static function
{ Function<&exampleStaticFunction>{}, "exampleStaticFunction", JMethodID::PUBLIC | JMethodID::STATIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(DerivedClass)
END_NATIVE_DESCRIPTOR

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    return JNI_VERSION_1_6;
}

int main(int argc, char** argv) {
    FakeJni::Jvm jvm;
    jvm.registerClass<SampleClass>();
    jvm.registerClass<DerivedClass>();
    jvm.attachLibrary("");
    LocalFrame frame(jvm);
    jobject ref = frame.getJniEnv().createLocalReference(std::make_shared<SampleClass>());
    jfieldID fieldid = frame.getJniEnv().GetFieldID(frame.getJniEnv().GetObjectClass(ref), "booleanfield", "Z");
    jboolean value = frame.getJniEnv().GetBooleanField(ref, fieldid);
    std::cout << "booleanfield has value " << (bool)value << "\n";
    std::shared_ptr<SampleClass> refAsObj = std::dynamic_pointer_cast<SampleClass>(frame.getJniEnv().resolveReference(ref));
    refAsObj->booleanfield = false;
    value = frame.getJniEnv().GetBooleanField(ref, fieldid);
    std::cout << "booleanfield has changed it's value to " << (bool)value << "\n";

    jmethodID method = frame.getJniEnv().GetMethodID(frame.getJniEnv().GetObjectClass(ref), "JustAMemberFunction", "([I)D");
    auto a = frame.getJniEnv().NewIntArray(23);
    jint* carray = frame.getJniEnv().GetIntArrayElements(a, nullptr);
    for (jsize i = 0; i < frame.getJniEnv().GetArrayLength(a); i++) {
        carray[i] = 1 << i;
    }
    frame.getJniEnv().ReleaseIntArrayElements(a, carray, 0);
    jdouble ret = frame.getJniEnv().CallDoubleMethod(ref, method, a);
    std::cout << "JustAMemberFunction returned " << ret << "\n";

    jmethodID exampleStaticMemberFunction = frame.getJniEnv().GetMethodID(frame.getJniEnv().GetObjectClass(ref), "exampleStaticMemberFunction", "(D)V");
    frame.getJniEnv().CallVoidMethod(ref, exampleStaticMemberFunction, 7.9);

    jmethodID exampleStaticFunction = frame.getJniEnv().GetMethodID(frame.getJniEnv().GetObjectClass(ref), "exampleStaticMemberFunction", "(D)V");
    frame.getJniEnv().CallVoidMethod(frame.getJniEnv().GetObjectClass(ref), exampleStaticFunction, 3.8);

    jfieldID fieldid2 = frame.getJniEnv().GetFieldID(frame.getJniEnv().GetObjectClass(ref), "booleanfield2", "Z");
    value = frame.getJniEnv().GetBooleanField(ref, fieldid2);
    std::cout << "booleanfield2 has value " << (bool)value << "\n";

    SampleClass::staticbooleanfield = true;
    value = frame.getJniEnv().GetBooleanField(ref, fieldid2);
    std::cout << "booleanfield2 has changed it's value to " << (bool)value << "\n";

    jfieldID staticbooleanfield = frame.getJniEnv().GetStaticFieldID(frame.getJniEnv().GetObjectClass(ref), "staticbooleanfield", "Z");
    value = frame.getJniEnv().GetStaticBooleanField(frame.getJniEnv().GetObjectClass(ref), staticbooleanfield);
    std::cout << "staticbooleanfield has value " << (bool)value << "\n";

    SampleClass::staticbooleanfield = false;
    value = frame.getJniEnv().GetStaticBooleanField(frame.getJniEnv().GetObjectClass(ref), staticbooleanfield);
    std::cout << "staticbooleanfield has changed it's value to " << (bool)value << "\n";

    auto CDerivedClass = jvm.findClass("com/example/DerivedClass");
    // ! This line may not work with the original fake-jni, see here https://github.com/dukeify/fake-jni/issues/101
    CDerivedClass->getMethod("(D)V", "exampleStaticFunction")->invoke(frame.getJniEnv(), CDerivedClass.get(), 3.4);

    frame.getJniEnv().GetStaticFieldID(frame.getJniEnv().GetObjectClass(ref), "staticstringfield", "Ljava/lang/String;");

    auto args = std::make_shared<JArray<JString>>(1);
	(*args)[0] = std::make_shared<JString>("main");

    {
        jfieldID fieldid = frame.getJniEnv().GetFieldID(frame.getJniEnv().GetObjectClass(ref), "longarrayfield", "[J");
        jobject value = frame.getJniEnv().GetObjectField(ref, fieldid);
        frame.getJniEnv().SetObjectField(ref, fieldid, frame.getJniEnv().NewLongArray(100));

        jobject value2 = frame.getJniEnv().GetObjectField(ref, fieldid);
        frame.getJniEnv().GetArrayLength((jarray)value2);
    }
    return 0;
}