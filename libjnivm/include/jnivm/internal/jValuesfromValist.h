#include <jni.h>
#include <vector>
#include <cstdarg>

namespace jnivm {
    std::vector<jvalue> JValuesfromValist(va_list list, const char* signature);
}