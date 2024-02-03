#pragma once
#include "object.h"
#include <string>

namespace jnivm {
    class String : public Object, public std::string {
    public:
        String() : std::string() {}
        String(const std::string & str) : std::string(std::move(str)) {}
        String(std::string && str) : std::string(std::move(str)) {}
        inline std::string asStdString() {
            return *this;
        }
    };
}