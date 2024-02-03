#include <jnivm/jnitypes.h>
#include <jnivm/string.h>
#include <sstream>

namespace jnivm {

    jstring NewString(JNIEnv *env, const jchar * str, jsize size);
    jsize GetStringLength(JNIEnv *env, jstring str);
    const jchar *GetStringChars(JNIEnv * env, jstring str, jboolean * copy);
    void ReleaseStringChars(JNIEnv * env, jstring str, const jchar * cstr);
    jstring NewStringUTF(JNIEnv * env, const char *str);
    jsize GetStringUTFLength(JNIEnv *, jstring str);
    const char *GetStringUTFChars(JNIEnv * env, jstring str, jboolean *copy);
    void ReleaseStringUTFChars(JNIEnv * env, jstring str, const char * cstr);
    void GetStringRegion(JNIEnv *, jstring str, jsize start, jsize length, jchar * buf);
    void GetStringUTFRegion(JNIEnv *, jstring str, jsize start, jsize len, char * buf);

}