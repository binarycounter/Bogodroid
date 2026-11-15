#include "android_extra.h"
#include "logging.h"
#include <dirent.h>
#include <fstream>
#include <string>
#include <ctype.h>
#include <stdlib.h>

using namespace jnivm::android;

///// Battery: the sticky ACTION_BATTERY_CHANGED intent, read from /sys/class/power_supply

namespace {
    std::string readLine(const std::string& path)
    {
        std::ifstream f(path);
        std::string s;
        std::getline(f, s);
        return s;
    }

    struct BatteryState { bool present = false; int level = -1; int status = 1; int plugged = 0; };

    BatteryState readBattery()
    {
        BatteryState b;
        const std::string root = "/sys/class/power_supply/";
        DIR* d = opendir(root.c_str());
        if (!d) return b;
        while (dirent* e = readdir(d)) {
            if (e->d_name[0] == '.') continue;
            std::string dir = root + e->d_name + "/";
            std::string type = readLine(dir + "type");
            if (type == "Battery" && !b.present) {
                b.present = true;
                std::string cap = readLine(dir + "capacity");
                if (!cap.empty()) b.level = atoi(cap.c_str());
                std::string st = readLine(dir + "status");
                // android.os.BatteryManager.BATTERY_STATUS_*
                b.status = st == "Charging" ? 2 : st == "Discharging" ? 3 : st == "Not charging" ? 4 : st == "Full" ? 5 : 1;
            } else if ((type == "Mains" || type == "USB") && readLine(dir + "online") == "1") {
                b.plugged = type == "Mains" ? 1 : 2; // BATTERY_PLUGGED_AC / _USB
            }
        }
        closedir(d);
        return b;
    }
}

jnivm::android::content::Intent::Intent(std::shared_ptr<FakeJni::JString> a)
{
    action = a ? a->c_str() : "";
}

std::shared_ptr<FakeJni::JString> jnivm::android::content::Intent::getAction()
{
    return std::make_shared<FakeJni::JString>(action);
}

int jnivm::android::content::Intent::getIntExtra(std::shared_ptr<FakeJni::JString> name, int defaultValue)
{
    if (!name || action != "android.intent.action.BATTERY_CHANGED")
        return defaultValue;
    BatteryState b = readBattery();
    std::string n = name->c_str();
    if (n == "level") return b.present ? b.level : defaultValue;
    if (n == "scale") return b.present ? 100 : defaultValue;
    if (n == "status") return b.status;
    if (n == "plugged") return b.plugged;
    if (n == "present") return b.present ? 1 : 0;
    return defaultValue;
}

std::shared_ptr<jnivm::android::content::Intent> jnivm::android::content::Intent::setData(std::shared_ptr<jnivm::android::net::Uri> uri)
{
    data = uri ? uri->value : "";
    return std::dynamic_pointer_cast<Intent>(shared_from_this());
}

jnivm::android::content::IntentFilter::IntentFilter(std::shared_ptr<FakeJni::JString> action)
{
    addAction(action);
}

void jnivm::android::content::IntentFilter::addAction(std::shared_ptr<FakeJni::JString> action)
{
    if (action) actions.push_back(action->c_str());
}

std::shared_ptr<jnivm::android::content::Intent> jnivm::android::content::Context::registerReceiver(
    std::shared_ptr<BroadcastReceiver> receiver, std::shared_ptr<IntentFilter> filter)
{
    if (!filter) return nullptr;
    for (auto& a : filter->actions) {
        if (a == "android.intent.action.BATTERY_CHANGED") {
            // Sticky broadcast: registering returns the current battery state.
            return std::make_shared<Intent>(std::make_shared<FakeJni::JString>(a));
        }
    }
    verbose("JBRIDGE", "registerReceiver: no broadcasts are delivered");
    return nullptr;
}

void jnivm::android::content::Context::startActivity(std::shared_ptr<Intent> intent)
{
    // No other apps to hand an intent to (OpenURL, share sheets, ...).
    printf("[JBRIDGE] startActivity(%s %s) ignored\n", intent ? intent->action.c_str() : "null", intent ? intent->data.c_str() : "");
}

///// Uri

