#include <jnivm.h>
#ifdef _WIN32
#define pthread_self() GetCurrentThreadId()
#endif
#include <jnivm/internal/skipJNIType.h>
#include <jnivm/internal/findclass.h>
#include <jnivm/internal/jValuesfromValist.h>
#include <jnivm/internal/codegen/namespace.h>
#include <locale>
#include <sstream>
#include <climits>
#include "internal/log.h"
#include <jnivm/weak.h>
#include <stdexcept>

using namespace jnivm;

jint GetVersion(JNIEnv *) {
	return JNI_VERSION_1_6;
};
jclass DefineClass(JNIEnv *, const char *, jobject, const jbyte *, jsize) {
#ifdef JNI_DEBUG
	LOG("JNIVM", "DefineClass unsupported");
#endif
	return 0;
};

template<bool returnZero=false>
jclass FindClass(JNIEnv *env, const char *name) {
	auto&& nenv = *ENV::FromJNIEnv(env);
	std::lock_guard<std::mutex> lock(nenv.GetVM()->mtx);
	return InternalFindClass(env, name, returnZero, true);
};
jmethodID FromReflectedMethod(JNIEnv *env, jobject obj) {
	if(obj && env->functions->IsSameObject(env, env->functions->GetObjectClass(env, obj), FindClass(env, "java/lang/reflect/Method"))) {
		return (jmethodID) obj;
	} else {
		return nullptr;
	}
};
jfieldID FromReflectedField(JNIEnv *env, jobject obj) {
	if(obj && env->functions->IsSameObject(env, env->functions->GetObjectClass(env, obj), FindClass(env, "java/lang/reflect/Field"))) {
		return (jfieldID) obj;
	} else {
		return nullptr;
	}
};
jobject ToReflectedMethod(JNIEnv * env, jclass c, jmethodID mid, jboolean isStatic) {
	auto method = (Method*)mid;
	if(c && method && method->_static == (bool)isStatic) {
		return env->NewLocalRef((jobject) method);
	}
	return 0;
};
jclass GetSuperclass(JNIEnv * env, jclass c) {
	auto&& cl = JNITypes<std::shared_ptr<jnivm::Class>>::JNICast(ENV::FromJNIEnv(env), c);
	return cl->baseclasses ? (jclass)(cl->baseclasses(ENV::FromJNIEnv(env))[0]).get() : nullptr;
};
bool HasBaseClass(JNIEnv *env, Class* cl, Class* c2) {
	if(cl == c2) {
		return true;
	}
	if(cl->baseclasses) {
		for(auto&& i : cl->baseclasses(ENV::FromJNIEnv(env))) {
			if(i && HasBaseClass(env, i.get(), c2)) {
				return true;
			}
		}
	}
	return false;
}
jboolean IsAssignableFrom(JNIEnv *env, jclass c1, jclass c2) {
	return HasBaseClass(env, JNITypes<std::shared_ptr<jnivm::Class>>::JNICast(ENV::FromJNIEnv(env), c1).get(), JNITypes<std::shared_ptr<jnivm::Class>>::JNICast(ENV::FromJNIEnv(env), c2).get());
};
jobject ToReflectedField(JNIEnv * env, jclass c, jfieldID fid, jboolean isStatic) {
	auto field = (Field*)fid;
	if(c && field && field->_static == (bool)isStatic) {
		return env->NewLocalRef((jobject) field);
	}
	return 0;
};
jint Throw(JNIEnv *env, jthrowable ex) {
	auto except = JNITypes<std::shared_ptr<jnivm::Throwable>>::JNICast(ENV::FromJNIEnv(env), ex);
	(ENV::FromJNIEnv(env))->current_exception = except ? except : nullptr;
	return 0;
};
jint ThrowNew(JNIEnv *env, jclass c, const char * message) {
	try {
		throw std::runtime_error(message);
	} catch(...) {
		auto th = std::make_shared<Throwable>();
		th->except = std::current_exception();
		(ENV::FromJNIEnv(env))->current_exception = th;
	}
	return 0;
};
jthrowable ExceptionOccurred(JNIEnv * env) {
	return JNITypes<std::shared_ptr<Throwable>>::ToJNIType(ENV::FromJNIEnv(env), (ENV::FromJNIEnv(env))->current_exception) ;
};
void ExceptionDescribe(JNIEnv *env) {
	if((ENV::FromJNIEnv(env))->current_exception) {
		try {
			std::rethrow_exception((ENV::FromJNIEnv(env))->current_exception->except);
		} catch (const std::exception& ex) {
			LOG("JNIVM", "Exception with Message `%s` was thrown", ex.what());
		} catch (...) {
		}
	} else {
		LOG("JNIVM", "No pending Exception");
	}
};
void ExceptionClear(JNIEnv *env) {
	(ENV::FromJNIEnv(env))->current_exception = nullptr;
};
void FatalError(JNIEnv *, const char * err) {
	LOG("JNIVM", "FatalError called: %s", err);
	abort();
};
jint PushLocalFrame(JNIEnv * env, jint cap) {
	return 0;
#ifdef EnableJNIVMGC
	auto&& nenv = *ENV::FromJNIEnv(env);
	// Add it to the list
	if(nenv.freeframes.empty()) {
		nenv.localframe.emplace_front();
	} else {
		nenv.localframe.splice_after(nenv.localframe.before_begin(), nenv.freeframes, nenv.freeframes.before_begin());
	}
	// Reserve Memory for the new Frame
	if(cap)
		nenv.localframe.front().reserve(cap);
#endif
	return 0;
};
jobject PopLocalFrame(JNIEnv * env, jobject result) {
	return result;
#ifdef EnableJNIVMGC
	auto&& nenv = *ENV::FromJNIEnv(env);
	// save reference on stack
	auto res = JNITypes<std::shared_ptr<Object>>::JNICast(&nenv, result);
	// Clear current Frame and move to freelist
	nenv.localframe.front().clear();
	nenv.freeframes.splice_after(nenv.freeframes.before_begin(), nenv.localframe, nenv.localframe.before_begin());
	if(nenv.localframe.empty()) {
		LOG("JNIVM", "Freed top level frame of this ENV, recreate it");
		nenv.localframe.emplace_front();
	}
	// Add result to previous frame
	if(res) {
		return JNITypes<std::shared_ptr<Object>>::ToJNIType(&nenv, res);
	}
#endif
	return result;
};
jobject NewGlobalRef(JNIEnv * env, std::shared_ptr<Object> obj) {
#ifdef EnableJNIVMGC
	if(!obj) return nullptr;
	auto&& nenv = *ENV::FromJNIEnv(env);
	auto&& nvm = *nenv.GetVM();
	std::lock_guard<std::mutex> guard{nvm.mtx};
	nvm.globals.emplace_back(obj);
#endif
	return (jobject)obj.get();
};
jobject NewGlobalRef(JNIEnv * env, jobject obj) {
	auto strong = JNITypes<std::shared_ptr<Object>>::JNICast(ENV::FromJNIEnv(env), obj);
	if(!strong) {
		return (jobject)nullptr;
	}
	auto global = std::make_shared<Global>();
	global->wrapped = std::move(strong);
	global->clazz = InternalFindClass(ENV::FromJNIEnv(env), "internal/lang/Global");
	return NewGlobalRef(env, global);
};
void DeleteGlobalRef(JNIEnv * env, std::shared_ptr<Object> obj) {
#ifdef EnableJNIVMGC
	if(!obj) return;
	auto&& nenv = *ENV::FromJNIEnv(env);
	auto&& nvm = *nenv.GetVM();
	std::lock_guard<std::mutex> guard{nvm.mtx};
	auto fe = nvm.globals.end();
	auto f = std::find(nvm.globals.begin(), fe, obj);
	if(f != fe) {
		nvm.globals.erase(f);
	} else {
		LOG("JNIVM", "Failed to delete Global Reference");
	}
#endif
};
void DeleteGlobalRef(JNIEnv * env, jobject obj) {
	DeleteGlobalRef(env, JNITypes<std::shared_ptr<Global>>::JNICast(ENV::FromJNIEnv(env), obj));
};
void DeleteLocalRef(JNIEnv * env, std::shared_ptr<Object> obj) {
#ifdef EnableJNIVMGC
	if(!obj) return;
	auto&& nenv = *ENV::FromJNIEnv(env);
	for(auto && frame : nenv.localframe) {
		auto fe = frame.end();
		auto f = std::find(frame.begin(), fe, obj);
		if(f != fe) {
			frame.erase(f);
			return;
		}
	}
	LOG("JNIVM", "Failed to delete Local Reference");
#endif
};
void DeleteLocalRef(JNIEnv * env, jobject obj) {
	DeleteLocalRef(env, JNITypes<std::shared_ptr<Object>>::JNICast(ENV::FromJNIEnv(env), obj));
};
jboolean IsSameObject(JNIEnv *env, jobject lobj, jobject robj) {
	if(lobj == robj) {
		return true;
	}
	auto l = JNITypes<std::shared_ptr<Object>>::JNICast(ENV::FromJNIEnv(env), lobj);
	auto r = JNITypes<std::shared_ptr<Object>>::JNICast(ENV::FromJNIEnv(env), robj);
	return l.get() == r.get();
};
jobject NewLocalRef(JNIEnv * env, std::shared_ptr<Object> obj) {
#ifdef EnableJNIVMGC
	if(!obj) return nullptr;
	auto&& nenv = *ENV::FromJNIEnv(env);
	// Get the current localframe and create a ref
	nenv.localframe.front().emplace_back(obj);
#endif
	return (jobject)obj.get();
};
jobject NewLocalRef(JNIEnv * env, jobject obj) {
	return NewLocalRef(env, JNITypes<std::shared_ptr<Object>>::JNICast(ENV::FromJNIEnv(env), obj));
}
jint EnsureLocalCapacity(JNIEnv * env, jint cap) {
#ifdef EnableJNIVMGC
	auto&& nenv = *ENV::FromJNIEnv(env);
	nenv.localframe.front().reserve(cap);
#endif
	return 0;
};
jobject AllocObject(JNIEnv *env, jclass cl) {
	LOG("JNIVM", "Not Implemented Method AllocObject called");
	return nullptr;
};

