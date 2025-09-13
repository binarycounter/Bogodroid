#include "baron/baron.h"
#include "teapot.h"


void jnivm::com::sample::teapot::TeapotNativeActivity::updateFPS(float val)
{
    printf("FPS: %.6f\n", val);
}

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::sample::teapot::TeapotNativeActivity){FakeJni::Constructor<TeapotNativeActivity>{}},
{FakeJni::Function<&TeapotNativeActivity::updateFPS>{}, "updateFPS", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR


