#ifndef __TEAPOT_H__
#define __TEAPOT_H__

#include "baron/baron.h"
#include "android.h"
namespace jnivm
{
    namespace com
    {
        namespace sample
        {
            namespace teapot
            {
                class TeapotNativeActivity : public jnivm::android::app::NativeActivity
                {
                public:
                    DEFINE_CLASS_NAME("com/sample/teapot/TeapotNativeActivity")
                    void updateFPS(float val);
                };
            }

        }
    }
}
#endif