jclass GetObjectClass(JNIEnv *env, jobject jo) {
	return jo ? JNITypes<std::shared_ptr<jnivm::Class>>::ToJNIType(ENV::FromJNIEnv(env), JNITypes<std::shared_ptr<jnivm::Object>>::JNICast(ENV::FromJNIEnv(env), jo)->getClassInternal(ENV::FromJNIEnv(env))) : env->FindClass("Invalid");
};
jboolean IsInstanceOf(JNIEnv *env, jobject jo, jclass cl) {
	printf("[JNIVM] Is %s an instance of %s? - %d\n",JNITypes<std::shared_ptr<jnivm::Object>>::JNICast(ENV::FromJNIEnv(env), jo).get()->getClass().getName().c_str(),JNITypes<std::shared_ptr<jnivm::Class>>::JNICast(ENV::FromJNIEnv(env), cl).get()->getClass().getName().c_str(),  jo && IsAssignableFrom(env, GetObjectClass(env, jo), cl));
	return jo && IsAssignableFrom(env, GetObjectClass(env, jo), cl);
};

#include "internal/method.h"

#include "internal/field.hpp"

#include "internal/string.hpp"

#include "internal/array.hpp"

jint RegisterNatives(JNIEnv *env, jclass c, const JNINativeMethod *method, jint i) {
	auto&& clazz = JNITypes<std::shared_ptr<jnivm::Class>>::JNICast(ENV::FromJNIEnv(env), c);
	if(!clazz) {
		LOG("JNIVM", "RegisterNatives failed, class is nullptr");
	} else {
		std::lock_guard<std::mutex> lock(clazz->mtx);
		while(i--) {
			clazz->natives[method->name] = method->fnPtr;
#ifdef JNI_TRACE
			std::stringstream ss;
			ss << "Class \"" << clazz->nativeprefix << "\" registred native method: " << method->name << " signature: " << method->signature;
			LOG("JNIVM", "%s", ss.str().data());
#endif
			auto m = std::make_shared<Method>();
			m->name = method->name;
			m->signature = method->signature;
			m->native = method->fnPtr;
			clazz->methods.push_back(m);
			method++;
		}
	}
	return 0;
};
jint UnregisterNatives(JNIEnv *env, jclass c) {
	auto&& clazz = JNITypes<std::shared_ptr<jnivm::Class>>::JNICast(ENV::FromJNIEnv(env), c);
	if(!clazz) {
		LOG("JNIVM", "UnRegisterNatives failed, class is nullptr");
	} else {
		std::lock_guard<std::mutex> lock(clazz->mtx);
		clazz->natives.clear();
		for(size_t i = 0; i < clazz->methods.size(); ++i) {
			if(clazz->methods[i]->native) {
				clazz->methods.erase(clazz->methods.begin() + i);
				--i;
			}
		}
	}
	return 0;
};

