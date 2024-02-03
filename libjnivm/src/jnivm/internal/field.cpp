#include "field.hpp"
#include <jnivm/internal/findclass.h>
#include "log.h"
#include <jnivm/field.h>
#include <jnivm/class.h>
#include <functional>
#include "method.h"

using namespace jnivm;

template<bool isStatic, bool ReturnNull, bool trace>
jfieldID jnivm::GetFieldID(JNIEnv *env, jclass cl_, const char *name, const char *type) {
    auto cl = JNITypes<std::shared_ptr<Class>>::JNICast(ENV::FromJNIEnv(env), cl_);
    std::lock_guard<std::mutex> lock(cl->mtx);
    std::string &classname = cl->name;

    auto cur = cl;
    auto sname = name;
    auto ssig = type;
    auto ccl =
            std::find_if(cur->fields.begin(), cur->fields.end(),
                                    [&sname, &ssig](std::shared_ptr<Field> &namesp) {
                                        return namesp->_static == isStatic && namesp->name == sname && namesp->type == ssig;
                                    });
    std::shared_ptr<Field> next;
    if (ccl != cur->fields.end()) {
        next = *ccl;
#ifdef JNI_TRACE
        LOG("JNIVM", "Found symbol, Class=`%s`, %sField=`%s`, Signature=`%s`", cl ? cl->nativeprefix.data() : nullptr, isStatic ? "Static" : "", name, type);
#endif
    } else {
        if(cur->baseclasses) {
            for(auto&& i : cur->baseclasses(ENV::FromJNIEnv(env))) {
                if(i) {
                    auto id = GetFieldID<isStatic, true, false>(env, (jclass)i.get(), name, type);
                    if(id) {
                        return id;
                    }
                }
            }
        }
        if(ReturnNull) {
#ifdef JNI_TRACE
            if(trace) {
                LOG("JNIVM", "Unresolved symbol, Class=`%s`, %sField=`%s`, Signature=`%s`", cl ? cl->nativeprefix.data() : nullptr, isStatic ? "Static" : "", name, type);
            }
#endif
            return nullptr;
        }
        next = std::make_shared<Field>();
        cur->fields.emplace_back(next);
        next->name = std::move(sname);
        next->type = std::move(ssig);
        next->_static = isStatic;
#ifdef JNI_DEBUG
        Declare(env, next->type.data());
#endif
#ifdef JNI_TRACE
        LOG("JNIVM", "Constructed Unresolved symbol, Class=`%s`, %sField=`%s`, Signature=`%s`", cl ? cl->nativeprefix.data() : nullptr, isStatic ? "Static" : "", name, type);
#endif
    }
    return (jfieldID)next.get();
};

namespace Util {
    std::shared_ptr<Class> GetClass(jnivm::ENV* env, jobject o) {
        return JNITypes<std::shared_ptr<Class>>::JNICast(env, env->GetJNIEnv()->GetObjectClass(o));
    }
    std::shared_ptr<Class> GetClass(jnivm::ENV* env, jclass o) {
        return JNITypes<std::shared_ptr<Class>>::JNICast(env, o);
    }
    struct Proxy {
        std::shared_ptr<Class> in;
        operator Class*() {
            return in.get();
        }
    };
    Proxy GetParam(jnivm::ENV* env, jclass c) {
        return { GetClass(env, c) };
    }
    jobject GetParam(jnivm::ENV* env, jobject c) {
        return c;
    }
};

template<bool> struct Caller {
    template<class T>
    static T Get(jnivm::impl::MethodHandleBase<T>*p, ENV*env, jobject val) {
        return p->InstanceGet(env, val, nullptr, {});
    }
    template<class T>
    static void Set(jnivm::impl::MethodHandleBase<T>*p, ENV*env, jobject val, jvalue* r) {
        return p->InstanceSet(env, val, r, {});
    }
};
template<> struct Caller<true> {
    template<class T>
    static T Get(jnivm::impl::MethodHandleBase<T>*p, ENV*env, jnivm::Class* val) {
        return p->StaticGet(env, val, nullptr, {});
    }
    template<class T>
    static void Set(jnivm::impl::MethodHandleBase<T>*p, ENV*env, jnivm::Class* val, jvalue* r) {
        return p->StaticSet(env, val, r, {});
    }
};

