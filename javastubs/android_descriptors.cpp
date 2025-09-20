#include "toml++/toml.hpp"
extern toml::table config;

#include "android.h"
#include "baron/baron.h"
#include "javac.h"
#include "logging.h"
#include <fstream>
#include <inttypes.h>
#include <pthread.h>

///// JNI Class Registration

void InitJNIAndroidClasses(FakeJni::Jvm* vm)
{
    verbose("JBRIDGE", "Initializing Android JNI Classes");
    // Net
    vm->registerClass<jnivm::android::net::Uri>();

    // Util
    vm->registerClass<jnivm::android::util::DisplayMetrics>();

    // View
    vm->registerClass<jnivm::android::view::DisplayMode>();
    vm->registerClass<jnivm::android::view::Display>();
    vm->registerClass<jnivm::android::view::Surface>();
    vm->registerClass<jnivm::android::view::Window>();
    vm->registerClass<jnivm::android::view::View>();
    vm->registerClass<jnivm::android::view::SurfaceView>();
    vm->registerClass<jnivm::android::view::Choreographer>();
    vm->registerClass<jnivm::android::view::Choreographer::FrameCallback>();
    vm->registerClass<jnivm::android::view::ContextThemeWrapper>();
    vm->registerClass<jnivm::android::view::MotionRange>();
    vm->registerClass<jnivm::android::view::InputDevice>();
    vm->registerClass<jnivm::android::view::InputEvent>();
    vm->registerClass<jnivm::android::view::KeyEvent>();
    vm->registerClass<jnivm::android::view::MotionEvent>();
    vm->registerClass<jnivm::android::view::KeyCharacterMap>();

    // Hardware
    vm->registerClass<jnivm::android::hardware::display::DisplayManager>();
    vm->registerClass<jnivm::android::hardware::input::InputManager>();
    vm->registerClass<jnivm::android::hardware::input::InputManager::InputDeviceListener>();

    // Media
    vm->registerClass<jnivm::android::media::MediaRouterRouteInfo>();
    vm->registerClass<jnivm::android::media::MediaRouter>();
    vm->registerClass<jnivm::android::media::AudioDeviceInfo>();
    vm->registerClass<jnivm::android::media::AudioManager>();

    // OS
    vm->registerClass<jnivm::android::os::Build>();
    vm->registerClass<jnivm::android::os::BuildVersion>();
    vm->registerClass<jnivm::android::os::Process>();
    vm->registerClass<jnivm::android::os::Bundle>();
    vm->registerClass<jnivm::android::os::Message>();
    vm->registerClass<jnivm::android::os::Looper>();
    vm->registerClass<jnivm::android::os::Handler>();
    vm->registerClass<jnivm::android::os::Handler::Callback>();
    vm->registerClass<jnivm::android::os::HandlerThread>();
    vm->registerClass<jnivm::android::os::Environment>();
    vm->registerClass<jnivm::android::os::PowerManager>();

    // Content
    vm->registerClass<jnivm::android::content::SharedPreferences>();
    vm->registerClass<jnivm::android::content::SharedPreferencesEditor>();
    vm->registerClass<jnivm::android::content::Context>();
    vm->registerClass<jnivm::android::content::Intent>();

    // Content.pm
    vm->registerClass<jnivm::android::content::pm::ActivityInfo>();
    vm->registerClass<jnivm::android::content::pm::PackageInfo>();
    vm->registerClass<jnivm::android::content::pm::ApplicationInfo>();
    vm->registerClass<jnivm::android::content::pm::PackageManager>();

    // Content.res
    vm->registerClass<jnivm::android::content::res::AssetManager>();
    vm->registerClass<jnivm::android::content::res::Resources>();

    // App
    vm->registerClass<jnivm::android::app::Activity>();
    vm->registerClass<jnivm::android::app::NativeActivity>();
}