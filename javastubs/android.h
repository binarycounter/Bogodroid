#ifndef __ANDROID_H__
#define __ANDROID_H__

#include "baron/baron.h"
#include "javac.h"
namespace jnivm
{
    namespace android
    {

        namespace os
        {
            class Process : public FakeJni::JObject
            {
                public:
                    DEFINE_CLASS_NAME("android/os/Process")
                    static void setThreadPriority(int i, int j);
            };
            class Bundle : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/os/Bundle")
                bool containsKey(std::shared_ptr<FakeJni::JString> key);
                std::shared_ptr<FakeJni::JString> getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def);
            };
        }

        namespace content
        {
            namespace res
            {
                class AssetManager : public FakeJni::JObject
                {
                public:
                    DEFINE_CLASS_NAME("android/content/res/AssetManager")
                    std::shared_ptr<jnivm::java::io::InputStream> open(std::shared_ptr<FakeJni::JString> file);
                };
            }
            class Context : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/content/Context")
                static FakeJni::JString LOCATION_SERVICE;
                Context()
                {
                    LOCATION_SERVICE=(FakeJni::JString)"LOCATION_SERVICE";
                }
                std::shared_ptr<FakeJni::JObject> getSystemService(std::shared_ptr<FakeJni::JString> service);
                std::shared_ptr<jnivm::android::content::res::AssetManager> getAssets();
                std::shared_ptr<FakeJni::JString> getPackageCodePath();
                std::shared_ptr<FakeJni::JString> getPackageName();
                std::shared_ptr<jnivm::java::io::File> getFilesDir();
                std::shared_ptr<jnivm::java::io::File> getExternalFilesDir(std::shared_ptr<FakeJni::JString> path);
                std::shared_ptr<jnivm::java::io::File> getObbDir();

                
            };

            class Intent : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/content/Intent")
                std::shared_ptr<jnivm::android::os::Bundle> getExtras();
            };
        }

        namespace app
        {
            class Activity : public jnivm::android::content::Context
            {
            public:
                DEFINE_CLASS_NAME("android/app/Activity")
                std::shared_ptr<jnivm::android::content::Intent> getIntent();
            };
        }

    }
}
#endif