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

            class Environment : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/os/Environment")
                inline static FakeJni::JString MEDIA_MOUNTED = (FakeJni::JString) "MEDIA_MOUNTED";
                static std::shared_ptr<FakeJni::JString> getExternalStorageState();
            };
        }

        namespace content
        {
            namespace pm
            {
                class PackageInfo : public FakeJni::JObject
                {
                public:
                    DEFINE_CLASS_NAME("android/content/pm/PackageInfo")
                    FakeJni::JString versionName = (FakeJni::JString) "0.1";
                };

                class ApplicationInfo : public FakeJni::JObject
                {
                public:
                    DEFINE_CLASS_NAME("android/content/pm/ApplicationInfo")
                    
                    std::shared_ptr<jnivm::Array<FakeJni::JString>> splitPublicSourceDirs=std::make_shared<jnivm::Array<FakeJni::JString>>();
                };

                class PackageManager : public FakeJni::JObject
                {
                public:
                    DEFINE_CLASS_NAME("android/content/pm/PackageManager")
                    std::shared_ptr<PackageInfo> getPackageInfo(std::shared_ptr<FakeJni::JString> packageName, int number);
                };
            }

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
                inline static FakeJni::JString LOCATION_SERVICE = (FakeJni::JString) "LOCATION_SERVICE";
                std::shared_ptr<FakeJni::JObject> getSystemService(std::shared_ptr<FakeJni::JString> service);
                std::shared_ptr<jnivm::android::content::res::AssetManager> getAssets();
                std::shared_ptr<jnivm::android::content::pm::ApplicationInfo> getApplicationInfo();
                std::shared_ptr<FakeJni::JString> getPackageCodePath();
                std::shared_ptr<FakeJni::JString> getPackageName();
                std::shared_ptr<jnivm::android::content::pm::PackageManager> getPackageManager();
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