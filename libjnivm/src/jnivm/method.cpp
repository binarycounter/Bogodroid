#include <jnivm/class.h>
#include <jnivm/method.h>
#include <stdexcept>

using namespace jnivm;

template<class T> jvalue toJValue(T val) {
	jvalue ret;
	static_assert(sizeof(T) <= sizeof(jvalue), "jvalue cannot hold the specified type!");
	memset(&ret, 0, sizeof(ret));
	memcpy(&ret, &val, sizeof(val));
	return ret;
}

jvalue Method::jinvoke(jnivm::ENV &env, jclass cl, jvalue* l) {
	if(signature.empty()) {
		throw std::runtime_error("jni signature is empty");
	}
	jvalue ret;
	auto type = signature[signature.find_last_of(')') + 1];
	switch (type) {
	case 'V':
		env.GetJNIEnv()->functions->CallStaticVoidMethodA(env.GetJNIEnv(), cl, (jmethodID)this, l);
		ret = {};
		break;
	case 'Z':
		ret = toJValue(env.GetJNIEnv()->functions->CallStaticBooleanMethodA(env.GetJNIEnv(), cl, (jmethodID)this, l));
		break;
	case 'B':
		ret = toJValue(env.GetJNIEnv()->functions->CallStaticByteMethodA(env.GetJNIEnv(), cl, (jmethodID)this, l));
		break;
	case 'S':
		ret = toJValue(env.GetJNIEnv()->functions->CallStaticShortMethodA(env.GetJNIEnv(), cl, (jmethodID)this, l));
		break;
    case 'C':
		ret = toJValue(env.GetJNIEnv()->functions->CallStaticCharMethodA(env.GetJNIEnv(), cl, (jmethodID)this, l));
		break;
	case 'I':
		ret = toJValue(env.GetJNIEnv()->functions->CallStaticIntMethodA(env.GetJNIEnv(), cl, (jmethodID)this, l));
		break;
	case 'J':
		ret = toJValue(env.GetJNIEnv()->functions->CallStaticLongMethodA(env.GetJNIEnv(), cl, (jmethodID)this, l));
		break;
	case 'F':
		ret = toJValue(env.GetJNIEnv()->functions->CallStaticFloatMethodA(env.GetJNIEnv(), cl, (jmethodID)this, l));
		break;
	case 'D':
		ret = toJValue(env.GetJNIEnv()->functions->CallStaticDoubleMethodA(env.GetJNIEnv(), cl, (jmethodID)this, l));
		break;
	case '[':
	case 'L':
		ret = toJValue(env.GetJNIEnv()->functions->CallStaticObjectMethodA(env.GetJNIEnv(), cl, (jmethodID)this, l));
		break;
	default:
		throw std::runtime_error("Unsupported signature");
	}
	return ret;
}

jvalue Method::jinvoke(jnivm::ENV &env, jobject obj, jvalue* l) {
	if(signature.empty()) {
		throw std::runtime_error("jni signature is empty");
	}
	jvalue ret;
	auto type = signature[signature.find_last_of(')') + 1];
	switch (type) {
	case 'V':
		env.GetJNIEnv()->functions->CallVoidMethodA(env.GetJNIEnv(), obj, (jmethodID)this, l);
		ret = {};
		break;
	case 'Z':
		ret = toJValue(env.GetJNIEnv()->functions->CallBooleanMethodA(env.GetJNIEnv(), obj, (jmethodID)this, l));
		break;
	case 'B':
		ret = toJValue(env.GetJNIEnv()->functions->CallByteMethodA(env.GetJNIEnv(), obj, (jmethodID)this, l));
		break;
	case 'S':
		ret = toJValue(env.GetJNIEnv()->functions->CallShortMethodA(env.GetJNIEnv(), obj, (jmethodID)this, l));
		break;
	case 'C':
		ret = toJValue(env.GetJNIEnv()->functions->CallCharMethodA(env.GetJNIEnv(), obj, (jmethodID)this, l));
		break;
	case 'I':
		ret = toJValue(env.GetJNIEnv()->functions->CallIntMethodA(env.GetJNIEnv(), obj, (jmethodID)this, l));
		break;
	case 'J':
		ret = toJValue(env.GetJNIEnv()->functions->CallLongMethodA(env.GetJNIEnv(), obj, (jmethodID)this, l));
		break;
	case 'F':
		ret = toJValue(env.GetJNIEnv()->functions->CallFloatMethodA(env.GetJNIEnv(), obj, (jmethodID)this, l));
		break;
	case 'D':
		ret = toJValue(env.GetJNIEnv()->functions->CallDoubleMethodA(env.GetJNIEnv(), obj, (jmethodID)this, l));
		break;
	case '[':
	case 'L':
		ret = toJValue(env.GetJNIEnv()->functions->CallObjectMethodA(env.GetJNIEnv(), obj, (jmethodID)this, l));
		break;
	default:
		throw std::runtime_error("Unsupported signature");
	}
	return ret;
}

#define DeclareTemplate(T) template jvalue toJValue(T val);
DeclareTemplate(jboolean);
DeclareTemplate(jbyte);
DeclareTemplate(jshort);
DeclareTemplate(jint);
DeclareTemplate(jlong);
DeclareTemplate(jfloat);
DeclareTemplate(jdouble);
DeclareTemplate(jchar);
DeclareTemplate(jobject);
#undef DeclareTemplate