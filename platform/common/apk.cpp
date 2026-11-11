#include "abi.h"
#include "apk.h"
#include "logging.h"
#include "toml++/toml.hpp"

#include <zlib.h>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

extern toml::table config;

namespace {
    uint16_t le16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
    uint32_t le32(const unsigned char* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
    uint64_t le64(const unsigned char* p) { return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32); }

    bool readAt(std::ifstream& f, unsigned long long off, void* buf, size_t len)
    {
        f.clear();
        f.seekg((std::streamoff)off);
        f.read(reinterpret_cast<char*>(buf), (std::streamsize)len);
        return (size_t)f.gcount() == len;
    }
}

bool ApkArchive::open(const std::string& path)
{
    path_ = path;
    entries_.clear();
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    unsigned long long fileSize = (unsigned long long)f.tellg();
    if (fileSize < 22) return false;

    // End of central directory: in the last 64 KiB + 22 bytes (the comment can be up to 64 KiB).
    size_t tail = (size_t)std::min<unsigned long long>(fileSize, 65535 + 22);
    std::vector<unsigned char> buf(tail);
    if (!readAt(f, fileSize - tail, buf.data(), tail)) return false;
    long eocd = -1;
    for (long i = (long)tail - 22; i >= 0; i--)
        if (le32(&buf[i]) == 0x06054b50) { eocd = i; break; }
    if (eocd < 0) return false;
    unsigned long long cdCount = le16(&buf[eocd + 10]);
    unsigned long long cdSize = le32(&buf[eocd + 12]);
    unsigned long long cdOffset = le32(&buf[eocd + 16]);

    // Zip64 end of central directory (only for very large archives).
    if (eocd >= 20 && le32(&buf[eocd - 20]) == 0x07064b50) {
        unsigned char z[56];
        unsigned long long z64 = le64(&buf[eocd - 20 + 8]);
        if (readAt(f, z64, z, sizeof(z)) && le32(z) == 0x06064b50) {
            cdCount = le64(z + 32);
            cdSize = le64(z + 40);
            cdOffset = le64(z + 48);
        }
    }

    std::vector<unsigned char> cd(cdSize);
    if (!readAt(f, cdOffset, cd.data(), cdSize)) return false;
    size_t p = 0;
    for (unsigned long long n = 0; n < cdCount && p + 46 <= cd.size(); n++) {
        if (le32(&cd[p]) != 0x02014b50) break;
        Entry e;
        e.method = le16(&cd[p + 10]);
        e.compressedSize = le32(&cd[p + 20]);
        e.size = le32(&cd[p + 24]);
        uint16_t nameLen = le16(&cd[p + 28]), extraLen = le16(&cd[p + 30]), commentLen = le16(&cd[p + 32]);
        e.localHeader = le32(&cd[p + 42]);
        if (p + 46 + nameLen + extraLen > cd.size()) break;
        e.name.assign(reinterpret_cast<const char*>(&cd[p + 46]), nameLen);
        // Zip64 sizes/offset live in the extra field when the 32-bit ones are 0xFFFFFFFF.
        size_t x = p + 46 + nameLen, xEnd = x + extraLen;
        while (x + 4 <= xEnd) {
            uint16_t id = le16(&cd[x]), len = le16(&cd[x + 2]);
            if (id == 0x0001) {
                size_t q = x + 4;
                if (e.size == 0xFFFFFFFF && q + 8 <= x + 4 + len) { e.size = le64(&cd[q]); q += 8; }
                if (e.compressedSize == 0xFFFFFFFF && q + 8 <= x + 4 + len) { e.compressedSize = le64(&cd[q]); q += 8; }
                if (e.localHeader == 0xFFFFFFFF && q + 8 <= x + 4 + len) { e.localHeader = le64(&cd[q]); q += 8; }
            }
            x += 4 + len;
        }
        entries_.push_back(e);
        p += 46 + nameLen + extraLen + commentLen;
    }
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.name < b.name; });
    return !entries_.empty();
}

const ApkArchive::Entry* ApkArchive::find(const std::string& name) const
{
    auto it = std::lower_bound(entries_.begin(), entries_.end(), name, [](const Entry& e, const std::string& n) { return e.name < n; });
    return it != entries_.end() && it->name == name ? &*it : nullptr;
}

