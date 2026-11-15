#include "method.h"
#include "log.h"
#include <jnivm/internal/jValuesfromValist.h>
#include <set>
#include <vector>

using namespace jnivm;

template<bool isStatic, bool ReturnNull, bool AllowNative, bool trace>
jmethodID jnivm::GetMethodID(JNIEnv *env, jclass cl, const char *str0, const char *str1) {
    std::shared_ptr<Method> next;
    std::string sname = str0 ? str0 : "";
    std::string ssig = str1 ? str1 : "";
    auto cur = JNITypes<std::shared_ptr<Class>>::JNICast(ENV::FromJNIEnv(env), cl);
    if(cur) {
        // Rewrite init to Static external function
        if(!isStatic && sname == "<init>") {
            {
                std::lock_guard<std::mutex> lock(cur->mtx);
                auto acbrack = ssig.find(')') + 1;
                ssig.erase(acbrack, std::string::npos);
                ssig.append("L");
                ssig.append(cur->nativeprefix);
                ssig.append(";");
            }
            return GetMethodID<true, ReturnNull, AllowNative, trace>(env, cl, str0, ssig.data());
        }
        else {
            std::lock_guard<std::mutex> lock(cur->mtx);
            std::string &classname = cur->name;
            auto ccl =
                    std::find_if(cur->methods.begin(), cur->methods.end(),
                                            [&sname, &ssig](std::shared_ptr<Method> &namesp) {
                                                return namesp->_static == isStatic && AllowNative == (bool)namesp->native && namesp->name == sname && namesp->signature == ssig;
                                            });
            if (ccl != cur->methods.end()) {
                next = *ccl;
            }
        }
    } else {
#ifdef JNI_DEBUG
        LOG("JNIVM", "Get%sMethodID class is null %s, %s", AllowNative ? "Native" : isStatic ? "Static" : "", str0, str1);
#endif
    }
    if(!next) {
        if(cur && cur->baseclasses) {
            for(auto&& i : cur->baseclasses(ENV::FromJNIEnv(env))) {
                if(i) {
                    auto id = GetMethodID<isStatic, true, AllowNative, false>(env, (jclass)i.get(), str0, str1);
                    if(id) {
                        return id;
                    }
                }
            }
        }
        if(ReturnNull) {
#ifdef JNI_TRACE
            if(trace) {
                LOG("JNIVM", "Unresolved symbol, Class=`%s`, %sMethod=`%s`, Signature=`%s`", cur ? cur->nativeprefix.data() : nullptr, AllowNative ? "Native" : isStatic ? "Static" : "", str0, str1);
            }
#endif
            return nullptr;
        }
        if(cur) {
            // Other threads read cur->methods (and the fields of what is in it) under this lock. So: fill the new
            // method in completely *before* it is published, publish under the lock, and look again first because
            // another thread may have created the same method since our lookup (it would be listed twice).
            std::lock_guard<std::mutex> lock(cur->mtx);
            auto again = std::find_if(cur->methods.begin(), cur->methods.end(),
                                      [&sname, &ssig](std::shared_ptr<Method> &namesp) {
                                          return namesp->_static == isStatic && AllowNative == (bool)namesp->native && namesp->name == sname && namesp->signature == ssig;
                                      });
            if(again != cur->methods.end()) {
                return (jmethodID)again->get();
            }
            next = std::make_shared<Method>();
            next->name = std::move(sname);
            next->signature = std::move(ssig);
            next->_static = isStatic;
            cur->methods.push_back(next);
        } else {
            next = std::make_shared<Method>();
            next->name = std::move(sname);
            next->signature = std::move(ssig);
            next->_static = isStatic;
            // For Debugging purposes without valid parent class
            JNITypes<std::shared_ptr<Method>>::ToJNIType(ENV::FromJNIEnv(env), next);
        }
#ifdef JNI_DEBUG
        Declare(env, next->signature.data());
#endif
#ifdef JNI_TRACE
        LOG("JNIVM", "Constructed Unresolved symbol, Class=`%s`, %sMethod=`%s`, Signature=`%s`", cur ? cur->nativeprefix.data() : nullptr, AllowNative ? "Native" : isStatic ? "Static" : "", str0, str1);
#endif
    } else {
#ifdef JNI_TRACE
        LOG("JNIVM", "Found symbol, Class=`%s`, %sMethod=`%s`, Signature=`%s`", cur ? cur->nativeprefix.data() : nullptr, AllowNative ? "Native" : isStatic ? "Static" : "", str0, str1);
#endif
    }
    return (jmethodID)next.get();
};

