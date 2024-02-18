#ifndef __ANDROID_H__
#define __ANDROID_H__

#include "baron/baron.h"
#include "javac.h"
namespace jnivm
{
    namespace android
    {
        namespace view
        {
            class Display : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/view/Display")
                int getDisplayId();
                int getRotation();
                int getWidth();
                int getHeight();
            };
        }
        namespace hardware
        {
            namespace display
            {
                class DisplayManager : public FakeJni::JObject
                {
                public:
                    DEFINE_CLASS_NAME("android/hardware/display/DisplayManager")
                    std::shared_ptr<jnivm::android::view::Display> getDisplay();
                };
            }
        }

        namespace os
        {

            class Build : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/os/Build");
                inline static FakeJni::JString MANUFACTURER = (FakeJni::JString) "Rockchip";
                inline static FakeJni::JString MODEL = (FakeJni::JString) "RK3326";
            };

            class BuildVersion : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/os/Build$VERSION");
                //static const int SDK_INT = 24;
            };

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

            class Looper : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/os/Looper")
                static std::shared_ptr<Looper> getMainLooper();
            };

            class Handler : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/os/Handler")
                Handler(std::shared_ptr<Looper> looper);
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

                    std::shared_ptr<jnivm::Array<FakeJni::JString>> splitPublicSourceDirs = std::make_shared<jnivm::Array<FakeJni::JString>>();
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

            class SharedPreferencesEditor : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/content/SharedPreferences$Editor")
                void apply();
                std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> putInt(std::shared_ptr<FakeJni::JString> key, int val);
                std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> putString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> val);
            };

            class SharedPreferences : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/content/SharedPreferences")
                bool contains(std::shared_ptr<FakeJni::JString> key);
                int getInt(std::shared_ptr<FakeJni::JString> key, int def);
                std::shared_ptr<FakeJni::JString> getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def);
                std::shared_ptr<jnivm::java::util::Map> getAll();
                std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> edit();
            };

            class Context : public FakeJni::JObject
            {
            public:
                DEFINE_CLASS_NAME("android/content/Context")
                inline static FakeJni::JString LOCATION_SERVICE = (FakeJni::JString) "LOCATION_SERVICE";
                inline static FakeJni::JString DISPLAY_SERVICE = (FakeJni::JString) "DISPLAY_SERVICE";
                inline static FakeJni::JString AUDIO_SERVICE = (FakeJni::JString) "AUDIO_SERVICE";

                inline static int MODE_PRIVATE = 0;

                std::shared_ptr<FakeJni::JObject> getSystemService(std::shared_ptr<FakeJni::JString> service);
                std::shared_ptr<jnivm::android::content::res::AssetManager> getAssets();
                std::shared_ptr<jnivm::android::content::pm::ApplicationInfo> getApplicationInfo();
                std::shared_ptr<FakeJni::JString> getPackageCodePath();
                std::shared_ptr<FakeJni::JString> getPackageName();
                std::shared_ptr<jnivm::android::content::pm::PackageManager> getPackageManager();
                std::shared_ptr<jnivm::android::content::SharedPreferences> getSharedPreferences(std::shared_ptr<FakeJni::JString> str, int num);
                std::shared_ptr<jnivm::java::io::File> getFilesDir();
                std::shared_ptr<jnivm::java::io::File> getCacheDir();
                std::shared_ptr<jnivm::java::io::File> getExternalFilesDir(std::shared_ptr<FakeJni::JString> path);
                std::shared_ptr<jnivm::java::io::File> getObbDir();
                std::shared_ptr<jnivm::Array<jnivm::java::io::File>> getObbDirs();
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

            class NativeActivity : public jnivm::android::app::Activity
            {
            public:
                DEFINE_CLASS_NAME("android/app/NativeActivity")
            };
        }

    }
}
#endif