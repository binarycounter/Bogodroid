#include "ofandroid.h"
#include "../globals.h"
#include "baron/baron.h"
#include "logging.h"
#include <so_util.h>

void jnivm::cc::openframeworks::OFAndroid::setupGL(int version, bool preserveOnPause) {
    verbose("OFAndroid","I should probably set up GLES %d here...",version);
    auto openfw_onSurfaceCreated = (void (*)(void))so_symbol(nullptr, "Java_cc_openframeworks_OFAndroid_onSurfaceCreated");
    openfw_onSurfaceCreated();
}

BEGIN_NATIVE_DESCRIPTOR(jnivm::cc::openframeworks::OFAndroid)
    { FakeJni::Function<&OFAndroid::setupGL> {}, "setupGL", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR