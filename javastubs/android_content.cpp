#include "toml++/toml.hpp"
extern toml::table config;

#include "android.h"
#include "android_extra.h"
#include "apk.h"
#include "manifest.h"
#include "baron/baron.h"
#include "javac.h"
#include "logging.h"
#include <fstream>
#include <inttypes.h>
#include <pthread.h>
#include <filesystem>

///// PackageManager

std::shared_ptr<jnivm::android::content::pm::PackageInfo>
jnivm::android::content::pm::PackageManager::getPackageInfo(std::shared_ptr<FakeJni::JString> packageName, int number)
{
    auto info = std::make_shared<jnivm::android::content::pm::PackageInfo>();
    // [package] versionName/versionCode in the config win; otherwise the APK's AndroidManifest.xml; otherwise "0.1".
    info->versionName = (FakeJni::JString) bogo_package_version_name("0.1").c_str();
    info->versionCode = bogo_package_version_code();
    return info;
}

bool jnivm::android::content::pm::PackageManager::hasSystemFeature(std::shared_ptr<FakeJni::JString> feature)
{
    if (feature == nullptr)
        return false;
    verbose("JBRIDGE", "App asks about availability of feature: %s", feature.get()->c_str());
    // Nothing is claimed by default. [device] features = ["android.hardware.gamepad", ...] opts features in.
    if (auto* list = config["device"]["features"].as_array()) {
        for (auto& item : *list)
            if (auto name = item.value<std::string>())
                if (*name == feature.get()->c_str())
                    return true;
    }
    return false;
}

///// AssetManager

std::shared_ptr<jnivm::java::io::InputStream>
jnivm::android::content::res::AssetManager::open(std::shared_ptr<FakeJni::JString> filename)
{
    verbose("JBRIDGE", "AssetManager opening file %s", filename.get()->c_str());
    // Android throws FileNotFoundException for a missing asset; apps probe for optional ones this way
    // (e.g. Unity 7's "pram-shadow-files") and carry on after catching it.
    std::string path = std::string("assets/").append(filename.get()->c_str());
    if (!std::filesystem::is_regular_file(path)) {
        // Not extracted: read it straight from the APK when one is configured.
        std::string data;
        if (ApkArchive* apk = bogo_apk())
            if (apk->read(path, data))
                return std::make_shared<jnivm::java::io::InputStream>(path, std::move(data));
        throw std::runtime_error("java.io.FileNotFoundException: " + std::string(filename.get()->c_str()));
    }
    return std::make_shared<jnivm::java::io::InputStream>(std::make_shared<FakeJni::JString>(std::string("assets/").append(filename.get()->c_str())));
}

std::shared_ptr<jnivm::Array<FakeJni::JString>>
jnivm::android::content::res::AssetManager::list(std::shared_ptr<FakeJni::JString> path)
{
    const std::string relPath = path ? path->c_str() : "";
    verbose("JBRIDGE", "AssetManager listing files in '%s'", relPath.c_str());

    // Construct the actual filesystem path
    std::filesystem::path p = std::filesystem::path("assets") / relPath;

    try {
        if (!std::filesystem::exists(p) || !std::filesystem::is_directory(p)) {
            // Not extracted: list the APK's folder instead (files and subfolders, like Android).
            std::vector<std::string> names;
            if (ApkArchive* apk = bogo_apk())
                names = apk->list(p.string(), true);
            auto result = std::make_shared<jnivm::Array<jnivm::java::lang::String>>(names.size());
            for (size_t i = 0; i < names.size(); i++)
                (*result)[i] = std::make_shared<jnivm::java::lang::String>(names[i]);
            return result;
        }

        std::vector<std::string> entries;

        for (const auto& entry : std::filesystem::directory_iterator(p)) {
            // ANDROID SPEC: Only return the child name; no prefix
            std::string name = entry.path().filename().string();
            entries.push_back(name);

            verbose("JBRIDGE", "AssetManager found entry '%s'", name.c_str());
        }

        // Convert vector<string> to JNI-style Array<JString>
        auto result = std::make_shared<jnivm::Array<jnivm::java::lang::String>>(entries.size());

        for (size_t i = 0; i < entries.size(); i++) {
            (*result)[i] = std::make_shared<jnivm::java::lang::String>(entries[i]);
        }

        return result;
    }
    catch (const std::filesystem::filesystem_error& e) {
        verbose("JBRIDGE", "Error listing files in '%s': %s", relPath.c_str(), e.what());
        return std::make_shared<jnivm::Array<jnivm::java::lang::String>>(0);
    }
}


///// Resources

int jnivm::android::content::res::Resources::getIdentifier(std::shared_ptr<FakeJni::JString> name, std::shared_ptr<FakeJni::JString> defType, std::shared_ptr<FakeJni::JString> defPackage)
{
    verbose("JBRIDGE", "Resources requesting identifier for %s - %s - %s", name.get()->c_str(), defType.get()->c_str(), defPackage.get()->c_str());
    return 1337420;
}

///// SharedPreferences

namespace {
std::string pref_escape(const std::string& in)
{
    std::string out;
    for (char c : in) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default: out += c;
        }
    }
    return out;
}

std::string pref_unescape(const std::string& in)
{
    std::string out;
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '\\' && i + 1 < in.size()) {
            char n = in[++i];
            out += (n == 't') ? '\t' : (n == 'n') ? '\n' : (n == 'r') ? '\r' : n;
        } else {
            out += in[i];
        }
    }
    return out;
}

std::string pref_key(const std::shared_ptr<FakeJni::JString>& key)
{
    return key ? std::string(key->c_str()) : std::string();
}
} // namespace

// File format: one value per line, "<type>\t<key>\t<value>", type = i(nt) l(ong) f(loat) b(ool) s(tring).
void jnivm::android::content::PreferencesStore::load()
{
    std::lock_guard<std::mutex> lock(mtx);
    values.clear();
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        size_t t1 = line.find('\t');
        size_t t2 = (t1 == std::string::npos) ? t1 : line.find('\t', t1 + 1);
        if (t2 == std::string::npos || t1 != 1)
            continue;
        PrefValue v;
        std::string key = pref_unescape(line.substr(2, t2 - 2));
        std::string val = pref_unescape(line.substr(t2 + 1));
        switch (line[0]) {
        case 'i': v.type = PrefValue::INT; v.i = atoll(val.c_str()); break;
        case 'l': v.type = PrefValue::LONG; v.i = atoll(val.c_str()); break;
        case 'b': v.type = PrefValue::BOOL; v.i = (val == "1"); break;
        case 'f': v.type = PrefValue::FLOAT; v.f = atof(val.c_str()); break;
        case 's': v.type = PrefValue::STRING; v.s = val; break;
        default: continue;
        }
        values[key] = v;
    }
}

void jnivm::android::content::PreferencesStore::save()
{
    std::lock_guard<std::mutex> lock(mtx);
    if (path.empty())
        return;
    try {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    } catch (...) {
    }
    // Write to a temp file and rename, so a crash/power loss can't leave a half-written file.
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out)
            return;
        for (auto& kv : values) {
            const PrefValue& v = kv.second;
            char buf[64];
            switch (v.type) {
            case PrefValue::INT: out << "i\t" << pref_escape(kv.first) << '\t' << v.i << '\n'; break;
            case PrefValue::LONG: out << "l\t" << pref_escape(kv.first) << '\t' << v.i << '\n'; break;
            case PrefValue::BOOL: out << "b\t" << pref_escape(kv.first) << '\t' << (v.i ? 1 : 0) << '\n'; break;
            case PrefValue::FLOAT:
                snprintf(buf, sizeof(buf), "%.9g", v.f);
                out << "f\t" << pref_escape(kv.first) << '\t' << buf << '\n';
                break;
            case PrefValue::STRING: out << "s\t" << pref_escape(kv.first) << '\t' << pref_escape(v.s) << '\n'; break;
            }
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
}

bool jnivm::android::content::SharedPreferences::contains(std::shared_ptr<FakeJni::JString> key)
{
    if (!store)
        return false;
    std::lock_guard<std::mutex> lock(store->mtx);
    return store->values.count(pref_key(key)) > 0;
}

int jnivm::android::content::SharedPreferences::getInt(std::shared_ptr<FakeJni::JString> key, int def)
{
    verbose("JBRIDGE", "getInt(%s, %d)", pref_key(key).c_str(), def);
    if (!store)
        return def;
    std::lock_guard<std::mutex> lock(store->mtx);
    auto it = store->values.find(pref_key(key));
    return (it != store->values.end() && it->second.type == PrefValue::INT) ? (int)it->second.i : def;
}