jint MonitorEnter(JNIEnv *env, jobject o) {
	JNITypes<std::shared_ptr<jnivm::Object>>::JNICast(ENV::FromJNIEnv(env), o)->lock.lock.lock();
	return 0;
};
jint MonitorExit(JNIEnv *env, jobject o) {
	JNITypes<std::shared_ptr<jnivm::Object>>::JNICast(ENV::FromJNIEnv(env), o)->lock.lock.unlock();
	return 0;
}

jint GetJavaVM(JNIEnv * env, JavaVM ** vm) {
	if(vm) {
		std::lock_guard<std::mutex> lock((ENV::FromJNIEnv(env))->GetVM()->mtx);
		*vm = (ENV::FromJNIEnv(env))->GetVM()->GetJavaVM();
	}
	return 0;
}

jweak NewWeakGlobalRef(JNIEnv *env, jobject obj) {
	auto strong = JNITypes<std::shared_ptr<Object>>::JNICast(ENV::FromJNIEnv(env),obj);
	if(!strong) {
		return (jweak)nullptr;
	}
	auto weak = std::make_shared<Weak>();
	weak->wrapped = strong;
	weak->clazz = InternalFindClass(ENV::FromJNIEnv(env), "java/lang/ref/WeakReference");

	return (jweak) NewGlobalRef(env, weak);
}
void DeleteWeakGlobalRef(JNIEnv *env, jweak w) {
	DeleteGlobalRef(env, JNITypes<std::shared_ptr<Weak>>::JNICast(ENV::FromJNIEnv(env), w));
}

