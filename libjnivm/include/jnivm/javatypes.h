#pragma once
#include "object.h"
#include "array.h"
#include "string.h"
#include "class.h"
#include "bytebuffer.h"
#include "throwable.h"

namespace jnivm {
    namespace java {
        namespace lang {
            using Array = jnivm::Array<void>;
            using Object = jnivm::Object;
            using String = jnivm::String;
            using Class = jnivm::Class;
            using Throwable = jnivm::Throwable;
            namespace reflect {
                using Method = jnivm::Method;
                using Field = jnivm::Field;
            }
        }
        namespace nio {
            using ByteBuffer = jnivm::ByteBuffer;
        }
    }
    namespace internal {
        namespace lang {
            using Global = jnivm::Global;
            using Weak = jnivm::Weak;
        }
    }
}