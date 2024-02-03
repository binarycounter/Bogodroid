#ifndef __UNITY_H__
#define __UNITY_H__

#include "baron/baron.h"
#include "android.h"
namespace jnivm
{
    namespace com
    {
        namespace unity3d
        {
            namespace player
            {
                class PlayAssetDeliveryUnityWrapper : public FakeJni::JObject
                {
                public:
                    DEFINE_CLASS_NAME("com/unity3d/player/PlayAssetDeliveryUnityWrapper")
                    bool playCoreApiMissing();
                    static std::shared_ptr<PlayAssetDeliveryUnityWrapper> init(std::shared_ptr<jnivm::android::content::Context> context);
                };
            }

        }
    }
}
#endif