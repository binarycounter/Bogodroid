#pragma once
namespace jnivm {
    class Weak;
}
#include <jnivm/object.h>
#include <jnivm/extends.h>
namespace jnivm {
    class Weak : public Extends<Object> {
    public:
        std::weak_ptr<Object> wrapped;
    };

    class Global : public Extends<Object> {
    public:
        std::shared_ptr<Object> wrapped;
    };
}