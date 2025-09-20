#ifndef __UNITY_H__
#define __UNITY_H__

#include "android.h"
#include "baron/baron.h"
namespace jnivm {
namespace com {
    namespace unity3d {
        namespace player {
            class PlayAssetDeliveryUnityWrapper : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("com/unity3d/player/PlayAssetDeliveryUnityWrapper")
                bool playCoreApiMissing();
                static std::shared_ptr<PlayAssetDeliveryUnityWrapper> init(std::shared_ptr<jnivm::android::content::Context> context);
            };

            class UnityPlayerActivity : public jnivm::android::app::Activity {
            public:
                DEFINE_CLASS_NAME("com/unity3d/player/UnityPlayerActivity")
                bool injectEvent(std::shared_ptr<android::view::InputEvent> event);
            };
        }

    }
}
}
#endif