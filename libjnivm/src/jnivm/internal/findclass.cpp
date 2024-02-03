#include <jnivm/internal/findclass.h>
#include <jnivm/env.h>
#include <jnivm/jnitypes.h>
#include <cstring>
#include "log.h"

std::shared_ptr<jnivm::Class> jnivm::InternalFindClass(ENV *env, const char *name, bool returnZero, bool trace) {
	auto prefix = name;
	auto && nenv = *env;
	auto && vm = nenv.GetVM();
#ifdef JNI_TRACE
	if(trace) {
		LOG("JNIVM", "FindClass %s", name);
	}
#endif
	std::shared_ptr<Class> curc = nullptr;
#ifdef JNI_DEBUG
	if(name[0] != '[') {
		// Generate the Namespace Hirachy to generate stub c++ files
		// Makes it easier to implement classes without writing everthing by hand
		auto end = name + strlen(name);
		auto pos = name;
		std::shared_ptr<Namespace> cur(&vm->np, [](Namespace *) {
			// Skip deleting this member pointer of VM
		});
		while ((pos = std::find(name, end, '/')) != end) {
			std::string sname = std::string(name, pos);
			auto namsp = std::find_if(cur->namespaces.begin(), cur->namespaces.end(),
																[&sname](std::shared_ptr<Namespace> &namesp) {
																	return namesp->name == sname;
																});
			std::shared_ptr<Namespace> next;
			if (namsp != cur->namespaces.end()) {
				next = *namsp;
			} else {
				if(returnZero) return nullptr;
				next = std::make_shared<Namespace>();
				cur->namespaces.push_back(next);
				next->name = std::move(sname);
			}
			cur = next;
			name = pos + 1;
		}
		do {
			pos = std::find(name, end, '$');
			std::string sname = std::string(name, pos);
			std::shared_ptr<Class> next;
			if (curc) {
				auto cl = std::find_if(curc->classes.begin(), curc->classes.end(),
															[&sname](std::shared_ptr<Class> &namesp) {
																return namesp->name == sname;
															});
				if (cl != curc->classes.end()) {
					next = *cl;
				} else {
					if(returnZero) return nullptr;
					next = std::make_shared<Class>();
					curc->classes.push_back(next);
					next->name = std::move(sname);
					next->nativeprefix = std::string(prefix, pos);
					vm->classes[next->nativeprefix] = next;
				}
			} else {
				auto cl = std::find_if(cur->classes.begin(), cur->classes.end(),
															[&sname](std::shared_ptr<Class> &namesp) {
																return namesp->name == sname;
															});
				if (cl != cur->classes.end()) {
					next = *cl;
				} else {
					if(returnZero) return nullptr;
					next = std::make_shared<Class>();
					cur->classes.push_back(next);
					next->name = std::move(sname);
					next->nativeprefix = std::string(prefix, pos);
					vm->classes[next->nativeprefix] = next;
				}
			}
			curc = next;
			name = pos + 1;
		} while (pos != end);
	} else {
#endif
	auto ccl = vm->classes.find(name);
	if (ccl != vm->classes.end()) {
		curc = ccl->second;
	} else {
		if(returnZero) return nullptr;
		curc = std::make_shared<Class>();
		const char * lastslash = strrchr(name, '/');
		curc->name = lastslash != nullptr ? lastslash + 1 : name;
		curc->nativeprefix = name;
		vm->classes[name] = curc;
	}
#ifdef JNI_DEBUG
	}
#endif
	// curc->nativeprefix = std::move(prefix);
	return curc;
}

jclass jnivm::InternalFindClass(JNIEnv *env, const char *name, bool returnZero, bool trace) {
	return JNITypes<std::shared_ptr<Class>>::ToJNIType(ENV::FromJNIEnv(env), InternalFindClass(ENV::FromJNIEnv(env), name, returnZero, trace));
}

void jnivm::Declare(JNIEnv *env, const char *signature) {
	for (const char *cur = signature, *end = cur + strlen(cur); cur != end;
			cur++) {
		if (*cur == 'L') {
			auto cend = std::find(cur, end, ';');
			std::string classpath(cur + 1, cend);
			InternalFindClass(env, classpath.data());
			cur = cend;
		}
	}
}