bool ApkArchive::read(const std::string& name, std::string& out) const
{
    const Entry* e = find(name);
    if (!e) return false;
    std::ifstream f(path_, std::ios::binary);
    unsigned char lh[30];
    if (!f || !readAt(f, e->localHeader, lh, sizeof(lh)) || le32(lh) != 0x04034b50) return false;
    unsigned long long dataOff = e->localHeader + 30 + le16(lh + 26) + le16(lh + 28);
    std::string comp(e->compressedSize, '\0');
    if (e->compressedSize && !readAt(f, dataOff, &comp[0], comp.size())) return false;
    if (e->method == 0) { out.swap(comp); return true; }
    if (e->method != 8) return false;
    out.assign(e->size, '\0');
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) return false;
    zs.next_in = reinterpret_cast<Bytef*>(&comp[0]);
    zs.avail_in = (uInt)comp.size();
    zs.next_out = reinterpret_cast<Bytef*>(out.empty() ? nullptr : &out[0]);
    zs.avail_out = (uInt)out.size();
    int r = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    return (r == Z_STREAM_END || (r == Z_BUF_ERROR && zs.avail_out == 0)) && zs.total_out == e->size;
}

std::vector<std::string> ApkArchive::list(const std::string& folder, bool includeFolders) const
{
    std::string prefix = folder;
    while (!prefix.empty() && prefix.back() == '/') prefix.pop_back();
    if (!prefix.empty()) prefix += "/";
    std::set<std::string> names;
    for (auto it = std::lower_bound(entries_.begin(), entries_.end(), prefix, [](const Entry& e, const std::string& n) { return e.name < n; });
         it != entries_.end() && it->name.compare(0, prefix.size(), prefix) == 0; ++it) {
        std::string rest = it->name.substr(prefix.size());
        size_t slash = rest.find('/');
        if (slash == std::string::npos) { if (!rest.empty()) names.insert(rest); }
        else if (includeFolders) names.insert(rest.substr(0, slash));
    }
    return std::vector<std::string>(names.begin(), names.end());
}

///// The configured APK

static std::once_flag g_apkOnce;
static ApkArchive* g_apk = nullptr;
static std::string g_apkPath;

std::string bogo_apk_path()
{
    bogo_apk();
    return g_apkPath;
}

ApkArchive* bogo_apk()
{
    std::call_once(g_apkOnce, []() {
        std::string p = config["paths"]["apk"].value_or<std::string>("");
        if (p.empty()) return;
        char abs[PATH_MAX];
        if (realpath(p.c_str(), abs)) p = abs; // relative to the game files folder (the working directory)
        auto* a = new ApkArchive();
        if (a->open(p)) {
            g_apk = a;
            g_apkPath = p;
            printf("[APK] %s: %zu entries\n", p.c_str(), a->entries().size());
        } else {
            fprintf(stderr, "[APK] can't read %s as an APK; [paths] apk is ignored\n", p.c_str());
            delete a;
        }
    });
    return g_apk;
}

static bool mkdirs(const std::string& path)
{
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/') {
            std::string part = path.substr(0, i);
            if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) return false;
        }
    }
    return true;
}

std::vector<std::string> bogo_apk_abis()
{
    std::set<std::string> abis;
    if (ApkArchive* apk = bogo_apk()) {
        for (auto& e : apk->entries()) {
            if (e.name.compare(0, 4, "lib/") != 0) continue;
            size_t slash = e.name.find('/', 4);
            if (slash != std::string::npos) abis.insert(e.name.substr(4, slash - 4));
        }
    }
    return std::vector<std::string>(abis.begin(), abis.end());
}

