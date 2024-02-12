#include "toml++/toml.hpp"
extern toml::table config;

#include "baron/baron.h"
#include <fstream>
#include "android.h"
#include "javac.h"
#include "platform.h"


///// Display

int jnivm::android::view::Display::getDisplayId()
{
    return 0;
}

int jnivm::android::view::Display::getRotation()
{
    return config["device"]["displayRotation"].value_or<int>(0);
}

int jnivm::android::view::Display::getWidth()
{
    return config["device"]["displayWidth"].value_or<int>(640);
}

int jnivm::android::view::Display::getHeight()
{
    return config["device"]["displayHeight"].value_or<int>(480);
}


///// DisplayManager

std::shared_ptr<jnivm::android::view::Display>
jnivm::android::hardware::display::DisplayManager::getDisplay()
{
    return std::make_shared<jnivm::android::view::Display>();
}


///// PackageManager

std::shared_ptr<jnivm::android::content::pm::PackageInfo>
jnivm::android::content::pm::PackageManager::getPackageInfo(std::shared_ptr<FakeJni::JString> packageName, int number)
{
    return std::make_shared<jnivm::android::content::pm::PackageInfo>();
}

///// AssetManager

std::shared_ptr<jnivm::java::io::InputStream>
jnivm::android::content::res::AssetManager::open(std::shared_ptr<FakeJni::JString> filename)
{
    verbose("JBRIDGE","AssetManager opening file %s", filename.get()->c_str());
    return std::make_shared<jnivm::java::io::InputStream>(std::make_shared<FakeJni::JString>(std::string("assets/").append(filename.get()->c_str())));
}


///// SharedPreferences

bool jnivm::android::content::SharedPreferences::contains(std::shared_ptr<FakeJni::JString> key)
{
    return false;
}

int jnivm::android::content::SharedPreferences::getInt(std::shared_ptr<FakeJni::JString> key, int def)
{
    return def;
}


std::shared_ptr<FakeJni::JString> jnivm::android::content::SharedPreferences::getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def)
{
    return def;
}

std::shared_ptr<jnivm::java::util::Map> jnivm::android::content::SharedPreferences::getAll()
{
    return nullptr;
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferences::edit()
{
    return std::make_shared<SharedPreferencesEditor>();
}


///// SharedPreferencesEditor

void jnivm::android::content::SharedPreferencesEditor::apply()
{

}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putInt(std::shared_ptr<FakeJni::JString> key,int val)
{
    return std::shared_ptr<SharedPreferencesEditor>(this);
}

std::shared_ptr<jnivm::android::content::SharedPreferencesEditor> jnivm::android::content::SharedPreferencesEditor::putString(std::shared_ptr<FakeJni::JString> key,std::shared_ptr<FakeJni::JString> val)
{
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
    if(*service == LOCATION_SERVICE)
        return nullptr;

    if(*service == AUDIO_SERVICE)
        return nullptr;
    
    if(*service == DISPLAY_SERVICE)
        return std::make_shared<jnivm::android::hardware::display::DisplayManager>();

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
    return std::make_shared<FakeJni::JString>(config["paths"]["android_package_code"].value_or<std::string>("/path_not_defined_code"));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getExternalFilesDir(std::shared_ptr<FakeJni::JString> path)
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(config["paths"]["android_external_files"].value_or<std::string>("/path_not_defined_external")));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getFilesDir()
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(config["paths"]["android_files"].value_or<std::string>("/path_not_defined_files")));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getCacheDir()
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>(config["paths"]["android_cache"].value_or<std::string>("/path_not_defined_cache")));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getObbDir()
{
    //return std::make_shared<jnivm::java::io::File>(config["paths"]["obb_dir"][0].value_or<std::string>("/path_not_defined_obb"));
    return NULL;
}

std::shared_ptr<jnivm::Array<jnivm::java::io::File>>
jnivm::android::content::Context::getObbDirs()
{
    return NULL;
}

///// Activity

std::shared_ptr<jnivm::android::content::Intent>
jnivm::android::app::Activity::getIntent()
{
    return std::make_shared<jnivm::android::content::Intent>();
}

///// Intent

std::shared_ptr<jnivm::android::os::Bundle>
jnivm::android::content::Intent::getExtras()
{
    return std::make_shared<jnivm::android::os::Bundle>();
}

///// Bundle

bool jnivm::android::os::Bundle::containsKey(std::shared_ptr<FakeJni::JString> key)
{
    

    if(!config["package"].as_table()->contains("mainIntentBundle"))
        return false;

    verbose("JBRIDGE","Bundle containsKey %s %d", key.get()->c_str(),config["package"].as_table()->contains("mainIntentBundle"));

    return config["package"]["mainIntentBundle"].as_table()->contains(key.get()->c_str());
}

std::shared_ptr<FakeJni::JString> jnivm::android::os::Bundle::getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def)
{
    if(!containsKey(key))
        return def;

    verbose("JBRIDGE","Bundle key %s = %s", key.get()->c_str(),config["package"]["mainIntentBundle"][key.get()->c_str()].value_or<std::string>("").c_str());   
    return std::make_shared<FakeJni::JString>(config["package"]["mainIntentBundle"][key.get()->c_str()].value_or<std::string>(""));
}

///// Process

void jnivm::android::os::Process::setThreadPriority(int i, int j){}


///// Looper

std::shared_ptr<jnivm::android::os::Looper> jnivm::android::os::Looper::getMainLooper()
{
    return std::make_shared<Looper>();
}

