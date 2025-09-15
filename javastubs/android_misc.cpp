#include "toml++/toml.hpp"
extern toml::table config;

#include "android.h"
#include "baron/baron.h"
#include "javac.h"
#include "logging.h"
#include <fstream>
#include <pthread.h>
#include <inttypes.h>

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

///// Misc Descriptors

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::net::Uri) { FakeJni::Constructor<Uri> {} },
    { FakeJni::Function<&Uri::encode> {}, "encode", FakeJni::JMethodID::STATIC },
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