template<class T> T jnivm::defaultVal(ENV* env, std::string signature) {
    return {};
}
template<> void jnivm::defaultVal(ENV* env, std::string signature) {
}

template<> jobject jnivm::defaultVal(ENV* env, std::string signature) {
#ifdef JNI_RETURN_NON_ZERO
    if(!signature.empty()) {
        size_t off = signature.find_last_of(")");
        if(signature[off + 1] == '[' ){
#ifdef JNI_TRACE
            LOG("JNIVM", "Construct array=`%s` via New*Array", &signature.data()[off + 1]);
#endif
            switch (signature[off + 2])
            {
            case 'B':
                return env->GetJNIEnv()->NewByteArray(0);
            case 'S':
                return env->GetJNIEnv()->NewShortArray(0);
            case 'C':
                return env->GetJNIEnv()->NewCharArray(0);
            case 'I':
                return env->GetJNIEnv()->NewIntArray(0);
            case 'J':
                return env->GetJNIEnv()->NewLongArray(0);
            case 'F':
                return env->GetJNIEnv()->NewFloatArray(0);
            case 'D':
                return env->GetJNIEnv()->NewDoubleArray(0);
            case 'L':
            case '[':
                return env->GetJNIEnv()->NewObjectArray(0, env->GetJNIEnv()->FindClass(signature[off + 2] == 'L' && signature[signature.size() - 1] == ';' ? signature.substr(off + 3, signature.size() - (off + 4)).data() : &signature.data()[off + 1]), nullptr);
            default:
#ifdef JNI_TRACE
            LOG("JNIVM", "Constructing array=`%s` failed unknown type", &signature.data()[off + 1]);
#endif
                break;
            }
            
        } else if(signature[off + 1] == 'L' && signature[signature.size() - 1] == ';'){
            auto c = env->GetClass(signature.substr(off + 2, signature.size() - (off + 3)).data());
            if(c->Instantiate) {
#ifdef JNI_TRACE
                LOG("JNIVM", "Construct object=`%s` via default constructor", &signature.data()[off + 1]);
#endif
                return JNITypes<std::shared_ptr<Object>>::ToJNIReturnType(env, c->Instantiate(env));
            } else {
                std::lock_guard<std::mutex> guard(env->GetVM()->mtx);
                bool safetocreatedummy = true;
                for(auto&& ty : env->GetVM()->typecheck) {
                    if(ty.second == c) {
                        safetocreatedummy = false;
                        break;
                    }
                }
                if(safetocreatedummy) {
#ifdef JNI_TRACE
                    LOG("JNIVM", "Construct dummy object=`%s`, no native type attached to this Class", &signature.data()[off + 1]);
#endif
                    auto dummy = std::make_shared<Object>();
                    dummy->clazz = c;
                    return JNITypes<std::shared_ptr<Object>>::ToJNIReturnType(env, dummy);
                } else {
#ifdef JNI_TRACE
                    LOG("JNIVM", "You have to create a default constructor for object=`%s` to get a non zero return value", &signature.data()[off + 1]);
#endif
                }
            }
        }
#ifdef JNI_TRACE
        LOG("JNIVM", "Failed to construct return value of signature=`%s`", signature.data());
#endif
    } else {
#ifdef JNI_TRACE
        LOG("JNIVM", "Failed to guess return value of empty signature");
#endif
    }
#endif
    return nullptr;
}

