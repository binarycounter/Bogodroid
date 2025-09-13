
#include "toml++/toml.hpp"
extern toml::table config;
#include "ndk.h"
#include "logging.h"
#include "thunk_gen.h"
#include "platform.h"
#include "so_util.h"
#include <unistd.h>
#include <sys/syscall.h>
#include <poll.h>
#include "alooper.h"
#include "asset_manager.h"
#include "anative_activity.h"

static ANativeWindow* default_native_window;

ABI_ATTR AConfiguration *AConfiguration_new()
{
    AConfiguration *config = new AConfiguration;
    memset(config, 0, sizeof(AConfiguration));
    return config;
}

ABI_ATTR void AConfiguration_fromAssetManager(AConfiguration *out, void *am)
{
}

ABI_ATTR int32_t AConfiguration_getMcc(AConfiguration *aconfig)
{
    return 0;
}
ABI_ATTR int32_t AConfiguration_getMnc(AConfiguration *aconfig)
{
    return 0;
}
ABI_ATTR void AConfiguration_getLanguage(AConfiguration *aconfig, char *outLanguage)
{
    const char *lang = config["device"]["language"].value_or("en");
    outLanguage[0] = lang[0];
    outLanguage[1] = lang[1];
}
ABI_ATTR void AConfiguration_getCountry(AConfiguration *aconfig, char *outCountry)
{
    const char *country = config["device"]["country"].value_or("US");
    outCountry[0] = country[0];
    outCountry[1] = country[1];
}
ABI_ATTR int32_t AConfiguration_getOrientation(AConfiguration *aconfig)
{
    return config["device"]["displayRotation"].value_or<int>(2); // LAND
}
ABI_ATTR int32_t AConfiguration_getTouchscreen(AConfiguration *aconfig)
{
    return config["device"]["displayTouchscreen"].value_or<int>(1); // NOTOUCH
}
ABI_ATTR int32_t AConfiguration_getDensity(AConfiguration *aconfig)
{
    return config["device"]["displayDensity"].value_or<int>(160); // MEDIUM
}
ABI_ATTR int32_t AConfiguration_getKeyboard(AConfiguration *aconfig)
{
    return config["device"]["keyboard"].value_or<int>(2); // QWERTY
}
ABI_ATTR int32_t AConfiguration_getNavigation(AConfiguration *aconfig)
{
    return 0;
}
ABI_ATTR int32_t AConfiguration_getKeysHidden(AConfiguration *aconfig)
{
    return 0;
}
ABI_ATTR int32_t AConfiguration_getNavHidden(AConfiguration *aconfig)
{
    return 0;
}
ABI_ATTR int32_t AConfiguration_getSdkVersion(AConfiguration *aconfig)
{
    return 25;
}
ABI_ATTR int32_t AConfiguration_getScreenSize(AConfiguration *aconfig)
{
    return 2;
}
ABI_ATTR int32_t AConfiguration_getScreenLong(AConfiguration *aconfig)
{
    return 1;
}
ABI_ATTR int32_t AConfiguration_getUiModeType(AConfiguration *aconfig)
{
    return 0;
}
ABI_ATTR int32_t AConfiguration_getUiModeNight(AConfiguration *aconfig)
{
    return 0;
}

ABI_ATTR int32_t ret0()
{
    return 0;
}

ABI_ATTR ANativeWindow* ANativeWindow_fromSurface(void*, void*)
{
    if(default_native_window==NULL)
    {
        default_native_window = (ANativeWindow*)malloc(sizeof(ANativeWindow));
    }
    return default_native_window;
}

ABI_ATTR int32_t ANativeWindow_getWidth(ANativeWindow *window)
{
    return config["device"]["displayWidth"].value_or<int>(640);
}

ABI_ATTR int32_t ANativeWindow_getHeight(ANativeWindow *window)
{
    return config["device"]["displayHeight"].value_or<int>(480);
}

ABI_ATTR void __assert2(const char* __file, int __line, const char* __function, const char* __msg)
{
    fatal_error("ASSERT!! File: %s Line: %d Function: %s Message: %s\n",__file, __line, __function, __msg);
    exit(1);
}

DynLibFunction symtable_ndk[] = {
NO_THUNK("AConfiguration_new", (uintptr_t)&AConfiguration_new),
NO_THUNK("AConfiguration_fromAssetManager", (uintptr_t)&AConfiguration_fromAssetManager),
NO_THUNK("AConfiguration_getLanguage", (uintptr_t)&AConfiguration_getLanguage),
NO_THUNK("AConfiguration_getCountry", (uintptr_t)&AConfiguration_getCountry),
NO_THUNK("AConfiguration_getMcc", (uintptr_t)&AConfiguration_getMcc),
NO_THUNK("AConfiguration_getMnc", (uintptr_t)&AConfiguration_getMnc),
NO_THUNK("AConfiguration_getOrientation", (uintptr_t)&AConfiguration_getOrientation),
NO_THUNK("AConfiguration_getTouchscreen", (uintptr_t)&AConfiguration_getTouchscreen),
NO_THUNK("AConfiguration_getDensity", (uintptr_t)&AConfiguration_getDensity),
NO_THUNK("AConfiguration_getKeyboard", (uintptr_t)&AConfiguration_getKeyboard),
NO_THUNK("AConfiguration_getNavigation", (uintptr_t)&AConfiguration_getNavigation),
NO_THUNK("AConfiguration_getKeysHidden", (uintptr_t)&AConfiguration_getKeysHidden),
NO_THUNK("AConfiguration_getNavHidden", (uintptr_t)&AConfiguration_getNavHidden),
NO_THUNK("AConfiguration_getSdkVersion", (uintptr_t)&AConfiguration_getSdkVersion),
NO_THUNK("AConfiguration_getScreenSize", (uintptr_t)&AConfiguration_getScreenSize),
NO_THUNK("AConfiguration_getScreenLong", (uintptr_t)&AConfiguration_getScreenLong),
NO_THUNK("AConfiguration_getUiModeType", (uintptr_t)&AConfiguration_getUiModeType),
NO_THUNK("AConfiguration_getUiModeNight", (uintptr_t)&AConfiguration_getUiModeNight),
NO_THUNK("ASensorManager_getInstanceForPackage", (uintptr_t)&ret0), //AWFUL
NO_THUNK("ASensorManager_getInstance", (uintptr_t)&ret0), //AWFUL
NO_THUNK("ASensorManager_getDefaultSensor", (uintptr_t)&ret0), //AWFUL
NO_THUNK("ASensorManager_createEventQueue", (uintptr_t)&ret0), //AWFUL
NO_THUNK("ALooper_prepare", (uintptr_t)&ALooper_prepare),
NO_THUNK("ALooper_addFd", (uintptr_t)&ALooper_addFd),
NO_THUNK("ALooper_pollAll", (uintptr_t)&ALooper_pollAll),
NO_THUNK("ALooper_pollOnce", (uintptr_t)&ALooper_pollOnce), //AWFUL
NO_THUNK("ANativeWindow_fromSurface", (uintptr_t)&ANativeWindow_fromSurface), //AWFUL
NO_THUNK("ANativeWindow_acquire", (uintptr_t)&ret0), //AWFUL
NO_THUNK("ANativeWindow_release", (uintptr_t)&ret0), //AWFUL
NO_THUNK("ANativeWindow_setBuffersGeometry", (uintptr_t)&ret0), //AWFUL
NO_THUNK("ANativeWindow_getWidth", (uintptr_t)&ANativeWindow_getWidth),
NO_THUNK("ANativeWindow_getHeight", (uintptr_t)&ANativeWindow_getHeight),
NO_THUNK("__assert2", (uintptr_t)&__assert2),
NO_THUNK("AAssetManager_open",(uintptr_t)&AAssetManager_open),
NO_THUNK("AAsset_getBuffer",(uintptr_t)&AAsset_getBuffer),
NO_THUNK("AAsset_getLength",(uintptr_t)&AAsset_getLength),
NO_THUNK("AAsset_close",(uintptr_t)&AAsset_close),
    {NULL, (uintptr_t)NULL}};



