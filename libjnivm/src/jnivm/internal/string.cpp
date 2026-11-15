#include <jnivm/class.h>
#include "string.hpp"
#include "stringUtil.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_set>
#include <vector>

using namespace jnivm;

jstring jnivm::NewString(JNIEnv *env, const jchar * str, jsize size) {
    std::stringstream ss;
    char out[3];
    for (jsize i = 0; i < size; i++) {
        ss.write(out, JCharToUTF(str[i], out, sizeof(out)));
    }
    return JNITypes<std::shared_ptr<String>>::ToJNIType(ENV::FromJNIEnv(env), std::make_shared<String>(ss.str()));
};
namespace {
// Decodes one code point of the (modified) UTF-8 in [cur, end). Never reads past end and never throws: the JNI
// string functions are called straight from native code, where an exception would terminate the process.
// 1-3 byte sequences as before (a 3-byte sequence may encode a lone surrogate, which is how NewString stores
// UTF-16), plus standard 4-byte sequences (emoji), which used to raise UtfDataFormatError. Bytes that aren't valid
// UTF-8 decode as U+FFFD, one byte at a time.
uint32_t decodeCodePoint(const char* cur, const char* end, int& size) {
    const unsigned char c0 = (unsigned char)*cur;
    const ptrdiff_t left = end - cur;
    auto cont = [&](int i) { return i < left && (((unsigned char)cur[i]) & 0xC0) == 0x80; };
    if(c0 < 0x80) {
        size = 1;
        return c0;
    }
    if((c0 & 0xE0) == 0xC0 && cont(1)) {
        size = 2;
        return ((c0 & 0x1Fu) << 6) | (cur[1] & 0x3Fu);
    }
    if((c0 & 0xF0) == 0xE0 && cont(1) && cont(2)) {
        size = 3;
        return ((c0 & 0x0Fu) << 12) | ((cur[1] & 0x3Fu) << 6) | (cur[2] & 0x3Fu);
    }
    if((c0 & 0xF8) == 0xF0 && cont(1) && cont(2) && cont(3)) {
        size = 4;
        return ((c0 & 0x07u) << 18) | ((cur[1] & 0x3Fu) << 12) | ((cur[2] & 0x3Fu) << 6) | (cur[3] & 0x3Fu);
    }
    size = 1;
    return 0xFFFD;
}

// The string as UTF-16 code units.
std::vector<jchar> toUnits(const std::string& str) {
    std::vector<jchar> units;
    units.reserve(str.size());
    const char* cur = str.data();
    const char* end = cur + str.size();
    while(cur < end) {
        int size;
        uint32_t cp = decodeCodePoint(cur, end, size);
        cur += size;
        if(cp >= 0x10000) {
            cp -= 0x10000;
            units.push_back((jchar)(0xD800 + (cp >> 10)));
            units.push_back((jchar)(0xDC00 + (cp & 0x3FF)));
        } else {
            units.push_back((jchar)cp);
        }
    }
    return units;
}

// Buffers handed out by GetStringChars. ReleaseStringChars frees exactly these (it used to leak every one, because
// freeing blindly was unsafe); anything else passed to it is ignored.
std::mutex charsMutex;
std::unordered_set<const jchar*>& charsHandedOut() {
    static std::unordered_set<const jchar*> set;
    return set;
}

std::shared_ptr<std::string> getString(JNIEnv* env, jstring str) {
    if(!str) {
        return nullptr;
    }
    return JNITypes<std::shared_ptr<String>>::JNICast(ENV::FromJNIEnv(env), str);
}
}

jsize jnivm::GetStringLength(JNIEnv *env, jstring str) {
    try {
        auto cstr = getString(env, str);
        return cstr ? (jsize)toUnits(*cstr).size() : 0;
    } catch(...) {
        return 0;
    }
};
const jchar *jnivm::GetStringChars(JNIEnv * env, jstring str, jboolean * copy) {
    if(copy) {
        *copy = true;
    }
    std::vector<jchar> units;
    try {
        auto cstr = getString(env, str);
        if(cstr) {
            units = toUnits(*cstr);
        }
    } catch(...) {
        units.clear();
    }
    jchar * jstr = new jchar[units.size() + 1];    // plus terminator
    std::copy(units.begin(), units.end(), jstr);
    jstr[units.size()] = 0;
    std::lock_guard<std::mutex> lock(charsMutex);
    charsHandedOut().insert(jstr);
    return jstr;
};
void jnivm::ReleaseStringChars(JNIEnv * env, jstring str, const jchar * cstr) {
    if(!cstr) {
        return;
    }
    std::lock_guard<std::mutex> lock(charsMutex);
    if(charsHandedOut().erase(cstr)) {
        delete[] cstr;
    }
}
jstring jnivm::NewStringUTF(JNIEnv * env, const char *str) {
    return JNITypes<std::shared_ptr<String>>::ToJNIType(ENV::FromJNIEnv(env), std::make_shared<String>(str ? str : ""));
};
jsize jnivm::GetStringUTFLength(JNIEnv *env, jstring str) {
    if(str) {
        auto length = JNITypes<std::shared_ptr<String>>::JNICast(ENV::FromJNIEnv(env), str)->length();
        if(length > static_cast<size_t>(std::numeric_limits<jsize>::max())) {
            throw std::runtime_error("String to long, to fit in jsize");
        } else {
            return static_cast<jsize>(length);
        }
    }
    return 0;
};
const char *jnivm::GetStringUTFChars(JNIEnv * env, jstring str, jboolean *copy) {
    if (copy)
        *copy = false;
    return str ? JNITypes<std::shared_ptr<String>>::JNICast(ENV::FromJNIEnv(env), str)->data() : "";
};
void jnivm::ReleaseStringUTFChars(JNIEnv * env, jstring str, const char * cstr) {
    // Never copied, never free
};

void jnivm::GetStringRegion(JNIEnv *env, jstring str, jsize start, jsize length, jchar * buf) {
    if(!buf || start < 0 || length <= 0) {
        return;
    }
    try {
        auto cstr = getString(env, str);
        if(!cstr) {
            return;
        }
        auto units = toUnits(*cstr);
        // Java throws StringIndexOutOfBoundsException for a range outside the string; copy what exists.
        for(jsize i = 0; i < length && (size_t)start + i < units.size(); i++) {
            buf[i] = units[start + i];
        }
    } catch(...) {
    }
};

void jnivm::GetStringUTFRegion(JNIEnv *env, jstring str, jsize start, jsize len, char * buf) {
    if(!buf) {
        return;
    }
    char* out = buf;
    try {
        auto cstr = getString(env, str);
        if(cstr && start >= 0 && len > 0) {
            auto units = toUnits(*cstr);
            for(jsize i = 0; i < len && (size_t)start + i < units.size(); i++) {
                jchar c = units[start + i];
                char tmp[3];
                int n = JCharToUTF(c, tmp, sizeof(tmp));
                memcpy(out, tmp, n);
                out += n;
            }
        }
    } catch(...) {
    }
    *out = '\0';
};