template<bool RetNull, class T, class O> T jnivm::GetField(JNIEnv *env, O obj, jfieldID id) {
    auto fid = ((Field *)id);
#ifdef JNI_DEBUG
    if(!obj)
        LOG("JNIVM", "GetField object is null");
    if(!id)
        LOG("JNIVM", "GetField field is null");
#endif
    if (fid && fid->getnativehandle) {
        if(fid->_static != std::is_same<O, jclass>::value) {
            env->FatalError("GetField / Invalid Field ID");
        }
#ifdef JNI_TRACE
        auto cl = Util::GetClass(ENV::FromJNIEnv(env), obj);
        LOG("JNIVM", "Invoked Field Getter Class=`%s` Field=`%s` Signature=`%s`", cl ? cl->nativeprefix.data() : "???", fid->name.data(), fid->type.data());
#endif
        return Caller<std::is_same<O, jclass>::value>::template Get<T>(fid->getnativehandle.get(), ENV::FromJNIEnv(env), Util::GetParam(ENV::FromJNIEnv(env), obj));
    } else {
#ifdef JNI_TRACE
        auto cl = Util::GetClass(ENV::FromJNIEnv(env), obj);
        LOG("JNIVM", "Invoked Unknown Field Getter Class=`%s` Field=`%s` Signature=`%s`", cl ? cl->nativeprefix.data() : "???", fid ? fid->name.data() : "???", fid ? fid->type.data() : "???");
#endif
        return defaultVal<T>(ENV::FromJNIEnv(env), fid ? fid->type : "");
    }
}

template<class T, class O> void jnivm::SetField(JNIEnv *env, O obj, jfieldID id, T value) {
    auto fid = ((Field *)id);
#ifdef JNI_DEBUG
    if(!obj)
        LOG("JNIVM", "SetField class or object is null");
    if(!id)
        LOG("JNIVM", "SetField field is null");
#endif
    if (obj && fid && fid->setnativehandle) {
        if(fid->_static != std::is_same<O, jclass>::value) {
            env->FatalError("SetField / Invalid Field ID");
        }
#ifdef JNI_TRACE
        auto cl = Util::GetClass(ENV::FromJNIEnv(env), obj);
        LOG("JNIVM", "Invoked Field Setter Class=`%s` Field=`%s` Signature=`%s`", cl ? cl->nativeprefix.data() : "???", fid->name.data(), fid->type.data());
#endif
        jvalue val;
        memset(&val, 0, sizeof(val));
        memcpy(&val, &value, sizeof(T));
        return Caller<std::is_same<O, jclass>::value>::template Set<T>(fid->setnativehandle.get(), ENV::FromJNIEnv(env), Util::GetParam(ENV::FromJNIEnv(env), obj), &val);
    } else {
#ifdef JNI_TRACE
        auto cl = Util::GetClass(ENV::FromJNIEnv(env), obj);
        LOG("JNIVM", "Invoked Unknown Field Setter Class=`%s` Field=`%s` Signature=`%s`", cl ? cl->nativeprefix.data() : "???", fid ? fid->name.data() : "???", fid ? fid->type.data() : "???");
#endif
    }
}

template jfieldID jnivm::GetFieldID<true, true>(JNIEnv *env, jclass cl, const char *name, const char *type);
template jfieldID jnivm::GetFieldID<false, true>(JNIEnv *env, jclass cl, const char *name, const char *type);
template jfieldID jnivm::GetFieldID<true, false>(JNIEnv *env, jclass cl, const char *name, const char *type);
template jfieldID jnivm::GetFieldID<false, false>(JNIEnv *env, jclass cl, const char *name, const char *type);

#define DeclareTemplate(T) template void jnivm::SetField<T, jclass>(JNIEnv *env, jclass obj, jfieldID id, T value); \
                           template T jnivm::GetField<true, T, jclass>(JNIEnv *env, jclass obj, jfieldID id);\
                           template T jnivm::GetField<false, T, jclass>(JNIEnv *env, jclass obj, jfieldID id);\
                           template void jnivm::SetField<T, jobject>(JNIEnv *env, jobject obj, jfieldID id, T value); \
                           template T jnivm::GetField<true, T, jobject>(JNIEnv *env, jobject obj, jfieldID id);\
                           template T jnivm::GetField<false, T, jobject>(JNIEnv *env, jobject obj, jfieldID id)
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