///// Handler

jnivm::android::os::Handler::Handler(std::shared_ptr<jnivm::android::os::Looper> looper)
{

}

///// Environment

std::shared_ptr<FakeJni::JString>
jnivm::android::os::Environment::getExternalStorageState()
{
    return std::make_shared<FakeJni::JString>("MEDIA_REMOVED");
}

///// Descriptors

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::view::Display){FakeJni::Constructor<Display>{}},
    {FakeJni::Function<&Display::getDisplayId>{}, "getDisplayId", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Display::getRotation>{}, "getRotation", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Display::getWidth>{}, "getWidth", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Display::getHeight>{}, "getHeight", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::hardware::display::DisplayManager){FakeJni::Constructor<DisplayManager>{}},
    {FakeJni::Function<&DisplayManager::getDisplay>{}, "getDisplay", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR


    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::PackageInfo){FakeJni::Constructor<PackageInfo>{}},
    {FakeJni::Field<&PackageInfo::versionName>{}, "versionName", FakeJni::JFieldID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::ApplicationInfo){FakeJni::Constructor<ApplicationInfo>{}},
    {FakeJni::Field<&ApplicationInfo::splitPublicSourceDirs>{}, "splitPublicSourceDirs", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::pm::PackageManager){FakeJni::Constructor<PackageManager>{}},
    {FakeJni::Function<&PackageManager::getPackageInfo>{}, "getPackageInfo", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::res::AssetManager){FakeJni::Constructor<AssetManager>{}},
    {FakeJni::Function<&AssetManager::open>{}, "open", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::app::Activity){FakeJni::Constructor<Activity>{}},
    {FakeJni::Function<&Activity::getIntent>{}, "getIntent", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::app::NativeActivity){FakeJni::Constructor<NativeActivity>{}},
    END_NATIVE_DESCRIPTOR


    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::SharedPreferencesEditor){FakeJni::Constructor<SharedPreferencesEditor>{}},
    {FakeJni::Function<&SharedPreferencesEditor::apply>{}, "apply", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&SharedPreferencesEditor::putInt>{}, "putInt", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&SharedPreferencesEditor::putString>{}, "putString", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR


    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::SharedPreferences){FakeJni::Constructor<SharedPreferences>{}},
    {FakeJni::Function<&SharedPreferences::contains>{}, "contains", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&SharedPreferences::getInt>{}, "getInt", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&SharedPreferences::getString>{}, "getString", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&SharedPreferences::getAll>{}, "getAll", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&SharedPreferences::edit>{}, "edit", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::Context){FakeJni::Constructor<Context>{}},
    {FakeJni::Field<&Context::LOCATION_SERVICE>{}, "LOCATION_SERVICE", FakeJni::JFieldID::STATIC},
    {FakeJni::Field<&Context::DISPLAY_SERVICE>{}, "DISPLAY_SERVICE", FakeJni::JFieldID::STATIC},
    {FakeJni::Field<&Context::AUDIO_SERVICE>{}, "AUDIO_SERVICE", FakeJni::JFieldID::STATIC},
    {FakeJni::Field<&Context::MODE_PRIVATE>{}, "MODE_PRIVATE", FakeJni::JFieldID::STATIC},
    {FakeJni::Function<&Context::getSystemService>{}, "getSystemService", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getAssets>{}, "getAssets", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getApplicationInfo>{}, "getApplicationInfo", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getPackageName>{}, "getPackageName", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getPackageManager>{}, "getPackageManager", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getPackageCodePath>{}, "getPackageCodePath", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getSharedPreferences>{}, "getSharedPreferences", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getExternalFilesDir>{}, "getExternalFilesDir", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getFilesDir>{}, "getFilesDir", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getCacheDir>{}, "getCacheDir", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getObbDir>{}, "getObbDir", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getObbDirs>{}, "getObbDirs", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::Intent){FakeJni::Constructor<Intent>{}},
    {FakeJni::Function<&Intent::getExtras>{}, "getExtras", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Build){FakeJni::Constructor<Build>{}},
    {FakeJni::Field<&Build::MANUFACTURER>{}, "MANUFACTURER", FakeJni::JMethodID::STATIC},
    {FakeJni::Field<&Build::MODEL>{}, "MODEL", FakeJni::JMethodID::STATIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::BuildVersion){FakeJni::Constructor<BuildVersion>{}},
    {FakeJni::Field<&BuildVersion::SDK_INT>{}, "SDK_INT", FakeJni::JMethodID::STATIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Bundle){FakeJni::Constructor<Bundle>{}},
    {FakeJni::Function<&Bundle::containsKey>{}, "containsKey", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Bundle::getString>{}, "getString", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Process){FakeJni::Constructor<Process>{}},
    {FakeJni::Function<&Process::setThreadPriority>{}, "setThreadPriority", FakeJni::JMethodID::STATIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Looper){FakeJni::Constructor<Looper>{}},
    {FakeJni::Function<&Looper::getMainLooper>{}, "getMainLooper", FakeJni::JMethodID::STATIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Handler){FakeJni::Constructor<Handler, std::shared_ptr<Looper>>{}},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Environment){FakeJni::Constructor<Environment>{}},
    {FakeJni::Field<&Environment::MEDIA_MOUNTED>{}, "MEDIA_MOUNTED", FakeJni::JFieldID::STATIC},
    {FakeJni::Function<&Environment::getExternalStorageState>{}, "getExternalStorageState", FakeJni::JMethodID::STATIC},
    END_NATIVE_DESCRIPTOR