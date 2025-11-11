#ifndef __OFANDROID_H__
#define __OFANDROID_H__

#include "android.h"
#include "baron/baron.h"
namespace jnivm {
namespace cc {
    namespace openframeworks {
        class OFAndroid : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("cc/openframeworks/OFAndroid")
            static void setupGL(int version, bool preserveOnPause);
        };

    }
}
}
#endif