static Method* findNonVirtualOverload(Class*cl, Method*mid) {
    if(!cl) {
        return mid;
    }
    std::lock_guard<std::mutex> lock(cl->mtx);
    auto res = std::find_if(cl->methods.begin(), cl->methods.end(), [mid](auto&& m) {
        return m && !m->_static && mid->name == m->name && mid->signature == m->signature && m->nativehandle;
    });
    if(res != cl->methods.end()) {
        return res->get();
    } else {
        return nullptr;
    }
}

// Looks for an implementation of mid in the object's class, then in its ancestors, and falls back to mid itself.
// (This used to recurse on the *method object's* base classes, which never terminated for an inherited method
// and dereferenced the empty result of the base class list.)
static Method* findVirtualOverload(jnivm::ENV *env, Class*cl, Method*mid) {
    std::vector<std::shared_ptr<Class>> keepalive;
    std::vector<Class*> pending { cl };
    std::set<Class*> seen;
    for(size_t n = 0; n < pending.size() && n < 256; n++) {
        Class* c = pending[n];
        if(!c || !seen.insert(c).second) {
            continue;
        }
        if(auto r = findNonVirtualOverload(c, mid)) {
            return r;
        }
        if(c->baseclasses) {
            for(auto&& b : c->baseclasses(env)) {
                if(b) {
                    keepalive.push_back(b);
                    pending.push_back(b.get());
                }
            }
        }
    }
    return mid;
}

template<class T> T jnivm::MDispatchBase2<T>::CallMethod(JNIEnv *env, jobject obj, jmethodID id, jvalue *param) {
    auto mid = ((Method *)id);
#ifdef JNI_DEBUG
    if(!obj)
        LOG("JNIVM", "CallMethod object is null");
    if(!id)
        LOG("JNIVM", "CallMethod field is null");
#endif
    if (mid && mid->nativehandle) {
        auto cl = JNITypes<std::shared_ptr<Class>>::JNICast(ENV::FromJNIEnv(env), env->GetObjectClass(obj));
        mid = findVirtualOverload(ENV::FromJNIEnv(env), cl.get(), mid);
#ifdef JNI_TRACE
        LOG("JNIVM", "Call Member Function Class=`%s` Method=`%s` Signature=`%s`", cl ? cl->nativeprefix.data() : "???", mid->name.data(), mid->signature.data());
#endif
        try {
            return static_cast<jnivm::impl::MethodHandleBase<T>*>(mid->nativehandle.get())->InstanceInvoke(ENV::FromJNIEnv(env), obj, param, jnivm::impl::MethodHandleBase<T>{});
        } catch (...) {
            auto cur = std::make_shared<Throwable>();
            cur->except = std::current_exception();
            (ENV::FromJNIEnv(env))->current_exception = cur;
#ifdef JNI_TRACE
            env->ExceptionDescribe();
#endif
            return defaultVal<T>(ENV::FromJNIEnv(env), mid ? mid->signature : "");
        }
    } else {
#ifdef JNI_TRACE
        auto cl = JNITypes<std::shared_ptr<Class>>::JNICast(ENV::FromJNIEnv(env), env->GetObjectClass(obj));
        LOG("JNIVM", "Call Unknown Member Function Class=`%s` Method=`%s` Signature=`%s`", cl ? cl->nativeprefix.data() : "???", mid ? mid->name.data() : "???", mid ? mid->signature.data() : "???");
#endif
        return defaultVal<T>(ENV::FromJNIEnv(env), mid ? mid->signature : "");
    }
};

template<class T, class... Y> T jnivm::MDispatchBase<T, Y...>::CallMethod(JNIEnv *env, Y ...p, jmethodID id, va_list param) {
    if(id) {
        return MDispatch<T, Y...>::CallMethod(env, p..., id, JValuesfromValist(param, ((Method *)id)->signature.data()).data());
    } else {
#ifdef JNI_TRACE
        LOG("JNIVM", "CallMethod Method ID is null");
#endif
        return defaultVal<T>(ENV::FromJNIEnv(env), "");
    }
};

