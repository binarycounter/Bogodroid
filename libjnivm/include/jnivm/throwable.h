#pragma once
#include <jnivm/object.h>

namespace jnivm {
    class Throwable : public Object {
    public:
        std::exception_ptr except;
    };
}