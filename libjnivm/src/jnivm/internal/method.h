#include <jnivm/method.h>
#include <jnivm/class.h>
#include <jnivm/internal/findclass.h>
#include <functional>

namespace jnivm {

    template<bool isStatic, bool ReturnNull = false, bool AllowNative = false, bool trace = true>
    jmethodID GetMethodID(JNIEnv *env, jclass cl, const char *str0, const char *str1);

    template<class T> T defaultVal(ENV* env, std::string signature);
    template<> void defaultVal(ENV* env, std::string signature);
    template<> jobject defaultVal(ENV* env, std::string signature);

    template <class T, class...Y> struct MDispatchBase {
        static T CallMethod(JNIEnv * env, Y...p, jmethodID id, va_list param);
    };

    template <class T> struct MDispatchBase2 {
        static T CallMethod(JNIEnv * env, jobject obj, jmethodID id, jvalue * param);
        static T CallMethod(JNIEnv * env, jobject obj, jclass cl, jmethodID id, jvalue * param);
        static T CallMethod(JNIEnv * env, jclass cl, jmethodID id, jvalue * param);
    };

    template <class T, class...Y> struct MDispatch : MDispatchBase<T, Y...>, MDispatchBase2<T> {
        using MDispatchBase<T, Y...>::CallMethod;
        using MDispatchBase2<T>::CallMethod;
        static T CallMethod(JNIEnv * env, Y...p, jmethodID id, ...);
    };

    template <class...Y> struct MDispatch<void, Y...> : MDispatchBase<void, Y...>, MDispatchBase2<void> {
        using MDispatchBase<void, Y...>::CallMethod;
        using MDispatchBase2<void>::CallMethod;
        static void CallMethod(JNIEnv * env, Y...p, jmethodID id, ...);
    };
}