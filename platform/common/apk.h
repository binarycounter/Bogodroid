// Read-only access to the game's APK (a zip file), for [paths] apk = "...".
//
// With an APK configured, Unity gets the APK itself as its package path and reads its data through its own APK
// reader, exactly as on Android (which also makes jar:file://...!/assets/... URLs work: StreamingAssets, Addressables,
// Unity Services config). The loader only extracts what has to exist as plain files (native libraries, Mono/CoreCLR
// managed code), and its own asset readers fall back to the APK for anything not on disk.
#ifndef __BOGO_APK_H__
#define __BOGO_APK_H__

#include <stddef.h>

#ifdef __cplusplus
#include <string>
#include <vector>

// The configured APK, opened on first use. Null when [paths] apk isn't set or the file can't be read.
class ApkArchive;
ApkArchive* bogo_apk();

// Absolute path of the configured APK, empty if none.
std::string bogo_apk_path();

// The ABIs the APK ships native libraries for (the folder names under lib/), e.g. {"armeabi-v7a"}.
std::vector<std::string> bogo_apk_abis();

// Extracts lib/<abi>/* (see abi.h), assets/bin/Data/Managed/* and assets/dotnet/* into the current directory (the game
// files folder) when they're missing or older than the APK. For a new APK the old assets/ and lib/ are cleared first
// (except any libc++_shared.so, which isn't in the APK). Returns false if extraction failed.
bool bogo_apk_extract_runtime_files();

class ApkArchive {
public:
    struct Entry {
        std::string name;
        unsigned method = 0;          // 0 stored, 8 deflate
        unsigned long long compressedSize = 0, size = 0, localHeader = 0;
    };

    bool open(const std::string& path);
    const Entry* find(const std::string& name) const;
    bool read(const std::string& name, std::string& out) const;
    // Names directly inside a folder ("assets/foo" -> files and subfolders of assets/foo), without the prefix.
    std::vector<std::string> list(const std::string& folder, bool includeFolders) const;
    const std::vector<Entry>& entries() const { return entries_; }
    const std::string& path() const { return path_; }

private:
    std::string path_;
    std::vector<Entry> entries_;
};

extern "C" {
#endif

// For C callers (the NDK AAssetManager thunk). Names are full entry names, e.g. "assets/foo.txt".
int bogo_apk_exists(const char* name);
// Reads an entry into a malloc'd buffer the caller frees. Returns 0 on success.
int bogo_apk_read(const char* name, void** data, size_t* size);
// Lists the files directly inside a folder; returns a malloc'd array of malloc'd names (caller frees), count in *count.
char** bogo_apk_list(const char* folder, int* count);

#ifdef __cplusplus
}
#endif

#endif
