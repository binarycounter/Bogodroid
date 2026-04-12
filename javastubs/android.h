#ifndef __ANDROID_H__
#define __ANDROID_H__

#include "alooper.h"
#include "baron/baron.h"
#include "javac.h"
#include <atomic>
#include <chrono>
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

        class MotionRange : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/view/InputDevice$MotionRange")

            int axis, source;
            float min, max, flat, fuzz;

            MotionRange(int ax, int src, float mn, float mx, float fl, float fz)
                : axis(ax)
                , source(src)
                , min(mn)
                , max(mx)
                , flat(fl)
                , fuzz(fz)
            {
            }

            int getAxis() { return axis; }
            int getSource() { return source; }
            float getMin() { return min; }
            float getMax() { return max; }
            float getFlat() { return flat; }
            float getFuzz() { return fuzz; }
        };

        class InputDevice : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/view/InputDevice")

            static inline int SOURCE_KEYBOARD = 0x00000101;
            static inline int SOURCE_GAMEPAD = 0x00000401;
            static inline int SOURCE_JOYSTICK = 0x01000010;
            static inline int SOURCE_MOUSE = 0x00002002;
            static inline int SOURCE_TOUCHSCREEN = 0x00001002;

            int id = 0;
            int vendor = 0x045e; // Microsoft
            int product = 0x028e; // Xbox 360 controller
            std::shared_ptr<FakeJni::JString> name = std::make_shared<FakeJni::JString>("Microsoft X-Box 360 pad");
            int source = SOURCE_GAMEPAD; // Gamepad
            std::vector<std::shared_ptr<MotionRange>> motionRanges;

            int getSources();
            int getId();
            int getProductId();
            int getVendorId();
            std::shared_ptr<FakeJni::JString> getName();
            std::shared_ptr<FakeJni::JString> getDescriptorString(); // Actually getDescriptor() but that's already taken
            bool isVirtual();
            std::shared_ptr<java::util::List> getMotionRanges();
            static std::shared_ptr<jnivm::android::view::InputDevice> getDevice(int device);
            static std::shared_ptr<FakeJni::JArray<int>> getDeviceIds();

            void addMotionRange(int axis, int src, float min, float max, float flat, float fuzz); // Helper
        };

        class InputEvent : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/view/InputEvent")
            std::shared_ptr<jnivm::android::view::InputDevice> device;
            long timestamp = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            InputEvent(std::shared_ptr<jnivm::android::view::InputDevice> dev)
                : device(dev)
            {
            }
            int getDeviceId();
            int getSource();
            long getEventTime();
            std::shared_ptr<jnivm::android::view::InputDevice> getDevice();
        };

        class KeyEvent : public InputEvent {
        public:
            DEFINE_CLASS_NAME("android/view/KeyEvent", jnivm::android::view::InputEvent)

            static inline int KEYCODE_UNKNOWN = 0;
            static inline int KEYCODE_SOFT_LEFT = 1;
            static inline int KEYCODE_SOFT_RIGHT = 2;
            static inline int KEYCODE_HOME = 3;
            static inline int KEYCODE_BACK = 4;
            static inline int KEYCODE_CALL = 5;
            static inline int KEYCODE_ENDCALL = 6;
            static inline int KEYCODE_0 = 7;
            static inline int KEYCODE_1 = 8;
            static inline int KEYCODE_2 = 9;
            static inline int KEYCODE_3 = 10;
            static inline int KEYCODE_4 = 11;
            static inline int KEYCODE_5 = 12;
            static inline int KEYCODE_6 = 13;
            static inline int KEYCODE_7 = 14;
            static inline int KEYCODE_8 = 15;
            static inline int KEYCODE_9 = 16;
            static inline int KEYCODE_STAR = 17;
            static inline int KEYCODE_POUND = 18;
            static inline int KEYCODE_DPAD_UP = 19;
            static inline int KEYCODE_DPAD_DOWN = 20;
            static inline int KEYCODE_DPAD_LEFT = 21;
            static inline int KEYCODE_DPAD_RIGHT = 22;
            static inline int KEYCODE_DPAD_CENTER = 23;
            static inline int KEYCODE_VOLUME_UP = 24;
            static inline int KEYCODE_VOLUME_DOWN = 25;
            static inline int KEYCODE_POWER = 26;
            static inline int KEYCODE_CAMERA = 27;
            static inline int KEYCODE_CLEAR = 28;
            static inline int KEYCODE_A = 29;
            static inline int KEYCODE_B = 30;
            static inline int KEYCODE_C = 31;
            static inline int KEYCODE_D = 32;
            static inline int KEYCODE_E = 33;
            static inline int KEYCODE_F = 34;
            static inline int KEYCODE_G = 35;
            static inline int KEYCODE_H = 36;
            static inline int KEYCODE_I = 37;
            static inline int KEYCODE_J = 38;
            static inline int KEYCODE_K = 39;
            static inline int KEYCODE_L = 40;
            static inline int KEYCODE_M = 41;
            static inline int KEYCODE_N = 42;
            static inline int KEYCODE_O = 43;
            static inline int KEYCODE_P = 44;
            static inline int KEYCODE_Q = 45;
            static inline int KEYCODE_R = 46;
            static inline int KEYCODE_S = 47;
            static inline int KEYCODE_T = 48;
            static inline int KEYCODE_U = 49;
            static inline int KEYCODE_V = 50;
            static inline int KEYCODE_W = 51;
            static inline int KEYCODE_X = 52;
            static inline int KEYCODE_Y = 53;
            static inline int KEYCODE_Z = 54;
            static inline int KEYCODE_COMMA = 55;
            static inline int KEYCODE_PERIOD = 56;
            static inline int KEYCODE_ALT_LEFT = 57;
            static inline int KEYCODE_ALT_RIGHT = 58;
            static inline int KEYCODE_SHIFT_LEFT = 59;
            static inline int KEYCODE_SHIFT_RIGHT = 60;
            static inline int KEYCODE_TAB = 61;
            static inline int KEYCODE_SPACE = 62;
            static inline int KEYCODE_SYM = 63;
            static inline int KEYCODE_EXPLORER = 64;
            static inline int KEYCODE_ENVELOPE = 65;
            static inline int KEYCODE_ENTER = 66;
            static inline int KEYCODE_DEL = 67;
            static inline int KEYCODE_GRAVE = 68;
            static inline int KEYCODE_MINUS = 69;
            static inline int KEYCODE_EQUALS = 70;
            static inline int KEYCODE_LEFT_BRACKET = 71;
            static inline int KEYCODE_RIGHT_BRACKET = 72;
            static inline int KEYCODE_BACKSLASH = 73;
            static inline int KEYCODE_SEMICOLON = 74;
            static inline int KEYCODE_APOSTROPHE = 75;
            static inline int KEYCODE_SLASH = 76;
            static inline int KEYCODE_AT = 77;
            static inline int KEYCODE_NUM = 78;
            static inline int KEYCODE_HEADSETHOOK = 79;
            static inline int KEYCODE_FOCUS = 80;
            static inline int KEYCODE_PLUS = 81;
            static inline int KEYCODE_MENU = 82;
            static inline int KEYCODE_NOTIFICATION = 83;
            static inline int KEYCODE_SEARCH = 84;
            static inline int KEYCODE_MEDIA_PLAY_PAUSE = 85;
            static inline int KEYCODE_MEDIA_STOP = 86;
            static inline int KEYCODE_MEDIA_NEXT = 87;
            static inline int KEYCODE_MEDIA_PREVIOUS = 88;
            static inline int KEYCODE_MEDIA_REWIND = 89;
            static inline int KEYCODE_MEDIA_FAST_FORWARD = 90;
            static inline int KEYCODE_MUTE = 91;
            static inline int KEYCODE_PAGE_UP = 92;
            static inline int KEYCODE_PAGE_DOWN = 93;
            static inline int KEYCODE_PICTSYMBOLS = 94;
            static inline int KEYCODE_SWITCH_CHARSET = 95;
            static inline int KEYCODE_BUTTON_A = 96;
            static inline int KEYCODE_BUTTON_B = 97;
            static inline int KEYCODE_BUTTON_C = 98;
            static inline int KEYCODE_BUTTON_X = 99;
            static inline int KEYCODE_BUTTON_Y = 100;
            static inline int KEYCODE_BUTTON_Z = 101;
            static inline int KEYCODE_BUTTON_L1 = 102;
            static inline int KEYCODE_BUTTON_R1 = 103;
            static inline int KEYCODE_BUTTON_L2 = 104;
            static inline int KEYCODE_BUTTON_R2 = 105;
            static inline int KEYCODE_BUTTON_THUMBL = 106;
            static inline int KEYCODE_BUTTON_THUMBR = 107;
            static inline int KEYCODE_BUTTON_START = 108;
            static inline int KEYCODE_BUTTON_SELECT = 109;
            static inline int KEYCODE_BUTTON_MODE = 110;
            static inline int KEYCODE_ESCAPE = 111;
            static inline int KEYCODE_FORWARD_DEL = 112;
            static inline int KEYCODE_CTRL_LEFT = 113;
            static inline int KEYCODE_CTRL_RIGHT = 114;
            static inline int KEYCODE_CAPS_LOCK = 115;
            static inline int KEYCODE_SCROLL_LOCK = 116;
            static inline int KEYCODE_SYSRQ = 120;
            static inline int KEYCODE_BREAK = 121;
            static inline int KEYCODE_MOVE_HOME = 122;
            static inline int KEYCODE_MOVE_END = 123;
            static inline int KEYCODE_INSERT = 124;
            static inline int KEYCODE_F1 = 131;
            static inline int KEYCODE_F2 = 132;
            static inline int KEYCODE_F3 = 133;
            static inline int KEYCODE_F4 = 134;
            static inline int KEYCODE_F5 = 135;
            static inline int KEYCODE_F6 = 136;
            static inline int KEYCODE_F7 = 137;
            static inline int KEYCODE_F8 = 138;
            static inline int KEYCODE_F9 = 139;
            static inline int KEYCODE_F10 = 140;
            static inline int KEYCODE_F11 = 141;
            static inline int KEYCODE_F12 = 142;
            static inline int KEYCODE_NUM_LOCK = 143;
            static inline int KEYCODE_NUMPAD_0 = 144;
            static inline int KEYCODE_NUMPAD_1 = 145;
            static inline int KEYCODE_NUMPAD_2 = 146;
            static inline int KEYCODE_NUMPAD_3 = 147;
            static inline int KEYCODE_NUMPAD_4 = 148;
            static inline int KEYCODE_NUMPAD_5 = 149;
            static inline int KEYCODE_NUMPAD_6 = 150;
            static inline int KEYCODE_NUMPAD_7 = 151;
            static inline int KEYCODE_NUMPAD_8 = 152;
            static inline int KEYCODE_NUMPAD_9 = 153;
            static inline int KEYCODE_NUMPAD_DIVIDE = 154;
            static inline int KEYCODE_NUMPAD_MULTIPLY = 155;
            static inline int KEYCODE_NUMPAD_SUBTRACT = 156;
            static inline int KEYCODE_NUMPAD_ADD = 157;
            static inline int KEYCODE_NUMPAD_DOT = 158;
            static inline int KEYCODE_NUMPAD_COMMA = 159;
            static inline int KEYCODE_NUMPAD_ENTER = 160;
            static inline int KEYCODE_NUMPAD_EQUALS = 161;
            static inline int KEYCODE_NUMPAD_LEFT_PAREN = 162;
            static inline int KEYCODE_NUMPAD_RIGHT_PAREN = 163;

            static inline int MAX_KEYCODE = 164;
            static inline int META_ALT_ON = 2;
            static inline int META_ALT_LEFT_ON = 16;
            static inline int META_ALT_RIGHT_ON = 32;
            static inline int META_SHIFT_ON = 1;
            static inline int META_SHIFT_LEFT_ON = 64;
            static inline int META_SHIFT_RIGHT_ON = 128;
            static inline int META_SYM_ON = 4;
            static inline int FLAG_WOKE_HERE = 1;
            static inline int FLAG_SOFT_KEYBOARD = 2;
            static inline int FLAG_KEEP_TOUCH_MODE = 4;
            static inline int FLAG_FROM_SYSTEM = 8;
            static inline int FLAG_EDITOR_ACTION = 16;
            static inline int FLAG_CANCELED = 32;
            static inline int FLAG_VIRTUAL_HARD_KEY = 64;
            static inline int FLAG_LONG_PRESS = 128;
            static inline int FLAG_CANCELED_LONG_PRESS = 256;
            static inline int FLAG_TRACKING = 512;

            static inline int ACTION_DOWN = 0;
            static inline int ACTION_UP = 1;
            static inline int ACTION_MULTIPLE = 2;

            int action;
            int keyCode;
            int state;
            long timestamp = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            KeyEvent(std::shared_ptr<jnivm::android::view::InputDevice> dev, int act, int code, int st)
                : InputEvent(dev)
                , action(act)
                , keyCode(code)
                , state(st)
            {
            }

            int getKeyCode();
            int getAction();
            int getMetaState();
            long getEventTime();
        };

        class MotionEvent : public InputEvent {
        public:
            DEFINE_CLASS_NAME("android/view/MotionEvent", jnivm::android::view::InputEvent)

            static inline int AXIS_X = 0;
            static inline int AXIS_Y = 1;
            static inline int AXIS_Z = 11;
            static inline int AXIS_RZ = 14;
            static inline int AXIS_LTRIGGER = 17;
            static inline int AXIS_RTRIGGER = 18;
            static inline int AXIS_BRAKE = 23;
            static inline int AXIS_GAS = 22;

            static inline int ACTION_DOWN = 0;
            static inline int ACTION_UP = 1;
            static inline int ACTION_MOVE = 2;
            static inline int ACTION_HOVER_MOVE = 7;
            static inline int ACTION_HOVER_ENTER = 9;
            static inline int ACTION_HOVER_EXIT = 10;
            static inline int ACTION_BUTTON_PRESS = 11;
            static inline int ACTION_BUTTON_RELEASE = 12;

            // Mouse buttons
            static inline int BUTTON_PRIMARY = 1;
            static inline int BUTTON_SECONDARY = 2;
            static inline int BUTTON_TERTIARY = 4; // Middle

            int action;
            float x, y;
            int buttonState = 0;

            std::unordered_map<int, float> axisValues;

            long timestamp = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            MotionEvent(std::shared_ptr<jnivm::android::view::InputDevice> dev, int act, float pX, float pY)
                : InputEvent(dev)
                , action(act)
                , x(pX)
                , y(pY)
            {
            }
            long getEventTime();
            int getPointerCount();
            int getHistorySize();
            int getButtonState();
            int getToolType(int pointerIndex);
            int getAction();
            int getActionMasked();
            float getAxisValue(int axis, int pointerIndex);
            float getX(int pointerIndex);
            float getY(int pointerIndex);

            static std::shared_ptr<FakeJni::JString> axisToString(int axis);
            static std::shared_ptr<MotionEvent> obtain(std::shared_ptr<MotionEvent> other);
        };

        class KeyCharacterMap : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/view/KeyCharacterMap")
            static std::shared_ptr<KeyCharacterMap> load(int deviceId);
            int get(int keyCode, int metaState);
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
            inline static int SDK_INT = 26;
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
            static std::shared_ptr<jnivm::java::io::File> getExternalStorageDirectory();
            static bool isExternalStorageManager();
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
                std::shared_ptr<jnivm::Array<FakeJni::JString>> list(std::shared_ptr<FakeJni::JString> path);
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
            float getFloat(std::shared_ptr<FakeJni::JString> key, float def);
            std::shared_ptr<FakeJni::JString> getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def);
            std::shared_ptr<jnivm::java::util::Map> getAll();
            std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> edit();
        };

        class ContentResolver : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/ContentResolver")
        };

        class Context : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/Context")
            inline static FakeJni::JString LOCATION_SERVICE = (FakeJni::JString) "location";
            inline static FakeJni::JString DISPLAY_SERVICE = (FakeJni::JString) "display";
            inline static FakeJni::JString AUDIO_SERVICE = (FakeJni::JString) "audio";
            inline static FakeJni::JString MEDIA_ROUTER_SERVICE = (FakeJni::JString) "media_router";
            inline static FakeJni::JString POWER_SERVICE = (FakeJni::JString) "power";
            inline static FakeJni::JString INPUT_SERVICE = (FakeJni::JString) "input";

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
            static std::shared_ptr<jnivm::java::io::File> getExternalFilesDirInternal();
            std::shared_ptr<jnivm::java::io::File> getObbDir();
            std::shared_ptr<jnivm::Array<jnivm::java::io::File>> getObbDirs();
            int checkCallingOrSelfPermission(std::shared_ptr<FakeJni::JString> permission);
            std::shared_ptr<jnivm::android::content::res::Resources> getResources();
            std::shared_ptr<jnivm::android::view::Window> getWindow();
            std::shared_ptr<ContentResolver> getContentResolver();
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
            DEFINE_CLASS_NAME("android/app/Activity", jnivm::android::content::Context)
            void runOnUiThread(std::shared_ptr<jnivm::java::lang::Runnable> runnable);
            std::shared_ptr<jnivm::android::content::Intent> getIntent();
            int getRequestedOrientation();
            void setRequestedOrientation(int orientation);
            std::shared_ptr<jnivm::android::content::res::Resources> getResources();
            std::shared_ptr<jnivm::android::view::Window> getWindow();
            std::shared_ptr<jnivm::android::view::View> findViewById(int id);

            // Input
            virtual bool onTouchEvent(std::shared_ptr<android::view::MotionEvent> event) { return false; }
            virtual bool onKeyDown(int keyCode, std::shared_ptr<android::view::KeyEvent> event) { return false; }
            virtual bool onKeyUp(int keyCode, std::shared_ptr<android::view::KeyEvent> event) { return false; }
            virtual bool onGenericMotionEvent(std::shared_ptr<android::view::MotionEvent> event) { return false; }
        };

        class NativeActivity : public jnivm::android::app::Activity {
        public:
            DEFINE_CLASS_NAME("android/app/NativeActivity")
        };

        class DialogInterface : public virtual FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/DialogInterface")
        };

        class DialogInterfaceOnClickListener : public virtual FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/DialogInterface$OnClickListener")
        };

        class DialogInterfaceOnCancelListener : public virtual FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/content/DialogInterface$OnCancelListener")
        };

        class AlertDialog : public jnivm::android::app::DialogInterface {
        public:
            DEFINE_CLASS_NAME("android/app/AlertDialog", jnivm::android::app::DialogInterface)
        };

        
        class AlertDialogBuilder : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/app/AlertDialog$Builder")
            AlertDialogBuilder(std::shared_ptr<jnivm::android::content::Context> context);
            std::shared_ptr<AlertDialogBuilder> setTitle(std::shared_ptr<jnivm::CharSequence> title);
            std::shared_ptr<AlertDialogBuilder> setMessage(std::shared_ptr<jnivm::CharSequence> message);
            std::shared_ptr<AlertDialogBuilder> setPositiveButton(std::shared_ptr<jnivm::CharSequence> text, std::shared_ptr<jnivm::android::app::DialogInterfaceOnClickListener> listener);
            std::shared_ptr<AlertDialogBuilder> setNegativeButton(std::shared_ptr<jnivm::CharSequence> text, std::shared_ptr<jnivm::android::app::DialogInterfaceOnClickListener> listener);
            std::shared_ptr<AlertDialogBuilder> setOnCancelListener(std::shared_ptr<jnivm::android::app::DialogInterfaceOnCancelListener> listener);
            std::shared_ptr<AlertDialogBuilder> setView(std::shared_ptr<jnivm::android::view::View> view);
            std::shared_ptr<jnivm::android::app::AlertDialog> show();
        };
    }

    namespace provider {
        class Settings : public FakeJni::JObject {
        public:
            DEFINE_CLASS_NAME("android/provider/Settings")
            class Secure : public FakeJni::JObject {
            public:
                DEFINE_CLASS_NAME("android/provider/Settings$Secure")
                inline static FakeJni::JString ANDROID_ID = (FakeJni::JString) "android_id"; // B06015BADC0DE00F
                static std::shared_ptr<FakeJni::JString> getString(std::shared_ptr<jnivm::android::content::ContentResolver> resolver, std::shared_ptr<FakeJni::JString> key);
            };
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

namespace jnivm::android::hardware::input {
class InputManager : public FakeJni::JObject {
public:
    DEFINE_CLASS_NAME("android/hardware/input/InputManager")
    class InputDeviceListener : public virtual FakeJni::JObject {
    public:
        DEFINE_CLASS_NAME("android/hardware/input/InputManager$InputDeviceListener")
    };
    std::shared_ptr<jnivm::Array<int>> getInputDeviceIds();
    std::shared_ptr<jnivm::android::view::InputDevice> getInputDevice(int device);
    void registerInputDeviceListener(std::shared_ptr<jnivm::android::hardware::input::InputManager::InputDeviceListener> listener, std::shared_ptr<jnivm::android::os::Handler> handler);
};
}
#endif