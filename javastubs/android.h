#ifndef __ANDROID_H__
#define __ANDROID_H__

#include "alooper.h"
#include "baron/baron.h"
#include "javac.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <thread>
#include <vector>

void InitJNIAndroidClasses(FakeJni::Jvm* vm);

namespace jnivm {
namespace android {
    namespace net {
        class Uri : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/net/Uri")
            static std::shared_ptr<FakeJni::JString> encode(std::shared_ptr<FakeJni::JString> string);
        };

    }

    namespace util {
        class DisplayMetrics : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/util/DisplayMetrics")
            int widthPixels = 0;
            int heightPixels = 0;
            int densityDpi = 0;
        };
    }
    namespace view {

        class DisplayMode : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/view/Display$Mode")
            int getPhysicalWidth();
            int getPhysicalHeight();
            float getRefreshRate();
        };

        class Display : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/view/Display")
            int getDisplayId();
            int getRotation();
            int getWidth();
            int getHeight();
            float getRefreshRate();
            long getAppVsyncOffsetNanos();
            long getPresentationDeadlineNanos();
            void getRealMetrics(std::shared_ptr<jnivm::android::util::DisplayMetrics> metrics);
            std::shared_ptr<jnivm::Array<jnivm::android::view::DisplayMode>> getSupportedModes();
        };
        class Surface : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/view/Surface")
        };
        class InputDevice : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/view/InputDevice")
            int getSources();
            static std::shared_ptr<jnivm::android::view::InputDevice> getDevice(int device);
            static std::shared_ptr<FakeJni::JArray<int>> getDeviceIds();
        };

        class View : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/view/View")
            inline static int SYSTEM_UI_FLAG_IMMERSIVE_STICKY = 4096;
            inline static int SYSTEM_UI_FLAG_LAYOUT_STABLE = 256;
            inline static int SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = 1024;
            inline static int SYSTEM_UI_FLAG_HIDE_NAVIGATION = 2;
            inline static int SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 512;
            inline static int SYSTEM_UI_FLAG_FULLSCREEN = 4;
            int getSystemUiVisibility();
            void setSystemUiVisibility(int visibility);
            std::shared_ptr<jnivm::android::view::Display> getDisplay();
        };

        class SurfaceView : public View {
        public:
            DEFINE_CLASS_NAME("android/view/SurfaceView", View)
        };

        class Window : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/view/Window")
            void setFlags(int flag1, int flag2);
            std::shared_ptr<jnivm::android::view::View> getDecorView();
        };
    }
    namespace hardware {
        namespace display {
            class DisplayManager : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("android/hardware/display/DisplayManager")
                std::shared_ptr<jnivm::android::view::Display> getDisplay(int disp);
            };
        }
    }
    namespace media {

        class AudioDeviceInfo : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/media/AudioDeviceInfo")
            inline static int TYPE_BLUETOOTH_A2DP = 8;
            inline static int TYPE_WIRED_HEADPHONES = 4;
            int getType();
        };

        class AudioManager : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/media/AudioManager")
            inline static FakeJni::JString PROPERTY_OUTPUT_FRAMES_PER_BUFFER = (FakeJni::JString) "PROPERTY_OUTPUT_FRAMES_PER_BUFFER";
            inline static FakeJni::JString PROPERTY_OUTPUT_SAMPLE_RATE = (FakeJni::JString) "PROPERTY_OUTPUT_SAMPLE_RATE";
            inline static int GET_DEVICES_OUTPUTS = 2;
            inline static int STREAM_MUSIC = 3;
            bool isBluetoothA2dpOn();
            std::shared_ptr<FakeJni::JString> getProperty(std::shared_ptr<FakeJni::JString> property);
            std::shared_ptr<jnivm::Array<jnivm::android::media::AudioDeviceInfo>> getDevices(int type);
            int getStreamVolume(int stream);
        };

        class MediaRouterRouteInfo : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/media/MediaRouter$RouteInfo")
            std::shared_ptr<jnivm::android::view::Display> getPresentationDisplay();
        };

        class MediaRouter : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/media/MediaRouter")
            inline static int ROUTE_TYPE_LIVE_VIDEO = 2;
            std::shared_ptr<jnivm::android::media::MediaRouterRouteInfo> getSelectedRoute(int type);
        };
    }

    namespace os {

        class Build : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/Build");
            inline static FakeJni::JString MANUFACTURER = (FakeJni::JString) "Allwinner";
            inline static FakeJni::JString MODEL = (FakeJni::JString) "h700";
            inline static FakeJni::JString DEVICE = (FakeJni::JString) "R36S";
            inline static FakeJni::JString ID = (FakeJni::JString) "0.01";
        };

        class BuildVersion : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/Build$VERSION");
            inline static int SDK_INT = 24;
            inline static FakeJni::JString RELEASE = (FakeJni::JString) "Oreo";
            inline static FakeJni::JString INCREMENTAL = (FakeJni::JString) "Bogodroid";
        };

        class Process : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/Process")
            static void setThreadPriority(int i, int j);
        };
        class Bundle : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/Bundle")
            bool containsKey(std::shared_ptr<FakeJni::JString> key);
            std::shared_ptr<FakeJni::JString> getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def);
        };

        class Handler;

        // Helper function to get a monotonic timestamp in milliseconds
        inline long long uptimeMillis()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
        }

        class Message : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/Message")
            // Public fields to mimic the Android API
            int what = 0;
            int arg1 = 0;
            int arg2 = 0;
            std::shared_ptr<java::lang::Object> obj = nullptr;
            std::shared_ptr<java::lang::Runnable> callback = nullptr;

            // Internal fields
            long long when = 0; // The absolute time in uptimeMillis when this message should be handled
            Handler* target = nullptr; // The handler that will process this message
            Message() = default;

        private:
            // Private constructor to enforce pooling

            // Next message in the global pool
            static jnivm::android::os::Message* sPool;
            static pthread_mutex_t sPoolMutex;
            Message* next = nullptr;

        public:
            ~Message() = default;
            Message(const jnivm::android::os::Message&) = delete;
            jnivm::android::os::Message& operator=(const jnivm::android::os::Message&) = delete;

            /**
             * Obtains a new Message from the global pool.
             */
            static std::shared_ptr<jnivm::android::os::Message> obtain();
            void sendToTarget();

            /**
             * Returns a Message to the global pool.
             */
            void recycle();
        };

        struct MessageComparer {
            bool operator()(const std::shared_ptr<jnivm::android::os::Message>& lhs, const std::shared_ptr<jnivm::android::os::Message>& rhs) const
            {
                return lhs->when > rhs->when;
            }
        };

        class Looper : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/Looper")

            ALooper* nativeLooper; // The underlying native looper
            std::priority_queue<std::shared_ptr<jnivm::android::os::Message>,
                std::vector<std::shared_ptr<jnivm::android::os::Message>>,
                MessageComparer>
                mQueue;

            pthread_mutex_t mQueueMutex;
            std::thread mThread; // Manages the dedicated thread, if any
            bool mQuitting;
            // Private constructor to control instantiation
            Looper();
            ~Looper();
            // No copying
            Looper(const jnivm::android::os::Looper&) = delete;
            jnivm::android::os::Looper& operator=(const jnivm::android::os::Looper&) = delete;
            static void prepare();
            static std::shared_ptr<jnivm::android::os::Looper> myLooper();
            static std::shared_ptr<jnivm::android::os::Looper> getMainLooper();
            void loop();
            void quit();
        };

        class Handler : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/Handler")
            class Callback : public virtual FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("android/os/Handler$Callback")
                virtual ~Callback() = default;
                virtual bool handleMessage(std::shared_ptr<jnivm::android::os::Message> msg);
            };

            std::shared_ptr<jnivm::android::os::Looper> mLooper;
            std::shared_ptr<jnivm::android::os::Handler::Callback> mCallback;

            Handler();
            Handler(std::shared_ptr<jnivm::android::os::Looper> looper);
            Handler(std::shared_ptr<jnivm::android::os::Looper> looper, std::shared_ptr<jnivm::android::os::Handler::Callback> callback);
            bool post(std::shared_ptr<java::lang::Runnable> runnable);
            bool sendMessage(std::shared_ptr<jnivm::android::os::Message> message);
            virtual void handleMessage(std::shared_ptr<jnivm::android::os::Message> message);
            bool postDelayed(std::shared_ptr<java::lang::Runnable> runnable, long delayMillis);
            bool sendMessageAtTime(std::shared_ptr<jnivm::android::os::Message> message, long long uptimeMillis);
            std::shared_ptr<Message> obtainMessage(int what);
        };

        class HandlerThread : public java::lang::Thread {
        private:
            std::shared_ptr<Looper> mLooper;
            std::mutex mMutex;
            std::condition_variable mCv;

        public:
            DEFINE_CLASS_NAME("android/os/HandlerThread", java::lang::Thread)
            HandlerThread(std::shared_ptr<FakeJni::JString> name);
            ~HandlerThread();

            // The main logic for the new thread
            void run() override;

            // The synchronized method to get the Looper
            std::shared_ptr<Looper> getLooper();

            // A way to gracefully shut down the thread
            bool quit();
        };

        class Environment : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/Environment")
            inline static FakeJni::JString MEDIA_MOUNTED = (FakeJni::JString) "MEDIA_MOUNTED";
            static std::shared_ptr<FakeJni::JString> getExternalStorageState();
        };

        class PowerManager : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/PowerManager")
            bool isSustainedPerformanceModeSupported();
        };
    }

    namespace content {
        namespace pm {
            class ActivityInfo : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("android/content/pm/ActivityInfo")
                inline static int SCREEN_ORIENTATION_PORTRAIT = 1;
                inline static int SCREEN_ORIENTATION_REVERSE_PORTRAIT = 9;
                inline static int SCREEN_ORIENTATION_REVERSE_LANDSCAPE = 8;
                inline static int SCREEN_ORIENTATION_LANDSCAPE = 0;
                inline static int SCREEN_ORIENTATION_FULL_USER = 13;
                inline static int SCREEN_ORIENTATION_USER_PORTRAIT = 12;
                inline static int SCREEN_ORIENTATION_USER_LANDSCAPE = 11;
                inline static int SCREEN_ORIENTATION_SENSOR = 4;
                inline static int SCREEN_ORIENTATION_UNSPECIFIED = -1;
            };

            class PackageInfo : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("android/content/pm/PackageInfo")
                FakeJni::JString versionName = (FakeJni::JString) "0.1";
            };

            class ApplicationInfo : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("android/content/pm/ApplicationInfo")

                std::shared_ptr<jnivm::Array<FakeJni::JString>> splitPublicSourceDirs = std::make_shared<jnivm::Array<FakeJni::JString>>();
            };

            class PackageManager : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("android/content/pm/PackageManager")
                inline static FakeJni::JString FEATURE_AUDIO_LOW_LATENCY = (FakeJni::JString) "FEATURE_AUDIO_LOW_LATENCY";
                inline static int PERMISSION_GRANTED = 0;
                std::shared_ptr<PackageInfo> getPackageInfo(std::shared_ptr<FakeJni::JString> packageName, int number);
                bool hasSystemFeature(std::shared_ptr<FakeJni::JString> feature);
            };
        }

        namespace res {
            class AssetManager : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("android/content/res/AssetManager")
                std::shared_ptr<jnivm::java::io::InputStream> open(std::shared_ptr<FakeJni::JString> file);
            };

            class Resources : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("android/content/res/Resources")
                int getIdentifier(std::shared_ptr<FakeJni::JString> name, std::shared_ptr<FakeJni::JString> defType, std::shared_ptr<FakeJni::JString> defPackage);
            };
        }

        class SharedPreferencesEditor : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/SharedPreferences$Editor")
            void apply();
            std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> putInt(std::shared_ptr<FakeJni::JString> key, int val);
            std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> putString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> val);
        };

        class SharedPreferences : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/SharedPreferences")
            bool contains(std::shared_ptr<FakeJni::JString> key);
            int getInt(std::shared_ptr<FakeJni::JString> key, int def);
            std::shared_ptr<FakeJni::JString> getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def);
            std::shared_ptr<jnivm::java::util::Map> getAll();
            std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> edit();
        };

        class Context : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/Context")
            inline static FakeJni::JString LOCATION_SERVICE = (FakeJni::JString) "LOCATION_SERVICE";
            inline static FakeJni::JString DISPLAY_SERVICE = (FakeJni::JString) "DISPLAY_SERVICE";
            inline static FakeJni::JString AUDIO_SERVICE = (FakeJni::JString) "AUDIO_SERVICE";
            inline static FakeJni::JString MEDIA_ROUTER_SERVICE = (FakeJni::JString) "MEDIA_ROUTER_SERVICE";
            inline static FakeJni::JString POWER_SERVICE = (FakeJni::JString) "POWER_SERVICE";

            inline static int MODE_PRIVATE = 0;

            std::shared_ptr<FakeJni::JObject> getSystemService(std::shared_ptr<FakeJni::JString> service);
            std::shared_ptr<jnivm::android::content::res::AssetManager> getAssets();
            std::shared_ptr<jnivm::android::content::pm::ApplicationInfo> getApplicationInfo();
            std::shared_ptr<FakeJni::JString> getPackageCodePath();
            std::shared_ptr<FakeJni::JString> getPackageName();
            std::shared_ptr<jnivm::android::content::pm::PackageManager> getPackageManager();
            std::shared_ptr<jnivm::android::content::SharedPreferences> getSharedPreferences(std::shared_ptr<FakeJni::JString> str, int num);
            std::shared_ptr<jnivm::java::io::File> getFilesDir();
            std::shared_ptr<jnivm::java::io::File> getExternalCacheDir();
            std::shared_ptr<jnivm::java::io::File> getCacheDir();
            std::shared_ptr<jnivm::java::io::File> getExternalFilesDir(std::shared_ptr<FakeJni::JString> path);
            std::shared_ptr<jnivm::java::io::File> getObbDir();
            std::shared_ptr<jnivm::Array<jnivm::java::io::File>> getObbDirs();
            int checkCallingOrSelfPermission(std::shared_ptr<FakeJni::JString> permission);
            std::shared_ptr<jnivm::android::content::res::Resources> getResources();
            std::shared_ptr<jnivm::android::view::Window> getWindow();
        };

        class Intent : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/Intent")
            std::shared_ptr<jnivm::android::os::Bundle> getExtras();
        };
    }

    namespace app {
        class Activity : public jnivm::android::content::Context {
        public:
            DEFINE_CLASS_NAME("android/app/Activity")
            void runOnUiThread(std::shared_ptr<jnivm::java::lang::Runnable> runnable);
            std::shared_ptr<jnivm::android::content::Intent> getIntent();
            int getRequestedOrientation();
            void setRequestedOrientation(int orientation);
            std::shared_ptr<jnivm::android::content::res::Resources> getResources();
            std::shared_ptr<jnivm::android::view::Window> getWindow();
            std::shared_ptr<jnivm::android::view::View> findViewById(int id);
        };

        class NativeActivity : public jnivm::android::app::Activity {
        public:
            DEFINE_CLASS_NAME("android/app/NativeActivity")
        };
    }

}
}

