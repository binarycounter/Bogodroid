#include <fake-jni/fake-jni.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <stdexcept>
#include <iostream>

FakeJni::JniEnvContext::JniEnvContext(FakeJni::Jvm &vm) {
	env2 = env.env.lock();
    if (!env2) {
		this->vm = &vm;
        vm.AttachCurrentThread(nullptr, nullptr);
		env2 = env.env.lock();
    } else {
		this->vm = nullptr;
	}
}

FakeJni::JniEnvContext::JniEnvContext() {
	env2 = env.env.lock();
	this->vm = nullptr;
}

FakeJni::JniEnvContext::~JniEnvContext() {
	if(this->vm) {
		vm->DetachCurrentThread();
	}
}

FakeJni::ThreadContext::~ThreadContext() {
	auto _env = env.lock();
	if(_env) {
		_env->getVM().DetachCurrentThread();
		_env = nullptr;
		if(env.lock()) {
			abort();
		}
	}
}

thread_local FakeJni::ThreadContext FakeJni::JniEnvContext::env = {};

FakeJni::Env &FakeJni::JniEnvContext::getJniEnv() {
    if (env2 == nullptr) {
        throw std::runtime_error("No Env in this thread");
    }
    return *env2;
}

std::shared_ptr<jnivm::Class> FakeJni::Jvm::findClass(const char *name) {
    return jnivm::InternalFindClass(jnivm::VM::GetEnv().get(), name, true);
}

jobject FakeJni::Jvm::createGlobalReference(std::shared_ptr<jnivm::Object> obj) {
    return jnivm::VM::GetEnv()->GetJNIEnv()->NewGlobalRef((jobject)obj.get());
}

#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif

FakeJni::Jvm::libinst::libinst(const std::string &rpath, JavaVM *javaVM, FakeJni::LibraryOptions loptions) : loptions(loptions), javaVM(javaVM) {
	std::cout << rpath << " Loading" << std::endl;
	handle = loptions.dlopen(rpath.c_str(), RTLD_LAZY);
	if(handle) {
		std::cout << rpath << " got handle" << std::endl;
		auto JNI_OnLoad = (jint (*)(JavaVM* vm, void* reserved))loptions.dlsym(handle, "JNI_OnLoad");
		if (JNI_OnLoad) {
			std::cout << rpath << " got JNI_OnLoad" << std::endl;
			JNI_OnLoad(javaVM, nullptr);
		}
	} else if(!rpath.empty() && rpath.find("/") == std::string::npos && rpath.find(".") == std::string::npos) {
		handle = loptions.dlopen(nullptr, RTLD_LAZY);
		if(handle) {
			std::cout << rpath << " got handle2" << std::endl;
			path = rpath;
			auto JNI_OnLoad = (jint (*)(JavaVM* vm, void* reserved))loptions.dlsym(handle, ("JNI_OnLoad_" + rpath).data());
			if (JNI_OnLoad) {
				std::cout << rpath << " got JNI_OnLoad" << std::endl;
				JNI_OnLoad(javaVM, nullptr);
			}
		}
	}
}

FakeJni::Jvm::libinst::~libinst() {
	if(handle) {
		auto JNI_OnUnload = (void (*)(JavaVM* vm, void* reserved))loptions.dlsym(handle, path.empty() ? "JNI_OnUnload" : ("JNI_OnUnload_" + path).data() );
		if (JNI_OnUnload) {
			JNI_OnUnload(javaVM, nullptr);
		}
		loptions.dlclose(handle);
	}
}

void FakeJni::Jvm::attachLibrary(const std::string &rpath, const std::string &options, FakeJni::LibraryOptions loptions) {
	libraries.insert({ rpath, std::make_unique<libinst>(rpath, this, loptions) });
}

void FakeJni::Jvm::removeLibrary(const std::string &rpath, const std::string &options) {
	libraries.erase(rpath);
}

std::shared_ptr<FakeJni::JObject> FakeJni::Env::resolveReference(jobject obj) {
    return jnivm::JNITypes<std::shared_ptr<JObject>>::JNICast(this, obj);
}

FakeJni::Jvm &FakeJni::Env::getVM() {
    return jvm;
}

void FakeJni::Jvm::start() {
	auto args = std::make_shared<JArray<JString>>(1);
	(*args)[0] = std::make_shared<JString>("main");
	start(args);
}

void FakeJni::Jvm::start(std::shared_ptr<FakeJni::JArray<FakeJni::JString>> args) {
	for(auto&& c : classes) {
		LocalFrame frame(*this);
		auto main = c.second->getMethod("([Ljava/lang/String;)V", "main");
		if(main != nullptr) {
			main->invoke(frame.getJniEnv(), c.second.get(), args);
			return;
		}
	}
	throw std::runtime_error("main with [Ljava/lang/String;)V not found in any class!");
}

FakeJni::LibraryOptions::LibraryOptions(void *(*dlopen)(const char *, int), void *(*dlsym)(void *handle, const char *), int (*dlclose)(void *)) : dlopen(dlopen), dlsym(dlsym), dlclose(dlclose) {
}
#ifdef _WIN32
struct LibraryOptions_Wrapper {
	HMODULE mod;
	bool free;
};
#endif

FakeJni::LibraryOptions::LibraryOptions() : LibraryOptions(
#ifdef _WIN32
[](const char * name, int) -> void* {
	static_assert(sizeof(HMODULE) <= sizeof(void*), "The windows handle is too large to fit in void*");
	if(!name || name[0] == '\0') {
		void(*h)() = +[]() {};
		HMODULE mod;
		if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR) h, &mod)) {
			return new LibraryOptions_Wrapper { mod , false };
		} else {
			return nullptr;
		}
	}
	int size = MultiByteToWideChar(CP_UTF8, 0, name, -1, NULL, 0);
	std::vector<wchar_t> wd(size + 1);
	(void)MultiByteToWideChar(CP_UTF8, 0, name, -1, wd.data(), size + 1);
	wd[size] = L'\0';
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) && !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
	auto ret = LoadPackagedLibrary(wd.data(), 0);
#else
	auto ret = LoadLibraryW(wd.data());
#endif
	if(!ret) {
		return nullptr;
	}
	return new LibraryOptions_Wrapper { ret , true };
}, [](void * handle, const char * sym) -> void* {
	return handle && sym ? (void*)GetProcAddress(((LibraryOptions_Wrapper*)handle)->mod, sym) : nullptr;
}, [](void * handle) -> int {
	int ret = 0;
	if(!handle) return ret;
	if(((LibraryOptions_Wrapper*)handle)->free) {
		ret = CloseHandle(((LibraryOptions_Wrapper*)handle)->mod);
	}
	delete (LibraryOptions_Wrapper*)handle;
	return ret;
}
#else
::dlopen, ::dlsym, ::dlclose
#endif
) {}

jnivm::Class &jnivm::Object::getClass() {
    auto&& env = std::addressof(FakeJni::JniEnvContext().getJniEnv());
    auto ret = getClassInternal(ENV::FromJNIEnv(env));
    if(ret == nullptr) {
        throw std::runtime_error("Invalid Object");
    }
    return *ret.get();
}

void FakeJni::createMainMethod(FakeJni::Jvm &jvm, std::function<void (std::shared_ptr<FakeJni::JArray<FakeJni::JString>> args)>&& callback) {
    std::shared_ptr<JClass> createMainMethod = jvm.findClass("fakejni/impl/createMainMethod");
    createMainMethod->Hook(jnivm::VM::FromJavaVM(&jvm)->GetEnv().get(), "main", std::move(callback));
}

void FakeJni::createMainMethod(FakeJni::Jvm &jvm, std::function<void (FakeJni::JArray<FakeJni::JString>* args)>&& callback) {
    std::shared_ptr<JClass> createMainMethod = jvm.findClass("fakejni/impl/createMainMethod");
    createMainMethod->Hook(jnivm::VM::FromJavaVM(&jvm)->GetEnv().get(), "main", [callback](std::shared_ptr<FakeJni::JArray<FakeJni::JString>> args) {
        callback(args.get());
    });
}