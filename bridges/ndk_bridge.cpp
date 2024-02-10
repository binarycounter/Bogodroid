
#include "toml++/toml.hpp"
extern toml::table config;
#include "ndk_bridge.h"
#include "platform.h"
#include "so_util.h"



ABI_ATTR AConfiguration* AConfiguration_new()
{

}

ABI_ATTR void AConfiguration_fromAssetManager(AConfiguration* out, void* am)
{

}

ABI_ATTR int32_t AConfiguration_getMcc(AConfiguration* aconfig) {
    return 0;
}
ABI_ATTR int32_t AConfiguration_getMnc(AConfiguration* aconfig) {
    return 0;
}
ABI_ATTR void AConfiguration_getLanguage(AConfiguration* aconfig, char* outLanguage) {
    const char* lang = config["device"]["language"].value_or("en");
    outLanguage[0] = lang[0];
    outLanguage[1] = lang[1];
}
ABI_ATTR void AConfiguration_getCountry(AConfiguration* aconfig, char* outCountry) {
    const char* country = config["device"]["country"].value_or("US");
    outCountry[0] = country[0];
    outCountry[1] = country[1];
}
ABI_ATTR int32_t AConfiguration_getOrientation(AConfiguration* aconfig) {
    return config["device"]["displayRotation"].value_or<int>(2); //LAND
}
ABI_ATTR int32_t AConfiguration_getTouchscreen(AConfiguration* aconfig) {
    return config["device"]["displayTouchscreen"].value_or<int>(1); //NOTOUCH
}
ABI_ATTR int32_t AConfiguration_getDensity(AConfiguration* aconfig) {
    return config["device"]["displayDensity"].value_or<int>(160); //MEDIUM
}
ABI_ATTR int32_t AConfiguration_getKeyboard(AConfiguration* aconfig) {
    return config["device"]["keyboard"].value_or<int>(2); //QWERTY
}
ABI_ATTR int32_t AConfiguration_getNavigation(AConfiguration* aconfig) {
    return 0;
}
ABI_ATTR int32_t AConfiguration_getKeysHidden(AConfiguration* aconfig) {
    return 0;
}
ABI_ATTR int32_t AConfiguration_getNavHidden(AConfiguration* aconfig) {
    return 0;
}
ABI_ATTR int32_t AConfiguration_getSdkVersion(AConfiguration* aconfig) {
    return 25;
}
ABI_ATTR int32_t AConfiguration_getScreenSize(AConfiguration* aconfig) {
    return 2;
}
ABI_ATTR int32_t AConfiguration_getScreenLong(AConfiguration* aconfig) {
    return 1;
}
ABI_ATTR int32_t AConfiguration_getUiModeType(AConfiguration* aconfig) {
    return 0;
}
ABI_ATTR int32_t AConfiguration_getUiModeNight(AConfiguration* aconfig) {
    return 0;
}

DynLibFunction symtable_ndk[] = {
    {"AConfiguration_new", (uintptr_t)&AConfiguration_new},
    {"AConfiguration_fromAssetManager", (uintptr_t)&AConfiguration_fromAssetManager},
    {"AConfiguration_getLanguage", (uintptr_t)&AConfiguration_getLanguage},
    {"AConfiguration_getCountry", (uintptr_t)&AConfiguration_getCountry},
    {"AConfiguration_getMcc", (uintptr_t)&AConfiguration_getMcc},
    {"AConfiguration_getMnc", (uintptr_t)&AConfiguration_getMnc},
    {"AConfiguration_getOrientation", (uintptr_t)&AConfiguration_getOrientation},
    {"AConfiguration_getTouchscreen", (uintptr_t)&AConfiguration_getTouchscreen},
    {"AConfiguration_getDensity", (uintptr_t)&AConfiguration_getDensity},
    {"AConfiguration_getKeyboard", (uintptr_t)&AConfiguration_getKeyboard},
    {"AConfiguration_getNavigation", (uintptr_t)&AConfiguration_getNavigation},
    {"AConfiguration_getKeysHidden", (uintptr_t)&AConfiguration_getKeysHidden},
    {"AConfiguration_getNavHidden", (uintptr_t)&AConfiguration_getNavHidden},
    {"AConfiguration_getSdkVersion", (uintptr_t)&AConfiguration_getSdkVersion},
    {"AConfiguration_getScreenSize", (uintptr_t)&AConfiguration_getScreenSize},
    {"AConfiguration_getScreenLong", (uintptr_t)&AConfiguration_getScreenLong},
    {"AConfiguration_getUiModeType", (uintptr_t)&AConfiguration_getUiModeType},
    {"AConfiguration_getUiModeNight", (uintptr_t)&AConfiguration_getUiModeNight},
    {NULL, (uintptr_t)NULL}};