jlong jnivm::android::content::SharedPreferences::getLong(std::shared_ptr<FakeJni::JString> key, jlong def)
{
    if (!store)
        return def;
    std::lock_guard<std::mutex> lock(store->mtx);
    auto it = store->values.find(pref_key(key));
    return (it != store->values.end() && (it->second.type == PrefValue::LONG || it->second.type == PrefValue::INT)) ? (jlong)it->second.i : def;
}

float jnivm::android::content::SharedPreferences::getFloat(std::shared_ptr<FakeJni::JString> key, float def)
{
    verbose("JBRIDGE", "getFloat(%s, %f)", pref_key(key).c_str(), def);
    if (!store)
        return def;
    std::lock_guard<std::mutex> lock(store->mtx);
    auto it = store->values.find(pref_key(key));
    return (it != store->values.end() && it->second.type == PrefValue::FLOAT) ? (float)it->second.f : def;
}

bool jnivm::android::content::SharedPreferences::getBoolean(std::shared_ptr<FakeJni::JString> key, bool def)
{
    if (!store)
        return def;
    std::lock_guard<std::mutex> lock(store->mtx);
    auto it = store->values.find(pref_key(key));
    return (it != store->values.end() && it->second.type == PrefValue::BOOL) ? (it->second.i != 0) : def;
}

std::shared_ptr<FakeJni::JString> jnivm::android::content::SharedPreferences::getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def)
{
    verbose("JBRIDGE", "getString(%s)", pref_key(key).c_str());
    if (!store || !key)
        return def;
    std::lock_guard<std::mutex> lock(store->mtx);
    auto it = store->values.find(pref_key(key));
    if (it != store->values.end() && it->second.type == PrefValue::STRING)
        return std::make_shared<FakeJni::JString>(it->second.s);
    return def;
}

