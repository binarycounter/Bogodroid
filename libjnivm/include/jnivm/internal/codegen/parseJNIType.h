#pragma once
#include <string>

namespace jnivm {
    const char *ParseJNIType(const char *cur, const char *end, std::string &type);
}