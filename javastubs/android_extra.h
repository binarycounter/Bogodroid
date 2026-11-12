// Small Android framework classes Unity's own JNI helpers use at startup (battery, vibration, sensors, configuration,
// OpenURL). Without them those helpers log "Null parameter detected" and give up.
#ifndef __ANDROID_EXTRA_H__
#define __ANDROID_EXTRA_H__

#include "android.h"
#include "javac.h"

void InitJNIAndroidExtraClasses(FakeJni::Jvm* vm);

namespace jnivm {
namespace android {
    namespace os { class LocaleList; }

    // android.Manifest.permission
    class ManifestPermission : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("android/Manifest$permission")
        inline static FakeJni::JString VIBRATE = (FakeJni::JString) "android.permission.VIBRATE";
    };

    namespace content {
        class BroadcastReceiver : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/BroadcastReceiver")
        };

        class IntentFilter : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/IntentFilter")
            std::vector<std::string> actions;
            IntentFilter() {}
            IntentFilter(std::shared_ptr<FakeJni::JString> action);
            void addAction(std::shared_ptr<FakeJni::JString> action);
        };

        namespace res {
            class Configuration : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("android/content/res/Configuration")
                inline static int TOUCHSCREEN_NOTOUCH = 1;
                inline static int UI_MODE_TYPE_NORMAL = 1;
                int touchscreen = 1;  // TOUCHSCREEN_NOTOUCH: a handheld with buttons
                int uiMode = 0x11;    // UI_MODE_TYPE_NORMAL | UI_MODE_NIGHT_NO
                int densityDpi = 160;
                float fontScale = 1.0f;
                int keyboard = 1;     // KEYBOARD_NOKEYS
                int hardKeyboardHidden = 2;
                int orientation = 2;  // landscape
                int colorMode = 0;
                std::shared_ptr<jnivm::android::os::LocaleList> getLocales();
            };
        }
    }

    namespace os {
        class LocaleList : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/LocaleList")
            std::shared_ptr<jnivm::java::util::Locale> get(int index);
            int size();
        };

        class Vibrator : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/os/Vibrator")
            bool hasVibrator();
            void vibrate(long ms);
            void cancel();
        };
    }

    namespace location {
        class Criteria : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/location/Criteria")
            int accuracy = 0, power = 0;
            Criteria() {}
            void setAccuracy(int a) { accuracy = a; }
            void setPowerRequirement(int p) { power = p; }
        };

        class LocationListener : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/location/LocationListener")
        };

        // No GPS: no providers, nothing is ever delivered.
        class LocationManager : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/location/LocationManager")
            std::shared_ptr<jnivm::java::util::List> getProviders(bool enabledOnly);
            std::shared_ptr<jnivm::java::util::List> getProvidersMatching(std::shared_ptr<Criteria> criteria, bool enabledOnly);
            std::shared_ptr<FakeJni::JString> getBestProvider(std::shared_ptr<Criteria> criteria, bool enabledOnly);
            bool isProviderEnabled(std::shared_ptr<FakeJni::JString> provider);
            void removeUpdates(std::shared_ptr<LocationListener> listener);
        };
    }

    namespace hardware {
        class Sensor : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/hardware/Sensor")
            inline static int TYPE_ACCELEROMETER = 1;
            inline static int TYPE_MAGNETIC_FIELD = 2;
            inline static int TYPE_GYROSCOPE = 4;
            inline static int TYPE_LIGHT = 5;
            inline static int TYPE_PROXIMITY = 8;
            inline static int TYPE_GRAVITY = 9;
            inline static int TYPE_LINEAR_ACCELERATION = 10;
            inline static int TYPE_ROTATION_VECTOR = 11;
        };

        // No sensors: getDefaultSensor() returns null, which is what Android does for a missing sensor.
        class SensorManager : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/hardware/SensorManager")
            std::shared_ptr<Sensor> getDefaultSensor(int type);
        };
    }
}

namespace com { namespace unity3d { namespace player {
    // Created at startup for controller light bars; nothing to drive here.
    class ControllerLights : public FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("com/unity3d/player/ControllerLights")
    };
} } }
}

#endif
