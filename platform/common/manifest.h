// The few values the loader needs from the game's AndroidManifest.xml (binary AXML), read from the configured APK
// (or a plain AndroidManifest.xml in the game files folder). Config overrides ([package] ...) win over these.
#pragma once

#include <string>

struct BogoManifestInfo {
    bool loaded = false;
    std::string packageName;
    std::string versionName;
    int versionCode = 0;
    int minSdkVersion = 0;
    int targetSdkVersion = 0;
};

// Parsed once, on first use. All fields stay empty/0 when there is no manifest (loaded == false).
const BogoManifestInfo& bogo_manifest();

// Effective values: the [package] config entry if present, else the manifest, else the fallback.
std::string bogo_package_version_name(const char* fallback);
int bogo_package_version_code();
int bogo_min_sdk_version();
int bogo_target_sdk_version();