namespace jnivm::android::view {
class ContextThemeWrapper : public FakeJni::JObject {
public:
    DEFINE_CLASS_NAME("android/view/ContextThemeWrapper")
    std::shared_ptr<jnivm::android::content::res::Resources> getResources();
};

class Choreographer : public FakeJni::JObject {
public:
    DEFINE_CLASS_NAME("android/view/Choreographer")

    class FrameCallback : public virtual FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("android/view/Choreographer$FrameCallback")
        virtual ~FrameCallback() = default;

        virtual void doFrame(jlong frameTimeNanos) = 0;
    };
    Choreographer();

private:
    std::shared_ptr<os::HandlerThread> mHandlerThread;
    std::shared_ptr<os::Looper> mLooper;
    std::shared_ptr<os::Handler> mHandler;

    std::vector<std::shared_ptr<FrameCallback>> mCallbacks;
    pthread_mutex_t mCallbacksMutex;

    std::atomic<long long> mLastVSyncTimeNanos;

    void watchdogLoop();

    void dispatchFrameCallbacks(bool isRealVSync);

public:
    static std::shared_ptr<Choreographer> getInstance();
    ~Choreographer();

    void postFrameCallback(std::shared_ptr<FrameCallback> callback);
    void signalVSync(); // The public method for eglSwapBuffers
};
}
#endif