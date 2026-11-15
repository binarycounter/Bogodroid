#pragma once
#include <jni.h>

namespace jnivm {
    int UTFToJCharLength(const char * cur);
    jchar UTFToJChar(const char * cur, int& size);
    int JCharToUTFLength(jchar c);
    int JCharToUTF(jchar c, char* cur, int len);
}