jboolean ExceptionCheck(JNIEnv *env) {
	return (bool)(ENV::FromJNIEnv(env))->current_exception;
}
#include "internal/bytebuffer.hpp"

jobjectRefType GetObjectRefType(JNIEnv *env, jobject obj) {
	if(!obj) {
		return jobjectRefType::JNIInvalidRefType;
	}
	auto o = reinterpret_cast<Object*>(obj);
	if(dynamic_cast<Weak*>(o)) {
		return jobjectRefType::JNIWeakGlobalRefType;
	} else if(dynamic_cast<Global*>(o)){
		return jobjectRefType::JNIGlobalRefType;
	}
	return jobjectRefType::JNILocalRefType;
};

template<class ...jnitypes> struct JNINativeInterfaceCompose;
template<class X, class ...jnitypes> struct JNINativeInterfaceCompose<X, jnitypes...> {
	using Type = decltype(std::tuple_cat(std::declval<std::tuple<X, X, X>>(), std::declval<typename JNINativeInterfaceCompose<jnitypes...>::Type>()));
};
template<> struct JNINativeInterfaceCompose<> {
	using Type = std::tuple<>;
	template<class Y> using index = std::integral_constant<size_t, 0>;	
};

template<bool ReturnNull, class ...jnitypes> struct InterfaceFactory {
	using SeqM2 = std::make_index_sequence<sizeof...(jnitypes) - 2>;
	using SeqM1 = std::make_index_sequence<sizeof...(jnitypes) - 1>;
	using Seq = std::make_index_sequence<sizeof...(jnitypes) * 3>;
	using compose = JNINativeInterfaceCompose<jnitypes...>;
	using composeType = typename compose::Type;
	template<class seq> struct Impl;
	template<size_t...I> struct Impl<std::integer_sequence<size_t, I...>> {
		template<class seq> struct Impl2;
		template<size_t...IM2> struct Impl2<std::integer_sequence<size_t, IM2...>> {
			template<class seq> struct Impl3;
			template<size_t...IM1> struct Impl3<std::integer_sequence<size_t, IM1...>> {
				static constexpr JNINativeInterface Get() {
					return {
						NULL,
						NULL,
						NULL,
						NULL,
						GetVersion,
						DefineClass,
						FindClass<ReturnNull>,
						FromReflectedMethod,
						FromReflectedField,
						ToReflectedMethod,
						GetSuperclass,
						IsAssignableFrom,
						ToReflectedField,
						Throw,
						ThrowNew,
						ExceptionOccurred,
						ExceptionDescribe,
						ExceptionClear,
						FatalError,
						PushLocalFrame,
						PopLocalFrame,
						NewGlobalRef,
						DeleteGlobalRef,
						DeleteLocalRef,
						IsSameObject,
						NewLocalRef,
						EnsureLocalCapacity,
						AllocObject,
						MDispatch<jobject, jclass>::CallMethod,
						MDispatch<jobject, jclass>::CallMethod,
						MDispatch<jobject, jclass>::CallMethod,
						GetObjectClass,
						IsInstanceOf,
						GetMethodID<false, ReturnNull>,
						MDispatch<std::tuple_element_t<I, composeType>, jobject>::CallMethod ...,
						MDispatch<std::tuple_element_t<I, composeType>, jobject, jclass>::CallMethod ...,
						GetFieldID<false, ReturnNull>,
						GetField<ReturnNull, std::tuple_element_t<IM1, std::tuple<jnitypes...>>, jobject>...,
						SetField<std::tuple_element_t<IM1, std::tuple<jnitypes...>>, jobject>...,
						GetMethodID<true, ReturnNull>,
						MDispatch<std::tuple_element_t<I, composeType>, jclass>::CallMethod ...,
						GetFieldID<true, ReturnNull>,
						GetField<ReturnNull, std::tuple_element_t<IM1, std::tuple<jnitypes...>>, jclass>...,
						SetField<std::tuple_element_t<IM1, std::tuple<jnitypes...>>, jclass>...,
						NewString,
						GetStringLength,
						GetStringChars,
						ReleaseStringChars,
						NewStringUTF,
						GetStringUTFLength,
						GetStringUTFChars,
						ReleaseStringUTFChars,
						GetArrayLength,
						NewObjectArray,
						GetObjectArrayElement,
						SetObjectArrayElement,
						NewArray<std::tuple_element_t<IM2 + 1, std::tuple<jnitypes...>>>...,
						GetArrayElements<std::tuple_element_t<IM2 + 1, std::tuple<jnitypes...>>>...,
						ReleaseArrayElements<std::tuple_element_t<IM2 + 1, std::tuple<jnitypes...>>>...,
						GetArrayRegion<std::tuple_element_t<IM2 + 1, std::tuple<jnitypes...>>>...,
						SetArrayRegion<std::tuple_element_t<IM2 + 1, std::tuple<jnitypes...>>>...,
						RegisterNatives,
						UnregisterNatives,
						MonitorEnter,
						MonitorExit,
						::GetJavaVM,
						GetStringRegion,
						GetStringUTFRegion,
						GetArrayElements<void>,
						ReleaseArrayElements<void>,
						GetStringChars,
						ReleaseStringChars,
						NewWeakGlobalRef,
						DeleteWeakGlobalRef,
						ExceptionCheck,
						NewDirectByteBuffer,
						GetDirectBufferAddress,
						GetDirectBufferCapacity,
						GetObjectRefType,
					};
				}
			};
		};
	};
	using Type = typename Impl<Seq>::template Impl2<SeqM2>::template Impl3<SeqM1>;
};