template<class T, class... Y> T jnivm::MDispatch<T, Y...>::CallMethod(JNIEnv *env, Y ...p, jmethodID id, ...) {
    va_list l;
    va_start(l, id);
    T ret = MDispatch<T, Y...>::CallMethod(env, p..., id, l);
    va_end(l);
    return ret;
};

template<class... Y> void jnivm::MDispatch<void, Y...>::CallMethod(JNIEnv *env, Y ...p, jmethodID id, ...) {
    va_list l;
    va_start(l, id);
    MDispatch<void, Y...>::CallMethod(env, p..., id, l);
    va_end(l);
};

template<class T> T jnivm::MDispatchBase2<T>::CallMethod(JNIEnv *env, jobject obj, jclass cl, jmethodID id, jvalue *param) {
    auto mid = ((Method *)id);
#ifdef JNI_DEBUG
    if(!obj)
        LOG("JNIVM", "CallMethod object is null");
    if(!cl)
        LOG("JNIVM", "CallMethod class is null");
    if(!id)
        LOG("JNIVM", "CallMethod field is null");
#endif
    if (mid && mid->nativehandle) {
        auto clz = JNITypes<std::shared_ptr<Class>>::JNICast(ENV::FromJNIEnv(env), cl); 
#ifdef JNI_TRACE
        LOG("JNIVM", "Call NonVirtual Member Function Class=`%s` Method=`%s` Signature=`%s`", clz ? clz->nativeprefix.data() : "???", mid->name.data(), mid ? mid->signature.data() : "???");
#endif
        if(auto overload = findNonVirtualOverload(clz.get(), mid)) {
            mid = overload;
        }
        try {
            return static_cast<jnivm::impl::MethodHandleBase<T>*>(mid->nativehandle.get())->NonVirtualInstanceInvoke(ENV::FromJNIEnv(env), obj, param, jnivm::impl::MethodHandleBase<T>{});
        } catch (...) {
            auto cur = std::make_shared<Throwable>();
            cur->except = std::current_exception();
            (ENV::FromJNIEnv(env))->current_exception = cur;
#ifdef JNI_TRACE
            env->ExceptionDescribe();
#endif
            return defaultVal<T>(ENV::FromJNIEnv(env), mid ? mid->signature : "");
        }
    } else {
#ifdef JNI_TRACE
        auto clz = JNITypes<std::shared_ptr<Class>>::JNICast(ENV::FromJNIEnv(env), cl);
        LOG("JNIVM", "Call Unknown NonVirtual Member Function Class=`%s` Method=`%s` Signature=`%s`", clz ? clz->nativeprefix.data() : "???", mid ? mid->name.data() : "???", mid ? mid->signature.data() : "???");
#endif
        return defaultVal<T>(ENV::FromJNIEnv(env), mid ? mid->signature : "");
    }
};

