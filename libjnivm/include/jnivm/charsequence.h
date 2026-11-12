#pragma once
#include "object.h"
#include <memory>
#include <string>

namespace jnivm {
    class String;

    class CharSequence : public virtual Object {
    public:
        virtual ~CharSequence() = default;
        virtual std::shared_ptr<String> toString() = 0;
    };
}
