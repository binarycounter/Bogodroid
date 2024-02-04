#include "baron/baron.h"
#include <fstream>
#include "android.h"
#include "javac.h"
#include "platform.h"



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
    return nullptr;
}

std::shared_ptr<FakeJni::JString>
jnivm::android::content::Context::getPackageName()
{
    return std::make_shared<FakeJni::JString>("com.binary.testGame");
}

std::shared_ptr<jnivm::android::content::pm::PackageManager>
jnivm::android::content::Context::getPackageManager()
{
    return std::make_shared<jnivm::android::content::pm::PackageManager>();
}

std::shared_ptr<FakeJni::JString>
jnivm::android::content::Context::getPackageCodePath()
{
    return std::make_shared<FakeJni::JString>("/fakepath");
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getExternalFilesDir(std::shared_ptr<FakeJni::JString> path)
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>("/fakepath2"));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getFilesDir()
{
    return std::make_shared<jnivm::java::io::File>(std::make_shared<FakeJni::JString>("/fakepath3"));
}

std::shared_ptr<jnivm::java::io::File>
jnivm::android::content::Context::getObbDir()
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
    verbose("JBRIDGE","Bundle containsKey %s", key.get()->c_str());
    return true;
}

std::shared_ptr<FakeJni::JString> jnivm::android::os::Bundle::getString(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def)
{
    return std::make_shared<FakeJni::JString>("");
}

///// Process

void jnivm::android::os::Process::setThreadPriority(int i, int j){}


///// Environment

std::shared_ptr<FakeJni::JString>
jnivm::android::os::Environment::getExternalStorageState()
{
    return std::make_shared<FakeJni::JString>("MEDIA_REMOVED");
}

///// Descriptors

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

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::Context){FakeJni::Constructor<Context>{}},
    {FakeJni::Field<&Context::LOCATION_SERVICE>{}, "LOCATION_SERVICE", FakeJni::JFieldID::STATIC},
    {FakeJni::Field<&Context::MODE_PRIVATE>{}, "MODE_PRIVATE", FakeJni::JFieldID::STATIC},
    {FakeJni::Function<&Context::getSystemService>{}, "getSystemService", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getAssets>{}, "getAssets", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getApplicationInfo>{}, "getApplicationInfo", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getPackageName>{}, "getPackageName", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getPackageManager>{}, "getPackageManager", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getPackageCodePath>{}, "getPackageCodePath", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getExternalFilesDir>{}, "getExternalFilesDir", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getFilesDir>{}, "getFilesDir", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Context::getObbDir>{}, "getObbDir", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::content::Intent){FakeJni::Constructor<Intent>{}},
    {FakeJni::Function<&Intent::getExtras>{}, "getExtras", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Bundle){FakeJni::Constructor<Bundle>{}},
    {FakeJni::Function<&Bundle::containsKey>{}, "containsKey", FakeJni::JMethodID::PUBLIC},
    {FakeJni::Function<&Bundle::getString>{}, "getString", FakeJni::JMethodID::PUBLIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Process){FakeJni::Constructor<Process>{}},
    {FakeJni::Function<&Process::setThreadPriority>{}, "setThreadPriority", FakeJni::JMethodID::STATIC},
    END_NATIVE_DESCRIPTOR

    BEGIN_NATIVE_DESCRIPTOR(jnivm::android::os::Environment){FakeJni::Constructor<Environment>{}},
    {FakeJni::Field<&Environment::MEDIA_MOUNTED>{}, "MEDIA_MOUNTED", FakeJni::JFieldID::STATIC},
    {FakeJni::Function<&Environment::getExternalStorageState>{}, "getExternalStorageState", FakeJni::JMethodID::STATIC},
    END_NATIVE_DESCRIPTOR