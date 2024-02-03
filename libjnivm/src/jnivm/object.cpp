#include <jnivm.h>
#include <jnivm/object.h>
#include <jnivm/weak.h>

std::shared_ptr<jnivm::Class> jnivm::Object::getClassInternal(jnivm::ENV *env) {
    return clazz.lock();
}

std::vector<std::shared_ptr<jnivm::Class>> jnivm::Object::GetBaseClasses(jnivm::ENV *env) {
	return { nullptr };
}