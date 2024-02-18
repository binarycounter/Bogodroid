
#include "toml++/toml.hpp"
extern toml::table config;
#include "ndk_bridge.h"
#include "platform.h"
#include "so_util.h"
#include <unistd.h>
#include <sys/syscall.h>
#include <poll.h>

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

ABI_ATTR ALooper *ALooper_prepare(int opts)
{
    pid_t tid = syscall(SYS_gettid);
    verbose("NATIVE", "looper prepare. Existing loopers for tid %d = %d", tid, loopers.count(tid));
    if (loopers.count(tid) > 0)
        return loopers[tid];

    ALooper *looper = new ALooper();
    looper->events = new std::vector<LooperEntry>();
    loopers.insert(std::pair<pid_t, ALooper *>(tid, looper));
    verbose("NATIVE", "looper prepare. New looper for tid %d = %p", tid, looper);
    return looper;
}

ABI_ATTR int32_t ALooper_addFd(ALooper *looper, int fd, int ident, int events, void *callback, void *data)
{
    verbose("NATIVE", "looper addFd %p %d %d %d %p %p", looper, fd, ident, events, callback, *(int *)data);
    LooperEntry entry = {};
    entry.fd = fd;
    entry.ident = ident;
    entry.events = events;
    entry.callback = callback;
    entry.data = data;
    looper->events->push_back(entry);
    return 1;
}

ABI_ATTR int32_t ALooper_pollAll(int timeout, int *outFd, int *outEvents, void **outData)
{
    verbose("NATIVE", "pollAll %p %p %p", outFd, outEvents, outData);

    for (auto looper_pair : loopers)
    {
        verbose("NATIVE", "processing looper for thread %d", looper_pair.first);
        for (LooperEntry entry : *(looper_pair.second->events))
        {
            verbose("NATIVE", "processing fd %d", entry.fd);
            fd_set read_fds;
            FD_ZERO(&read_fds);         
            FD_SET(entry.fd, &read_fds); 
            struct timeval select_timeout;
            select_timeout.tv_sec = 0;
            select_timeout.tv_usec = 0;
            int fds = select(entry.fd+1, &read_fds, NULL, NULL, &select_timeout);
            if (fds > 0)
            {
                if (FD_ISSET(entry.fd, &read_fds))
                {
                    verbose("NATIVE", "new data in fd %d", entry.fd);
                    *outFd=entry.fd;
                    *outEvents=entry.events;
                    *outData=entry.data;
                    return entry.ident;
                }
            }
        }
    }

    usleep(timeout * 1000);
    return -3;
}

ABI_ATTR int32_t ret0()
{
    return 0;
}

ABI_ATTR int32_t ANativeWindow_getWidth(ANativeWindow *window)
{
    return config["device"]["displayWidth"].value_or<int>(640);
}

ABI_ATTR int32_t ANativeWindow_getHeight(ANativeWindow *window)
{
    return config["device"]["displayHeight"].value_or<int>(480);
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
    {"ALooper_prepare", (uintptr_t)&ALooper_prepare},
    {"ALooper_addFd", (uintptr_t)&ALooper_addFd},
    {"ALooper_pollAll", (uintptr_t)&ALooper_pollAll},
    {"ANativeWindow_getWidth", (uintptr_t)&ANativeWindow_getWidth},
    {"ANativeWindow_getHeight", (uintptr_t)&ANativeWindow_getHeight},
    {NULL, (uintptr_t)NULL}};