template<class T> T jnivm::MDispatchBase2<T>::CallMethod(JNIEnv *env, jclass _cl, jmethodID id, jvalue *param) {
    auto cl = JNITypes<std::shared_ptr<Class>>::JNICast(ENV::FromJNIEnv(env), _cl);
    auto mid = ((Method *)id);
#ifdef JNI_DEBUG
    if(!cl)
        LOG("JNIVM", "CallMethod class is null");
    if(!id)
        LOG("JNIVM", "CallMethod method is null");
#endif
    if (mid && mid->nativehandle) {
#ifdef JNI_TRACE
        LOG("JNIVM", "Call Static Function Class=`%s` Method=`%s` Signature=`%s`", cl ? cl->nativeprefix.data() : "???", mid->name.data(), mid ? mid->signature.data() : "???");
#endif
        try {
             return static_cast<jnivm::impl::MethodHandleBase<T>*>(mid->nativehandle.get())->StaticInvoke(ENV::FromJNIEnv(env), cl.get(), param, jnivm::impl::MethodHandleBase<T>{});
        } catch (...) {
            auto cur = std::make_shared<Throwable>();
            cur->except = std::current_exception();
            (ENV::FromJNIEnv(env))->current_exception = cur;
#ifdef JNI_TRACE
            env->ExceptionDescribe();
#endif
            return defaultVal<T>(ENV::FromJNIEnv(env), mid ? mid->signature : "");
        }
    } else {
#ifdef JNI_TRACE
        LOG("JNIVM", "Call Unknown Static Function Class=`%s` Method=`%s` Signature=`%s`", cl ? cl->nativeprefix.data() : "???", mid ? mid->name.data() : "???", mid ? mid->signature.data() : "???");
#endif
        return defaultVal<T>(ENV::FromJNIEnv(env), mid ? mid->signature : "");
    }
};

template jmethodID jnivm::GetMethodID<true>(JNIEnv *env, jclass cl, const char *str0, const char *str1);
template jmethodID jnivm::GetMethodID<false>(JNIEnv *env, jclass cl, const char *str0, const char *str1);
template jmethodID jnivm::GetMethodID<false, true, true>(JNIEnv *env, jclass cl, const char *str0, const char *str1);
template jmethodID jnivm::GetMethodID<false, true, false, false>(JNIEnv *env, jclass cl, const char *str0, const char *str1);
template jmethodID jnivm::GetMethodID<true, true, false, true>(JNIEnv *env, jclass cl, const char *str0, const char *str1);
template jmethodID jnivm::GetMethodID<false, true, false, true>(JNIEnv *env, jclass cl, const char *str0, const char *str1);

#define DeclareTemplate(T) template T jnivm::MDispatchBase2<T>::CallMethod(JNIEnv *env, jobject obj, jmethodID id, jvalue *param);\
                           template T jnivm::MDispatchBase<T, jobject>::CallMethod(JNIEnv *env, jobject obj, jmethodID id, va_list param);\
                           template T jnivm::MDispatch<T, jobject>::CallMethod(JNIEnv *env, jobject obj, jmethodID id, ...);\
                           template T jnivm::MDispatchBase2<T>::CallMethod(JNIEnv *env, jclass obj, jmethodID id, jvalue *param);\
                           template T jnivm::MDispatchBase<T, jclass>::CallMethod(JNIEnv *env, jclass obj, jmethodID id, va_list param);\
                           template T jnivm::MDispatch<T, jclass>::CallMethod(JNIEnv *env, jclass obj, jmethodID id, ...);\
                           template T jnivm::MDispatchBase2<T>::CallMethod(JNIEnv *env, jobject obj, jclass c, jmethodID id, jvalue *param);\
                           template T jnivm::MDispatchBase<T, jobject, jclass>::CallMethod(JNIEnv *env, jobject obj, jclass c, jmethodID id, va_list param);\
                           template T jnivm::MDispatch<T, jobject, jclass>::CallMethod(JNIEnv *env, jobject obj, jclass c, jmethodID id, ...)
DeclareTemplate(jboolean);
DeclareTemplate(jbyte);
DeclareTemplate(jshort);
DeclareTemplate(jint);
DeclareTemplate(jlong);
DeclareTemplate(jfloat);
DeclareTemplate(jdouble);
DeclareTemplate(jchar);
DeclareTemplate(jobject);
DeclareTemplate(void);
#undef DeclareTemplate

#define DeclareTemplate(T) template T jnivm::defaultVal(ENV* env, std::string signature)
DeclareTemplate(jboolean);
DeclareTemplate(jbyte);
DeclareTemplate(jshort);
DeclareTemplate(jint);
DeclareTemplate(jlong);
DeclareTemplate(jfloat);
DeclareTemplate(jdouble);
DeclareTemplate(jchar);
#undef DeclareTemplate