std::shared_ptr<jnivm::android::net::Uri> jnivm::android::net::Uri::parse(std::shared_ptr<FakeJni::JString> s)
{
    auto u = std::make_shared<Uri>();
    u->value = s ? s->c_str() : "";
    return u;
}

bool jnivm::android::net::Uri::isRelative()
{
    return value.find(':') == std::string::npos;
}

std::shared_ptr<FakeJni::JString> jnivm::android::net::Uri::toString()
{
    return std::make_shared<FakeJni::JString>(value);
}

///// Configuration / LocaleList

std::shared_ptr<jnivm::android::content::res::Configuration> jnivm::android::content::res::Resources::getConfiguration()
{
    return std::make_shared<Configuration>();
}

std::shared_ptr<jnivm::android::os::LocaleList> jnivm::android::content::res::Configuration::getLocales()
{
    return std::make_shared<jnivm::android::os::LocaleList>();
}

std::shared_ptr<jnivm::java::util::Locale> jnivm::android::os::LocaleList::get(int index)
{
    return index == 0 ? jnivm::java::util::Locale::getDefault() : nullptr;
}

int jnivm::android::os::LocaleList::size() { return 1; }

///// Vibrator / sensors

bool jnivm::android::os::Vibrator::hasVibrator() { return false; }
void jnivm::android::os::Vibrator::vibrate(long) {}
void jnivm::android::os::Vibrator::cancel() {}

std::shared_ptr<jnivm::android::hardware::Sensor> jnivm::android::hardware::SensorManager::getDefaultSensor(int type)
{
    return nullptr;
}

///// Location

std::shared_ptr<jnivm::java::util::List> jnivm::android::location::LocationManager::getProviders(bool)
{
    return std::make_shared<jnivm::java::util::ArrayList>();
}

std::shared_ptr<jnivm::java::util::List> jnivm::android::location::LocationManager::getProvidersMatching(std::shared_ptr<Criteria>, bool)
{
    return std::make_shared<jnivm::java::util::ArrayList>();
}

std::shared_ptr<FakeJni::JString> jnivm::android::location::LocationManager::getBestProvider(std::shared_ptr<Criteria>, bool)
{
    return nullptr;
}

bool jnivm::android::location::LocationManager::isProviderEnabled(std::shared_ptr<FakeJni::JString>) { return false; }
void jnivm::android::location::LocationManager::removeUpdates(std::shared_ptr<LocationListener>) {}

///// Small additions to existing classes

std::shared_ptr<FakeJni::JString> jnivm::android::net::Uri::decode(std::shared_ptr<FakeJni::JString> s)
{
    if (!s) return nullptr;
    std::string in = s->c_str(), out;
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '%' && i + 2 < in.size() && isxdigit((unsigned char)in[i + 1]) && isxdigit((unsigned char)in[i + 2])) {
            out += (char)strtol(in.substr(i + 1, 2).c_str(), nullptr, 16);
            i += 2;
        } else {
            out += in[i];
        }
    }
    return std::make_shared<FakeJni::JString>(out);
}

void jnivm::android::hardware::display::DisplayManager::registerDisplayListener(std::shared_ptr<FakeJni::JObject>, std::shared_ptr<jnivm::android::os::Handler>) {}

std::shared_ptr<FakeJni::JString> jnivm::android::content::pm::PackageManager::getInstallerPackageName(std::shared_ptr<FakeJni::JString>)
{
    return nullptr; // sideloaded, as Android reports it
}

std::shared_ptr<jnivm::android::os::Vibrator> jnivm::android::view::InputDevice::getVibrator()
{
    return std::make_shared<jnivm::android::os::Vibrator>();
}

///// Descriptors