std::shared_ptr<jnivm::java::util::Map> jnivm::android::content::SharedPreferences::getAll()
{
    return std::make_shared<jnivm::java::util::Map>();
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferences::edit()
{
    auto editor = std::make_shared<SharedPreferencesEditor>();
    editor->store = store;
    return editor;
}

///// SharedPreferencesEditor

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::self()
{
    // Editor calls are chained (edit().putInt(..).apply()), so the same object has to be handed back,
    // not a second shared_ptr that thinks it owns it.
    return std::dynamic_pointer_cast<SharedPreferencesEditor>(shared_from_this());
}

void jnivm::android::content::SharedPreferencesEditor::apply()
{
    if (!store)
        return;
    for (auto& op : ops) // each op takes the store lock itself
        op(*store);
    ops.clear();
    store->save();
}

bool jnivm::android::content::SharedPreferencesEditor::commit()
{
    apply();
    return true;
}

#define PREF_OP(body) \
    ops.push_back([=](PreferencesStore& s) { std::lock_guard<std::mutex> lock(s.mtx); body; })

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putInt(std::shared_ptr<FakeJni::JString> key, int val)
{
    verbose("JBRIDGE", "putInt(%s, %d)", pref_key(key).c_str(), val);
    std::string k = pref_key(key);
    PREF_OP({ PrefValue v; v.type = PrefValue::INT; v.i = val; s.values[k] = v; });
    return self();
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putLong(std::shared_ptr<FakeJni::JString> key, jlong val)
{
    std::string k = pref_key(key);
    PREF_OP({ PrefValue v; v.type = PrefValue::LONG; v.i = val; s.values[k] = v; });
    return self();
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putFloat(std::shared_ptr<FakeJni::JString> key, float val)
{
    std::string k = pref_key(key);
    PREF_OP({ PrefValue v; v.type = PrefValue::FLOAT; v.f = val; s.values[k] = v; });
    return self();
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putBoolean(std::shared_ptr<FakeJni::JString> key, bool val)
{
    std::string k = pref_key(key);
    PREF_OP({ PrefValue v; v.type = PrefValue::BOOL; v.i = val ? 1 : 0; s.values[k] = v; });
    return self();
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> val)
{
    std::string k = pref_key(key);
    if (!val) { // putString(key, null) removes the key
        PREF_OP({ s.values.erase(k); });
    } else {
        std::string str = val->c_str();
        PREF_OP({ PrefValue v; v.type = PrefValue::STRING; v.s = str; s.values[k] = v; });
    }
    return self();
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::remove(std::shared_ptr<FakeJni::JString> key)
{
    std::string k = pref_key(key);
    PREF_OP({ s.values.erase(k); });
    return self();
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::clear()
{
    PREF_OP({ s.values.clear(); });
    return self();
}

#undef PREF_OP

///// Context

std::shared_ptr<jnivm::android::content::res::AssetManager>
jnivm::android::content::Context::getAssets()
{
    return std::make_shared<jnivm::android::content::res::AssetManager>();
}

std::shared_ptr<jnivm::android::content::pm::ApplicationInfo>
jnivm::android::content::Context::getApplicationInfo()
{
    auto info = std::make_shared<jnivm::android::content::pm::ApplicationInfo>();
    info->minSdkVersion = bogo_min_sdk_version();
    info->targetSdkVersion = bogo_target_sdk_version();
    return info;
}

std::shared_ptr<FakeJni::JObject>
jnivm::android::content::Context::getSystemService(std::shared_ptr<FakeJni::JString> service)
{
    if (service == nullptr)
        return nullptr;

    if (*service == LOCATION_SERVICE)
        return std::make_shared<jnivm::android::location::LocationManager>(); // reports no providers

    if (*service == AUDIO_SERVICE)
        return std::make_shared<jnivm::android::media::AudioManager>();

    if (*service == DISPLAY_SERVICE)
        return std::make_shared<jnivm::android::hardware::display::DisplayManager>();

    if (*service == POWER_SERVICE)
        return std::make_shared<jnivm::android::os::PowerManager>();

    if (*service == MEDIA_ROUTER_SERVICE)
        return std::make_shared<jnivm::android::media::MediaRouter>();

    if (*service == INPUT_SERVICE)
        return std::make_shared<jnivm::android::hardware::input::InputManager>();

    if (*service == VIBRATOR_SERVICE)
        return std::make_shared<jnivm::android::os::Vibrator>();

    if (*service == SENSOR_SERVICE)
        return std::make_shared<jnivm::android::hardware::SensorManager>();

    verbose("JBRIDGE", "App requesting unknown system service %s", service.get()->c_str());

    return nullptr;
}

std::shared_ptr<FakeJni::JString>
jnivm::android::content::Context::getPackageName()
{
    std::string name = config["package"]["packageName"].value_or<std::string>("");
    if (name.empty())
        name = bogo_manifest().packageName;
    return std::make_shared<FakeJni::JString>(name.empty() ? "package.name.not.defined" : name);
}

std::shared_ptr<jnivm::android::content::pm::PackageManager>
jnivm::android::content::Context::getPackageManager()
{
    return std::make_shared<jnivm::android::content::pm::PackageManager>();
}

std::shared_ptr<jnivm::android::content::SharedPreferences>
jnivm::android::content::Context::getSharedPreferences(std::shared_ptr<FakeJni::JString> str, int num)
{
    // One shared store per preferences name, loaded from <android_files>/shared_prefs/<name>.txt on first use.
    static std::mutex storesMutex;
    static std::map<std::string, std::shared_ptr<PreferencesStore>> stores;

    std::string name = str ? std::string(str->c_str()) : std::string("default");
    std::lock_guard<std::mutex> lock(storesMutex);
    auto& store = stores[name];
    if (!store) {
        store = std::make_shared<PreferencesStore>();
        std::string base = config["paths"]["android_files"].value_or<std::string>("");
        if (!base.empty() && base.back() != '/')
            base += '/';
        // Keep file names safe: only [A-Za-z0-9._-], everything else becomes '_'.
        std::string safe = name;
        for (auto& c : safe)
            if (!isalnum((unsigned char)c) && c != '.' && c != '_' && c != '-')
                c = '_';
        store->path = base.empty() ? std::string() : base + "shared_prefs/" + safe + ".txt";
        if (!store->path.empty())
            store->load();
    }
    auto prefs = std::make_shared<jnivm::android::content::SharedPreferences>();
    prefs->store = store;
    return prefs;
}

std::shared_ptr<FakeJni::JString>
jnivm::android::content::Context::getPackageCodePath()
{
    // With [paths] apk the real APK is the package: Unity reads its data (and jar:file:// URLs) from it, as on Android.
    std::string apk = bogo_apk_path();
    if (!apk.empty())
        return std::make_shared<FakeJni::JString>(apk);
    return std::make_shared<FakeJni::JString>(config["paths"]["android_package_code"].value_or<std::string>("./path_not_defined_code"));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getExternalFilesDir(std::shared_ptr<FakeJni::JString> path)
{
    return getExternalFilesDirInternal();
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getExternalFilesDirInternal()
{
    char* resolved_path = realpath(config["paths"]["android_external_files"].value_or<std::string>("./path_not_defined_external").c_str(), NULL);
    if (resolved_path == NULL) {
        return NULL;
    }
    size_t len = strlen(resolved_path);

    // Check if it already has a trailing slash
    if (len > 0 && resolved_path[len - 1] != '/') {
        // Reallocate to add space for slash and null terminator
        char* with_slash = (char*)realloc(resolved_path, len + 2);
        if (with_slash == NULL) {
            free(resolved_path);
            return NULL;
        }
        with_slash[len] = '/';
        with_slash[len + 1] = '\0';
        return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(with_slash));
    }
    return NULL;
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getFilesDir()
{
    char* resolved_path = realpath(config["paths"]["android_files"].value_or<std::string>("./path_not_defined").c_str(), NULL);
    if (resolved_path == NULL) {
        return NULL;
    }
    size_t len = strlen(resolved_path);

    // Check if it already has a trailing slash
    if (len > 0 && resolved_path[len - 1] != '/') {
        // Reallocate to add space for slash and null terminator
        char* with_slash = (char*)realloc(resolved_path, len + 2);
        if (with_slash == NULL) {
            free(resolved_path);
            return NULL;
        }
        with_slash[len] = '/';
        with_slash[len + 1] = '\0';
        return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(with_slash));
    }
    return NULL;
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getCacheDir()
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(config["paths"]["android_cache"].value_or<std::string>("./path_not_defined_cache")));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getExternalCacheDir()
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(config["paths"]["android_cache"].value_or<std::string>("./path_not_defined_cache")));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getObbDir()
{
    // return std::make_shared<jnivm::java::io::File>(config["paths"]["obb_dir"][0].value_or<std::string>("/path_not_defined_obb"));
    return NULL;
}

std::shared_ptr<jnivm::Array<jnivm::java::io::File>>
jnivm::android::content::Context::getObbDirs()
{
    return NULL;
}

int jnivm::android::content::Context::checkCallingOrSelfPermission(std::shared_ptr<FakeJni::JString> permission)
{
    verbose("JBRIDGE", "Granting permission: %s", permission.get()->c_str());
    return jnivm::android::content::pm::PackageManager::PERMISSION_GRANTED; // Sure why not, what could go wrong....
}

std::shared_ptr<jnivm::android::content::res::Resources> jnivm::android::content::Context::getResources()
{
    return std::make_shared<jnivm::android::content::res::Resources>();
}

std::shared_ptr<jnivm::android::view::Window> jnivm::android::content::Context::getWindow()
{
    return std::make_shared<jnivm::android::view::Window>();
}

std::shared_ptr<jnivm::android::content::ContentResolver> jnivm::android::content::Context::getContentResolver()
{
    return std::make_shared<jnivm::android::content::ContentResolver>();
}

///// Intent

std::shared_ptr<jnivm::android::os::Bundle>
jnivm::android::content::Intent::getExtras()
{
    return std::make_shared<jnivm::android::os::Bundle>();
}

///// Content Descriptors

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::ActivityInfo) { FakeJni::Constructor<ActivityInfo> {} },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_PORTRAIT> {}, "SCREEN_ORIENTATION_PORTRAIT", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_REVERSE_PORTRAIT> {}, "SCREEN_ORIENTATION_REVERSE_PORTRAIT", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_REVERSE_LANDSCAPE> {}, "SCREEN_ORIENTATION_REVERSE_LANDSCAPE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_LANDSCAPE> {}, "SCREEN_ORIENTATION_LANDSCAPE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_FULL_USER> {}, "SCREEN_ORIENTATION_FULL_USER", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_USER_PORTRAIT> {}, "SCREEN_ORIENTATION_USER_PORTRAIT", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_USER_LANDSCAPE> {}, "SCREEN_ORIENTATION_USER_LANDSCAPE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_SENSOR> {}, "SCREEN_ORIENTATION_SENSOR", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&ActivityInfo::SCREEN_ORIENTATION_UNSPECIFIED> {}, "SCREEN_ORIENTATION_UNSPECIFIED", FakeJni::JFieldID::STATIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::PackageInfo) { FakeJni::Constructor<PackageInfo> {} },
    { FakeJni::Field<&PackageInfo::versionName> {}, "versionName", FakeJni::JFieldID::PUBLIC },
    { FakeJni::Field<&PackageInfo::versionCode> {}, "versionCode", FakeJni::JFieldID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::ApplicationInfo) { FakeJni::Constructor<ApplicationInfo> {} },
    { FakeJni::Field<&ApplicationInfo::splitPublicSourceDirs> {}, "splitPublicSourceDirs", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Field<&ApplicationInfo::minSdkVersion> {}, "minSdkVersion", FakeJni::JFieldID::PUBLIC },
    { FakeJni::Field<&ApplicationInfo::targetSdkVersion> {}, "targetSdkVersion", FakeJni::JFieldID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::PackageManager) { FakeJni::Constructor<PackageManager> {} },
    { FakeJni::Field<&PackageManager::FEATURE_AUDIO_LOW_LATENCY> {}, "FEATURE_AUDIO_LOW_LATENCY", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&PackageManager::PERMISSION_GRANTED> {}, "PERMISSION_GRANTED", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&PackageManager::getPackageInfo> {}, "getPackageInfo", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&PackageManager::hasSystemFeature> {}, "hasSystemFeature", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&PackageManager::getInstallerPackageName> {}, "getInstallerPackageName", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::res::AssetManager) { FakeJni::Constructor<AssetManager> {} },
    { FakeJni::Function<&AssetManager::open> {}, "open", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&AssetManager::list> {}, "list", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::res::Resources) { FakeJni::Constructor<Resources> {} },
    { FakeJni::Function<&Resources::getConfiguration> {}, "getConfiguration", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Resources::getIdentifier> {}, "getIdentifier", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::SharedPreferencesEditor) { FakeJni::Constructor<SharedPreferencesEditor> {} },
    { FakeJni::Function<&SharedPreferencesEditor::apply> {}, "apply", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::commit> {}, "commit", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::putInt> {}, "putInt", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::putLong> {}, "putLong", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::putFloat> {}, "putFloat", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::putBoolean> {}, "putBoolean", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::putString> {}, "putString", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::remove> {}, "remove", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::clear> {}, "clear", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::SharedPreferences) { FakeJni::Constructor<SharedPreferences> {} },
    { FakeJni::Function<&SharedPreferences::contains> {}, "contains", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getInt> {}, "getInt", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getLong> {}, "getLong", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getFloat> {}, "getFloat", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getBoolean> {}, "getBoolean", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getString> {}, "getString", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getAll> {}, "getAll", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::edit> {}, "edit", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::ContentResolver) { FakeJni::Constructor<ContentResolver> {} },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::Context) { FakeJni::Constructor<Context> {} },
    { FakeJni::Field<&Context::LOCATION_SERVICE> {}, "LOCATION_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::VIBRATOR_SERVICE> {}, "VIBRATOR_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::SENSOR_SERVICE> {}, "SENSOR_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&Context::registerReceiver> {}, "registerReceiver", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::startActivity> {}, "startActivity", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Field<&Context::DISPLAY_SERVICE> {}, "DISPLAY_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::AUDIO_SERVICE> {}, "AUDIO_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::MEDIA_ROUTER_SERVICE> {}, "MEDIA_ROUTER_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::POWER_SERVICE> {}, "POWER_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::INPUT_SERVICE> {}, "INPUT_SERVICE", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Context::MODE_PRIVATE> {}, "MODE_PRIVATE", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&Context::getSystemService> {}, "getSystemService", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getAssets> {}, "getAssets", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getApplicationInfo> {}, "getApplicationInfo", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getPackageName> {}, "getPackageName", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getPackageManager> {}, "getPackageManager", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getPackageCodePath> {}, "getPackageCodePath", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getSharedPreferences> {}, "getSharedPreferences", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getExternalFilesDir> {}, "getExternalFilesDir", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getFilesDir> {}, "getFilesDir", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getCacheDir> {}, "getCacheDir", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getExternalCacheDir> {}, "getExternalCacheDir", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getObbDir> {}, "getObbDir", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getObbDirs> {}, "getObbDirs", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::checkCallingOrSelfPermission> {}, "checkCallingOrSelfPermission", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getResources> {}, "getResources", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getWindow> {}, "getWindow", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Context::getContentResolver> {}, "getContentResolver", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::Intent) { FakeJni::Constructor<Intent> {} },
    { FakeJni::Constructor<Intent, std::shared_ptr<FakeJni::JString>> {} },
    { FakeJni::Field<&Intent::ACTION_BATTERY_CHANGED> {}, "ACTION_BATTERY_CHANGED", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&Intent::ACTION_VIEW> {}, "ACTION_VIEW", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&Intent::getExtras> {}, "getExtras", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Intent::getAction> {}, "getAction", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Intent::getIntExtra> {}, "getIntExtra", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&Intent::setData> {}, "setData", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR