#include "baron/baron.h"
#include "limbo.h"



std::shared_ptr<FakeJni::JString> jnivm::com::playdead::limbo::LimboActivity::GetLimboCachedAassetsPath()
{
    auto context=std::make_shared<jnivm::android::content::Context>();
    return context->getCacheDir()->getPath();
}


BEGIN_NATIVE_DESCRIPTOR(jnivm::com::playdead::limbo::LimboActivity){FakeJni::Constructor<LimboActivity>{}},
{FakeJni::Function<&LimboActivity::GetLimboCachedAassetsPath>{}, "GetLimboCachedAssetsPath", FakeJni::JMethodID::PUBLIC },
END_NATIVE_DESCRIPTOR


