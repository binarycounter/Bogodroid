#include "toml++/toml.hpp"
extern toml::table config;

#include "android.h"
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
    return std::make_shared<jnivm::android::content::pm::PackageInfo>();
}

bool jnivm::android::content::pm::PackageManager::hasSystemFeature(std::shared_ptr<FakeJni::JString> feature)
{
    verbose("JBRIDGE", "App asks about availability of feature: %s", feature.get()->c_str());
    return false; // We don't claim support of anything right now
}

///// AssetManager

std::shared_ptr<jnivm::java::io::InputStream>
jnivm::android::content::res::AssetManager::open(std::shared_ptr<FakeJni::JString> filename)
{
    verbose("JBRIDGE", "AssetManager opening file %s", filename.get()->c_str());
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
            return std::make_shared<jnivm::Array<jnivm::java::lang::String>>(0);
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
        std::make_shared<jnivm::Array<jnivm::java::lang::String>>(0);
    }
}


///// Resources

int jnivm::android::content::res::Resources::getIdentifier(std::shared_ptr<FakeJni::JString> name, std::shared_ptr<FakeJni::JString> defType, std::shared_ptr<FakeJni::JString> defPackage)
{
    verbose("JBRIDGE", "Resources requesting identifier for %s - %s - %s", name.get()->c_str(), defType.get()->c_str(), defPackage.get()->c_str());
    return 1337420;
}

///// SharedPreferences

bool jnivm::android::content::SharedPreferences::contains(std::shared_ptr<FakeJni::JString> key)
{
    return false;
}

int jnivm::android::content::SharedPreferences::getInt(std::shared_ptr<FakeJni::JString> key, int def)
{
    verbose("JBRIDGE", "getInt(%s, %d)", key.get()->c_str(), def);
    return def;
}

float jnivm::android::content::SharedPreferences::getFloat(std::shared_ptr<FakeJni::JString> key, float def)
{
    verbose("JBRIDGE", "getFloat(%s, %f)", key.get()->c_str(), def);
    return def;
}

std::shared_ptr<FakeJni::JString> jnivm::android::content::SharedPreferences::getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def)
{
    if (key != nullptr) {
        if (def != nullptr) {
            verbose("JBRIDGE", "getString(%s, %s)", key.get()->c_str(), def.get()->c_str());
        } else {
            verbose("JBRIDGE", "getString(%s, null)", key.get()->c_str());
        }
    }
    return def;
}

std::shared_ptr<jnivm::java::util::Map> jnivm::android::content::SharedPreferences::getAll()
{
    return std::make_shared<jnivm::java::util::Map>();
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferences::edit()
{
    return std::make_shared<SharedPreferencesEditor>();
}

///// SharedPreferencesEditor

void jnivm::android::content::SharedPreferencesEditor::apply()
{
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putInt(std::shared_ptr<FakeJni::JString> key, int val)
{
    verbose("JBRIDGE", "putInt(%s, %d)", key.get()->c_str(), val);
    return std::shared_ptr<SharedPreferencesEditor>(this);
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> val)
{
    verbose("JBRIDGE", "putString(%s, %s)", key.get()->c_str(), val.get()->c_str());
    return std::shared_ptr<SharedPreferencesEditor>(this);
}

///// Context

std::shared_ptr<jnivm::android::content::res::AssetManager>
jnivm::android::content::Context::getAssets()
{
    return std::make_shared<jnivm::android::content::res::AssetManager>();
}

std::shared_ptr<jnivm::android::content::pm::ApplicationInfo>
jnivm::android::content::Context::getApplicationInfo()
{
    return std::make_shared<jnivm::android::content::pm::ApplicationInfo>();
}

std::shared_ptr<FakeJni::JObject>
jnivm::android::content::Context::getSystemService(std::shared_ptr<FakeJni::JString> service)
{
    if (service == nullptr)
        return nullptr;

    if (*service == LOCATION_SERVICE)
        return nullptr;

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

    verbose("JBRIDGE", "App requesting unknown system service %s", service.get()->c_str());

    return nullptr;
}

std::shared_ptr<FakeJni::JString>
jnivm::android::content::Context::getPackageName()
{
    return std::make_shared<FakeJni::JString>(config["package"]["packageName"].value_or<std::string>("package.name.not.defined"));
}

std::shared_ptr<jnivm::android::content::pm::PackageManager>
jnivm::android::content::Context::getPackageManager()
{
    return std::make_shared<jnivm::android::content::pm::PackageManager>();
}

std::shared_ptr<jnivm::android::content::SharedPreferences>
jnivm::android::content::Context::getSharedPreferences(std::shared_ptr<FakeJni::JString> str, int num)
{
    return std::make_shared<jnivm::android::content::SharedPreferences>();
}

std::shared_ptr<FakeJni::JString>
jnivm::android::content::Context::getPackageCodePath()
{
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
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::ApplicationInfo) { FakeJni::Constructor<ApplicationInfo> {} },
    { FakeJni::Field<&ApplicationInfo::splitPublicSourceDirs> {}, "splitPublicSourceDirs", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::PackageManager) { FakeJni::Constructor<PackageManager> {} },
    { FakeJni::Field<&PackageManager::FEATURE_AUDIO_LOW_LATENCY> {}, "FEATURE_AUDIO_LOW_LATENCY", FakeJni::JFieldID::STATIC },
    { FakeJni::Field<&PackageManager::PERMISSION_GRANTED> {}, "PERMISSION_GRANTED", FakeJni::JFieldID::STATIC },
    { FakeJni::Function<&PackageManager::getPackageInfo> {}, "getPackageInfo", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&PackageManager::hasSystemFeature> {}, "hasSystemFeature", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::res::AssetManager) { FakeJni::Constructor<AssetManager> {} },
    { FakeJni::Function<&AssetManager::open> {}, "open", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&AssetManager::list> {}, "list", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::res::Resources) { FakeJni::Constructor<Resources> {} },
    { FakeJni::Function<&Resources::getIdentifier> {}, "getIdentifier", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::SharedPreferencesEditor) { FakeJni::Constructor<SharedPreferencesEditor> {} },
    { FakeJni::Function<&SharedPreferencesEditor::apply> {}, "apply", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::putInt> {}, "putInt", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferencesEditor::putString> {}, "putString", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::SharedPreferences) { FakeJni::Constructor<SharedPreferences> {} },
    { FakeJni::Function<&SharedPreferences::contains> {}, "contains", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getInt> {}, "getInt", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getFloat> {}, "getFloat", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getString> {}, "getString", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::getAll> {}, "getAll", FakeJni::JMethodID::PUBLIC },
    { FakeJni::Function<&SharedPreferences::edit> {}, "edit", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::ContentResolver) { FakeJni::Constructor<ContentResolver> {} },
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::Context) { FakeJni::Constructor<Context> {} },
    { FakeJni::Field<&Context::LOCATION_SERVICE> {}, "LOCATION_SERVICE", FakeJni::JFieldID::STATIC },
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
    { FakeJni::Function<&Intent::getExtras> {}, "getExtras", FakeJni::JMethodID::PUBLIC },
    END_NATIVE_DESCRIPTOR