#include "toml++/toml.hpp"
extern toml::table config;

#include "android.h"
#include "baron/baron.h"
#include "javac.h"
#include "logging.h"
#include <fstream>
#include <pthread.h>
#include <inttypes.h>

///// JNI Class Registration

void InitJNIAndroidClasses(FakeJni::Jvm* vm)
{
    verbose("JBRIDGE", "Initializing Android JNI Classes");
    vm->registerClass<jnivm::android::net::Uri>();
    vm->registerClass<jnivm::android::util::DisplayMetrics>();
    vm->registerClass<jnivm::android::view::DisplayMode>();
    vm->registerClass<jnivm::android::view::Display>();
    vm->registerClass<jnivm::android::view::Surface>();
    vm->registerClass<jnivm::android::view::InputDevice>();
    vm->registerClass<jnivm::android::view::Window>();
    vm->registerClass<jnivm::android::view::View>();
    vm->registerClass<jnivm::android::view::SurfaceView>();
    vm->registerClass<jnivm::android::view::Choreographer>();
    vm->registerClass<jnivm::android::view::Choreographer::FrameCallback>();
    vm->registerClass<jnivm::android::view::ContextThemeWrapper>();
    vm->registerClass<jnivm::android::hardware::display::DisplayManager>();
    vm->registerClass<jnivm::android::media::MediaRouterRouteInfo>();
    vm->registerClass<jnivm::android::media::MediaRouter>();
    vm->registerClass<jnivm::android::media::AudioDeviceInfo>();
    vm->registerClass<jnivm::android::media::AudioManager>();
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
    vm->registerClass<jnivm::android::content::pm::ActivityInfo>();
    vm->registerClass<jnivm::android::content::pm::PackageInfo>();
    vm->registerClass<jnivm::android::content::pm::ApplicationInfo>();
    vm->registerClass<jnivm::android::content::pm::PackageManager>();
    vm->registerClass<jnivm::android::content::res::AssetManager>();
    vm->registerClass<jnivm::android::content::res::Resources>();
    vm->registerClass<jnivm::android::content::SharedPreferences>();
    vm->registerClass<jnivm::android::content::SharedPreferencesEditor>();
    vm->registerClass<jnivm::android::content::Context>();
    vm->registerClass<jnivm::android::content::Intent>();
    vm->registerClass<jnivm::android::app::Activity>();
    vm->registerClass<jnivm::android::app::NativeActivity>();
}