template<bool ReturnNull> JNINativeInterface jnivm::VM::GetNativeInterfaceTemplate() {
	return InterfaceFactory<ReturnNull, jobject, jboolean, jbyte, jchar, jshort, jint, jlong, jfloat, jdouble, void>::Type::Get();
}

jnivm::VM::VM(bool skipInit, bool ReturnNull) : ninterface(ReturnNull ? GetNativeInterfaceTemplate<true>() : GetNativeInterfaceTemplate<false>()), iinterface({
			this,
			NULL,
			NULL,
			[](JavaVM *) -> jint {

				return JNI_OK;
			},
			[](JavaVM *vm, JNIEnv **penv, void * args) -> jint {
#ifdef EnableJNIVMGC
				auto&& nvm = *VM::FromJavaVM(vm);
				std::lock_guard<std::mutex> lock(nvm.mtx);
				auto&& nenv = nvm.jnienvs[pthread_self()];
				if(!nenv) {
					nenv = nvm.CreateEnv();
				}
				if(penv) {
					*penv = nenv->GetJNIEnv();
				}
#else
				if(penv) {
					std::lock_guard<std::mutex> lock((VM::FromJavaVM(vm)))->mtx);
					*penv = (VM::FromJavaVM(vm)))->GetJNIEnv();
				}
#endif
				return JNI_OK;
			},
			[](JavaVM *vm) -> jint {
#ifdef EnableJNIVMGC
				auto&& nvm = *VM::FromJavaVM(vm);
				std::lock_guard<std::mutex> lock(nvm.mtx);
				auto fe = nvm.jnienvs.end();
				auto f = nvm.jnienvs.find(pthread_self());
				if(f != fe) {
					nvm.jnienvs.erase(f);
				}
#endif
				return JNI_OK;
			},
			[](JavaVM *vm, void **penv, jint) -> jint {
				if(penv) {
#ifdef EnableJNIVMGC
					return vm->AttachCurrentThread((JNIEnv**)penv, nullptr);
#else
					std::lock_guard<std::mutex> lock((VM::FromJavaVM(vm)))->mtx);
					*penv = (VM::FromJavaVM(vm)))->GetJNIEnv();
#endif
				}
				return JNI_OK;
			},
			[](JavaVM * vm, JNIEnv ** penv, void * args) -> jint {
				return vm->AttachCurrentThread(penv, args);
			},
	}) {

#ifdef JNI_DEBUG
	np.name = "jnivm";
#endif
	javaVM.functions = &iinterface;
	if(skipInit == false)
		initialize();
}

jnivm::VM::VM() : VM(false) {};

void VM::initialize() {
	auto env = jnienvs[pthread_self()] = CreateEnv();
	env->GetClass<Object>("java/lang/Object");
	env->GetClass<Class>("java/lang/Class");
	env->GetClass<String>("java/lang/String");
	env->GetClass<ByteBuffer>("java/nio/ByteBuffer");
	env->GetClass<Throwable>("java/lang/Throwable");
	env->GetClass<Method>("java/lang/reflect/Method");
	env->GetClass<Field>("java/lang/reflect/Field");
	env->GetClass<Weak>("java/lang/ref/WeakReference");
	env->GetClass<Global>("internal/lang/Global");
}

JavaVM *VM::GetJavaVM() {
	return &javaVM;
}

JNIEnv *VM::GetJNIEnv() {
	return GetEnv()->GetJNIEnv();
}

const std::shared_ptr<ENV>& VM::GetEnv() {
#ifdef EnableJNIVMGC
	return jnienvs[pthread_self()];
#else
	return jnienvs.begin()->second;
#endif
}

void jnivm::VM::OverrideJNIInvokeInterface(const JNIInvokeInterface &iinterface) {
	if(iinterface.reserved0 != nullptr && iinterface.reserved0 != this->iinterface.reserved0) {
		throw std::runtime_error("Updating `iinterface.reserved0` to a different value is forbidden");
	}
	auto reserved0 = this->iinterface.reserved0;
	this->iinterface = iinterface;
	this->iinterface.reserved0 = reserved0;
}

std::shared_ptr<jnivm::ENV> jnivm::VM::CreateEnv() {
	auto tmpl = GetNativeInterfaceTemplate();
	for(auto && hook : jnienvhooks) {
        hook(tmpl);
    }
	return std::make_shared<ENV>(this, tmpl);
}

const JNINativeInterface &jnivm::VM::GetNativeInterfaceTemplate() {
	return ninterface;
}

jnivm::VM *jnivm::VM::FromJavaVM(JavaVM *vm) {
	if(vm == nullptr || vm->functions->reserved0 == nullptr) throw std::runtime_error("Failed to get reference to jnivm::VM");
    return static_cast<jnivm::VM*>(vm->functions->reserved0);
}

template JNINativeInterface jnivm::VM::GetNativeInterfaceTemplate<true>();
template JNINativeInterface jnivm::VM::GetNativeInterfaceTemplate<false>();