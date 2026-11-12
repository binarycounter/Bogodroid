#include <jnivm/env.h>
#include <jnivm/object.h>
#include <jnivm/internal/findclass.h>
#include <jnivm/class.h>
#include <stdexcept>

using namespace jnivm;

jnivm::ENV::ENV(jnivm::VM *vm, const JNINativeInterface &defaultinterface) : vm(vm), ninterface(defaultinterface), env{&ninterface}
#ifdef EnableJNIVMGC
, localframe({{}})
#endif
{
    ninterface.reserved0 = this;
}

std::shared_ptr<Class> ENV::GetClass(const char * name) {
	return InternalFindClass(this, name);
}

void jnivm::ENV::OverrideJNINativeInterface(const JNINativeInterface &ninterface) {
	if(ninterface.reserved0 != nullptr && ninterface.reserved0 != this->ninterface.reserved0) {
		throw std::runtime_error("Updating `ninterface.reserved0` to a different value is forbidden");
	}
	auto reserved0 = this->ninterface.reserved0;
	this->ninterface = ninterface;
	this->ninterface.reserved0 = reserved0;
}

jnivm::VM *jnivm::ENV::GetVM() {
    return vm;
}

JNIEnv *jnivm::ENV::GetJNIEnv() {
    return &env;
}

jnivm::ENV *jnivm::ENV::FromJNIEnv(JNIEnv * env) {
	if(env == nullptr || env->functions->reserved0 == nullptr) throw std::runtime_error("Failed to get reference to jnivm::ENV");
    return static_cast<jnivm::ENV*>(env->functions->reserved0);
}