// A new APK replaces the old one's native libraries and managed code, so leftovers (another game's, another ABI's)
// must not stay behind. libc++_shared.so isn't in the APK (it's copied in by hand), so it's kept.
static void clear_stale_runtime_files()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (fs::is_directory(fs::symlink_status("assets", ec)))
        fs::remove_all("assets", ec);
    if (fs::is_directory(fs::symlink_status("lib", ec))) {
        std::vector<fs::path> dirs;
        for (fs::recursive_directory_iterator it("lib", ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code e2;
            if (fs::is_directory(it->symlink_status(e2)))
                dirs.push_back(it->path());
            else if (it->path().filename() != "libc++_shared.so")
                fs::remove(it->path(), e2);
        }
        // Deepest first; a folder that still holds libc++_shared.so isn't empty, so it stays.
        for (auto d = dirs.rbegin(); d != dirs.rend(); ++d)
            fs::remove(*d, ec);
    }
    printf("[APK] new APK: cleared the old assets/ and lib/ (kept any libc++_shared.so)\n");
}

bool bogo_apk_extract_runtime_files()
{
    ApkArchive* apk = bogo_apk();
    if (!apk) return true;
    struct stat as;
    if (stat(apk->path().c_str(), &as) != 0) return false;

    // A stamp of the APK we extracted from: handhelds often have a wrong clock, so file times can't be compared.
    const char* stampFile = ".bogodroid-apk-stamp";
    char stampWanted[512];
    // Size, time and name: a different APK dropped in under another name is new even if size and time happen to match.
    std::string apkName = apk->path().substr(apk->path().rfind('/') + 1);
    snprintf(stampWanted, sizeof(stampWanted), "%lld %lld %s", (long long)as.st_size, (long long)as.st_mtime, apkName.c_str());
    std::string stampHave;
    { std::ifstream in(stampFile); std::getline(in, stampHave); }
    bool sameApk = stampHave == stampWanted;
    if (!sameApk)
        clear_stale_runtime_files();

    const char* prefixes[] = { BOGO_LIB_DIR "/", "assets/bin/Data/Managed/", "assets/dotnet/" };
    int extracted = 0, kept = 0;
    bool ok = true;
    for (auto& e : apk->entries()) {
        if (e.name.empty() || e.name.back() == '/') continue;
        bool wanted = false;
        for (const char* pre : prefixes) if (e.name.compare(0, strlen(pre), pre) == 0) wanted = true;
        if (!wanted) continue;
        struct stat fs;
        if (sameApk && stat(e.name.c_str(), &fs) == 0 && (unsigned long long)fs.st_size == e.size) { kept++; continue; }
        if (extracted == 0) printf("[APK] extracting native libraries and managed code (first run or new APK)...\n");
        std::string data;
        size_t slash = e.name.rfind('/');
        if (!apk->read(e.name, data) || (slash != std::string::npos && !mkdirs(e.name.substr(0, slash)))) {
            fprintf(stderr, "[APK] failed to extract %s\n", e.name.c_str());
            ok = false;
            continue;
        }
        std::string tmp = e.name + ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out.write(data.data(), (std::streamsize)data.size());
            if (!out) { fprintf(stderr, "[APK] failed to write %s\n", e.name.c_str()); ok = false; continue; }
        }
        rename(tmp.c_str(), e.name.c_str());
        extracted++;
    }
    if (ok) { std::ofstream out(stampFile, std::ios::trunc); out << stampWanted << "\n"; }
    printf("[APK] runtime files: %d extracted, %d already up to date\n", extracted, kept);
    return ok;
}

///// C interface

extern "C" int bogo_apk_exists(const char* name)
{
    ApkArchive* apk = bogo_apk();
    return apk && name && apk->find(name) ? 1 : 0;
}

extern "C" int bogo_apk_read(const char* name, void** data, size_t* size)
{
    ApkArchive* apk = bogo_apk();
    std::string s;
    if (!apk || !name || !apk->read(name, s)) return -1;
    *data = malloc(s.size() ? s.size() : 1);
    if (!*data) return -1;
    memcpy(*data, s.data(), s.size());
    *size = s.size();
    return 0;
}

extern "C" char** bogo_apk_list(const char* folder, int* count)
{
    *count = 0;
    ApkArchive* apk = bogo_apk();
    if (!apk || !folder) return nullptr;
    auto names = apk->list(folder, false);
    char** out = (char**)malloc(sizeof(char*) * (names.size() ? names.size() : 1));
    for (auto& n : names) out[(*count)++] = strdup(n.c_str());
    return out;
}
