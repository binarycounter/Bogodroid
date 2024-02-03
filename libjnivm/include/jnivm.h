#pragma once
#include <string>
#include <stdexcept>

#include "jnivm/javatypes.h"
#include "jnivm/vm.h"
#include "jnivm/env.h"
#include "jnivm/extends.h"

namespace jnivm {
    // for variadic calling convention mismatches
    static const char* GetJMethodIDSignature(jmethodID id) {
        return id ? ((jnivm::Method *)id)->signature.data() : nullptr;
    }
}