using namespace jnivm::android::content;
using namespace jnivm::android::content::res;
using namespace jnivm::android::os;
using namespace jnivm::android::hardware;
using namespace jnivm::android::location;
using jnivm::com::unity3d::player::ControllerLights;

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::location::Criteria) { FakeJni::Constructor<Criteria> {} },
{ FakeJni::Function<&Criteria::setAccuracy> {}, "setAccuracy", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Criteria::setPowerRequirement> {}, "setPowerRequirement", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::location::LocationListener) { FakeJni::Constructor<LocationListener> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::location::LocationManager) { FakeJni::Constructor<LocationManager> {} },
{ FakeJni::Function<&LocationManager::getProviders> {}, "getProviders", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&LocationManager::getProvidersMatching> {}, "getProviders", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&LocationManager::getBestProvider> {}, "getBestProvider", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&LocationManager::isProviderEnabled> {}, "isProviderEnabled", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&LocationManager::removeUpdates> {}, "removeUpdates", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::com::unity3d::player::ControllerLights) { FakeJni::Constructor<ControllerLights> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::ManifestPermission) { FakeJni::Constructor<ManifestPermission> {} },
{ FakeJni::Field<&ManifestPermission::VIBRATE> {}, "VIBRATE", FakeJni::JFieldID::STATIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::BroadcastReceiver) { FakeJni::Constructor<BroadcastReceiver> {} },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::IntentFilter) { FakeJni::Constructor<IntentFilter> {} },
{ FakeJni::Constructor<IntentFilter, std::shared_ptr<FakeJni::JString>> {} },
{ FakeJni::Function<&IntentFilter::addAction> {}, "addAction", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::res::Configuration) { FakeJni::Constructor<Configuration> {} },
{ FakeJni::Field<&Configuration::touchscreen> {}, "touchscreen", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Configuration::uiMode> {}, "uiMode", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Configuration::densityDpi> {}, "densityDpi", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Configuration::fontScale> {}, "fontScale", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Configuration::keyboard> {}, "keyboard", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Configuration::hardKeyboardHidden> {}, "hardKeyboardHidden", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Configuration::orientation> {}, "orientation", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Configuration::colorMode> {}, "colorMode", FakeJni::JFieldID::PUBLIC },
{ FakeJni::Field<&Configuration::TOUCHSCREEN_NOTOUCH> {}, "TOUCHSCREEN_NOTOUCH", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Configuration::UI_MODE_TYPE_NORMAL> {}, "UI_MODE_TYPE_NORMAL", FakeJni::JFieldID::STATIC },
{ FakeJni::Function<&Configuration::getLocales> {}, "getLocales", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::LocaleList) { FakeJni::Constructor<LocaleList> {} },
{ FakeJni::Function<&LocaleList::get> {}, "get", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&LocaleList::size> {}, "size", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Vibrator) { FakeJni::Constructor<Vibrator> {} },
{ FakeJni::Function<&Vibrator::hasVibrator> {}, "hasVibrator", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Vibrator::vibrate> {}, "vibrate", FakeJni::JMethodID::PUBLIC },
{ FakeJni::Function<&Vibrator::cancel> {}, "cancel", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::hardware::Sensor) { FakeJni::Constructor<Sensor> {} },
{ FakeJni::Field<&Sensor::TYPE_ACCELEROMETER> {}, "TYPE_ACCELEROMETER", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Sensor::TYPE_MAGNETIC_FIELD> {}, "TYPE_MAGNETIC_FIELD", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Sensor::TYPE_GYROSCOPE> {}, "TYPE_GYROSCOPE", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Sensor::TYPE_LIGHT> {}, "TYPE_LIGHT", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Sensor::TYPE_PROXIMITY> {}, "TYPE_PROXIMITY", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Sensor::TYPE_GRAVITY> {}, "TYPE_GRAVITY", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Sensor::TYPE_LINEAR_ACCELERATION> {}, "TYPE_LINEAR_ACCELERATION", FakeJni::JFieldID::STATIC },
{ FakeJni::Field<&Sensor::TYPE_ROTATION_VECTOR> {}, "TYPE_ROTATION_VECTOR", FakeJni::JFieldID::STATIC },
END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::hardware::SensorManager) { FakeJni::Constructor<SensorManager> {} },
{ FakeJni::Function<&SensorManager::getDefaultSensor> {}, "getDefaultSensor", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR

void InitJNIAndroidExtraClasses(FakeJni::Jvm* vm)
{
    vm->registerClass<ManifestPermission>();
    vm->registerClass<BroadcastReceiver>();
    vm->registerClass<IntentFilter>();
    vm->registerClass<Configuration>();
    vm->registerClass<LocaleList>();
    vm->registerClass<Vibrator>();
    vm->registerClass<Sensor>();
    vm->registerClass<SensorManager>();
    vm->registerClass<Criteria>();
    vm->registerClass<LocationListener>();
    vm->registerClass<LocationManager>();
    vm->registerClass<ControllerLights>();
}
