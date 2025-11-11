#include "ofandroid.h"
#include "../globals.h"
#include "baron/baron.h"
#include "logging.h"

void jnivm::cc::openframeworks::OFAndroid::setupGL(int version, bool preserveOnPause) {
}

BEGIN_NATIVE_DESCRIPTOR(jnivm::cc::openframeworks::OFAndroid)
    { FakeJni::Function<&OFAndroid::setupGL> {}, "setupGL", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR