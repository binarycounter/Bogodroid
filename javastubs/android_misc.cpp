#include "toml++/toml.hpp"
extern toml::table config;

#include "android.h"
#include "baron/baron.h"
#include "javac.h"
#include "logging.h"
#include <fstream>
#include <inttypes.h>
#include <pthread.h>

///// Uri

std::shared_ptr<FakeJni::JString> jnivm::android::net::Uri::encode(std::shared_ptr<FakeJni::JString> string)
{
    return string;
}

///// DisplayManager

std::shared_ptr<jnivm::android::view::Display>
jnivm::android::hardware::display::DisplayManager::getDisplay(int disp)
{
    return std::make_shared<jnivm::android::view::Display>();
}

///// InputManager

std::shared_ptr<jnivm::android::view::InputDevice> jnivm::android::hardware::input::InputManager::getInputDevice(int device)
{
    return jnivm::android::view::InputDevice::getDevice(device);
}

std::shared_ptr<jnivm::Array<int>> jnivm::android::hardware::input::InputManager::getInputDeviceIds()
{
    return jnivm::android::view::InputDevice::getDeviceIds();
}

void jnivm::android::hardware::input::InputManager::registerInputDeviceListener(std::shared_ptr<InputDeviceListener> listener, std::shared_ptr<jnivm::android::os::Handler> handler)
{
}

///// Activity

void jnivm::android::app::Activity::runOnUiThread(std::shared_ptr<jnivm::java::lang::Runnable> runnable)
{
    verbose("JBRIDGE", "RunOnUiThread Running runnable!");
    runnable->run();
}

std::shared_ptr<jnivm::android::content::Intent>
jnivm::android::app::Activity::getIntent()
{
    return std::make_shared<jnivm::android::content::Intent>();
}

int jnivm::android::app::Activity::getRequestedOrientation()
{
    return config["device"]["displayOrientation"].value_or<int>(0);
}

void jnivm::android::app::Activity::setRequestedOrientation(int orientation)
{
    // Stub
}

std::shared_ptr<jnivm::android::content::res::Resources> jnivm::android::app::Activity::getResources()
{
    return std::make_shared<jnivm::android::content::res::Resources>();
}

std::shared_ptr<jnivm::android::view::Window> jnivm::android::app::Activity::getWindow()
{
    return std::make_shared<jnivm::android::view::Window>();
}

std::shared_ptr<jnivm::android::view::View> jnivm::android::app::Activity::findViewById(int id)
{
    return std::make_shared<jnivm::android::view::SurfaceView>(); // Sure, lol
}

///// Settings$Secure

std::shared_ptr<FakeJni::JString> jnivm::android::provider::Settings::Secure::getString(std::shared_ptr<jnivm::android::content::ContentResolver> resolver, std::shared_ptr<FakeJni::JString> key)
{
    if(key == nullptr)
        return nullptr;

    if(key.get()->asStdString() == ANDROID_ID)
        return std::make_shared<FakeJni::JString>("B06015BADC0DE00F");

    verbose("JBRIDGE","Secure.getString() called with unknown key: %s", key.get()->c_str());
    return nullptr;
}


///// Misc Descriptors

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::util::DisplayMetrics) { FakeJni::Constructor<DisplayMetrics> {} },
    { FakeJni::Field<&DisplayMetrics::widthPixels> {}, "widthPixels", FakeJni::JFieldID::PUBLIC },
    { FakeJni::Field<&DisplayMetrics::heightPixels> {}, "heightPixels", FakeJni::JFieldID::PUBLIC },
    { FakeJni::Field<&DisplayMetrics::densityDpi> {}, "densityDpi", FakeJni::JFieldID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::net::Uri) { FakeJni::Constructor<Uri> {} },
    { FakeJni::Function<&Uri::encode> {}, "encode", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::hardware::input::InputManager) { FakeJni::Constructor<InputManager> {} },
    { FakeJni::Function<&InputManager::getInputDeviceIds> {}, "getInputDeviceIds", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&InputManager::getInputDevice> {}, "getInputDevice", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&InputManager::registerInputDeviceListener> {}, "registerInputDeviceListener", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::hardware::input::InputManager::InputDeviceListener) { FakeJni::Constructor<InputDeviceListener> {} },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::hardware::display::DisplayManager) { FakeJni::Constructor<DisplayManager> {} },
    { FakeJni::Function<&DisplayManager::getDisplay> {}, "getDisplay", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::app::Activity) { FakeJni::Constructor<Activity> {} },
    { FakeJni::Function<&Activity::runOnUiThread> {}, "runOnUiThread", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::getIntent> {}, "getIntent", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::getRequestedOrientation> {}, "getRequestedOrientation", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::setRequestedOrientation> {}, "setRequestedOrientation", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::getResources> {}, "getResources", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::getWindow> {}, "getWindow", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Activity::findViewById> {}, "findViewById", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::app::NativeActivity) { FakeJni::Constructor<NativeActivity> {} },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::provider::Settings) { FakeJni::Constructor<Settings> {} },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::provider::Settings::Secure) { FakeJni::Constructor<Secure> {} },
    { FakeJni::Field<&Secure::ANDROID_ID> {}, "ANDROID_ID", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&Secure::getString> {}, "getString", FakeJni::JMethodID::STATIC },
    END_NATIVE_DESCRIPTOR