#ifndef __LIMBO_H__
#define __LIMBO_H__

#include "baron/baron.h"
#include "android.h"
namespace jnivm
{
    namespace com
    {
        namespace playdead
        {
            namespace limbo
            {
                class LimboActivity : public jnivm::android::app::NativeActivity
                {
                public:
                    DEFINE_CLASS_NAME("com/playdead/limbo/LimboActivity")
                    std::shared_ptr<FakeJni::JString> GetLimboCachedAassetsPath();
                };
            }

        }
    }
}
#endif