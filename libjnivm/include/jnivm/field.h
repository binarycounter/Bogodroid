#pragma once
#include "object.h"
#include <string>

#include "methodhandlebase.h"

namespace jnivm {

    class Field : public Object {
    public:
        std::string name;
        std::string type;
        bool _static = false;
        std::shared_ptr<MethodHandle> getnativehandle;
        std::shared_ptr<MethodHandle> setnativehandle;
#ifdef JNI_DEBUG
        std::string GenerateHeader();
        std::string GenerateStubs(std::string scope, const std::string &cname);
        std::string GenerateJNIBinding(std::string scope, const std::string &cname);
#endif
    };
}