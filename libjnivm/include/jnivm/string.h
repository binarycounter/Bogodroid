#pragma once
#include "charsequence.h"
#include <string>

namespace jnivm {
    class String : public CharSequence, public std::string {
    public:
        String() : std::string() {}
        String(const std::string & str) : std::string(str) {}
        String(std::string && str) : std::string(std::move(str)) {}
        inline std::string asStdString() {
            return *this;
        }
        std::shared_ptr<String> toString() override {
            return std::shared_ptr<String>(shared_from_this(), this);
        }
    };
}