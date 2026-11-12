#include "manifest.h"
#include "apk.h"
#include "toml++/toml.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <mutex>
#include <vector>

extern toml::table config;

namespace {
    uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
    uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

    constexpr uint16_t RES_XML_TYPE = 0x0003;
    constexpr uint16_t RES_STRING_POOL_TYPE = 0x0001;
    constexpr uint16_t RES_XML_START_ELEMENT_TYPE = 0x0102;
    constexpr uint32_t NO_INDEX = 0xFFFFFFFFu;
    constexpr uint8_t TYPE_STRING = 0x03;

    struct StringPool {
        const uint8_t* chunk = nullptr;
        size_t chunkSize = 0;
        uint32_t count = 0, stringsStart = 0;
        bool utf8 = false;

        bool init(const uint8_t* c, size_t size)
        {
            if (size < 28) return false;
            chunk = c;
            chunkSize = size;
            count = rd32(c + 8);
            utf8 = (rd32(c + 16) & 0x100) != 0;
            stringsStart = rd32(c + 20);
            uint32_t headerSize = rd16(c + 2);
            return (uint64_t)headerSize + (uint64_t)count * 4 <= size && stringsStart < size;
        }

        std::string get(uint32_t index) const
        {
            if (index >= count) return std::string();
            uint32_t headerSize = rd16(chunk + 2);
            uint64_t off = (uint64_t)stringsStart + rd32(chunk + headerSize + (size_t)index * 4);
            if (off + 2 > chunkSize) return std::string();
            const uint8_t* p = chunk + off;
            const uint8_t* end = chunk + chunkSize;
            std::string out;
            if (utf8) {
                // Two lengths (UTF-16 units, then bytes), each 1 or 2 bytes long.
                if (p[0] & 0x80) p += 2; else p += 1;
                if (p >= end) return out;
                size_t len = p[0];
                if (len & 0x80) { len = ((len & 0x7F) << 8) | p[1]; p += 2; } else p += 1;
                if (p + len > end) return out;
                out.assign(reinterpret_cast<const char*>(p), len);
            } else {
                size_t len = rd16(p);
                p += 2;
                if (len & 0x8000) { len = ((len & 0x7FFF) << 16) | rd16(p); p += 2; }
                if (p + len * 2 > end) return out;
                for (size_t i = 0; i < len; i++) {
                    uint32_t c = rd16(p + i * 2);
                    // Surrogate pair
                    if (c >= 0xD800 && c < 0xDC00 && i + 1 < len) {
                        uint32_t lo = rd16(p + (i + 1) * 2);
                        if (lo >= 0xDC00 && lo < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); i++; }
                    }
                    if (c < 0x80) out += (char)c;
                    else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
                    else if (c < 0x10000) { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
                    else { out += (char)(0xF0 | (c >> 18)); out += (char)(0x80 | ((c >> 12) & 0x3F)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
                }
            }
            return out;
        }
    };

    bool parseManifest(const std::string& data, BogoManifestInfo& out)
    {
        const uint8_t* d = reinterpret_cast<const uint8_t*>(data.data());
        size_t size = data.size();
        if (size < 8 || rd16(d) != RES_XML_TYPE) return false;
        size_t total = rd32(d + 4);
        if (total > size) total = size;

        StringPool pool;
        bool havePool = false;
        for (size_t pos = rd16(d + 2); pos + 8 <= total;) {
            uint16_t type = rd16(d + pos);
            uint16_t headerSize = rd16(d + pos + 2);
            uint32_t chunkSize = rd32(d + pos + 4);
            if (chunkSize < 8 || pos + chunkSize > total) break;

            if (type == RES_STRING_POOL_TYPE) {
                havePool = pool.init(d + pos, chunkSize);
            } else if (type == RES_XML_START_ELEMENT_TYPE && havePool && chunkSize >= 36) {
                const uint8_t* c = d + pos;
                std::string element = pool.get(rd32(c + 20));
                uint16_t attrStart = rd16(c + 24), attrSize = rd16(c + 26), attrCount = rd16(c + 28);
                bool wantManifest = element == "manifest", wantSdk = element == "uses-sdk";
                if ((wantManifest || wantSdk) && attrSize >= 20) {
                    for (unsigned i = 0; i < attrCount; i++) {
                        size_t off = 16 + (size_t)attrStart + (size_t)i * attrSize;
                        if (off + 20 > chunkSize) break;
                        const uint8_t* a = c + off;
                        std::string name = pool.get(rd32(a + 4));
                        uint32_t raw = rd32(a + 8);
                        uint8_t dataType = a[15];
                        uint32_t value = rd32(a + 16);
                        auto asString = [&]() { return pool.get(dataType == TYPE_STRING ? value : raw); };
                        auto asInt = [&]() { return dataType == TYPE_STRING || raw != NO_INDEX ? atoi(asString().c_str()) : (int)value; };
                        if (wantManifest) {
                            if (name == "package") out.packageName = asString();
                            else if (name == "versionName") out.versionName = asString();
                            else if (name == "versionCode") out.versionCode = asInt();
                        } else {
                            if (name == "minSdkVersion") out.minSdkVersion = asInt();
                            else if (name == "targetSdkVersion") out.targetSdkVersion = asInt();
                        }
                    }
                }
                // Only <manifest> and <uses-sdk> matter and both are near the top; stop after the <application> element.
                if (element == "application") break;
            }
            (void)headerSize;
            pos += chunkSize;
        }
        return true;
    }
}

const BogoManifestInfo& bogo_manifest()
{
    static BogoManifestInfo info;
    static std::once_flag once;
    std::call_once(once, []() {
        std::string data;
        if (ApkArchive* apk = bogo_apk())
            apk->read("AndroidManifest.xml", data);
        if (data.empty()) {
            std::ifstream f("AndroidManifest.xml", std::ios::binary);
            if (f) data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        }
        if (!data.empty() && parseManifest(data, info)) {
            info.loaded = true;
            printf("[Manifest] package=%s versionName=%s versionCode=%d minSdk=%d targetSdk=%d\n", info.packageName.c_str(),
                   info.versionName.c_str(), info.versionCode, info.minSdkVersion, info.targetSdkVersion);
        } else if (!data.empty()) {
            fprintf(stderr, "[Manifest] AndroidManifest.xml isn't readable binary XML; ignored\n");
        }
    });
    return info;
}

std::string bogo_package_version_name(const char* fallback)
{
    std::string v = config["package"]["versionName"].value_or<std::string>("");
    if (!v.empty()) return v;
    v = bogo_manifest().versionName;
    return v.empty() ? std::string(fallback) : v;
}

int bogo_package_version_code()
{
    return config["package"]["versionCode"].value_or<int>(int(bogo_manifest().versionCode));
}

int bogo_min_sdk_version()
{
    return config["package"]["minSdkVersion"].value_or<int>(int(bogo_manifest().minSdkVersion));
}

int bogo_target_sdk_version()
{
    return config["package"]["targetSdkVersion"].value_or<int>(int(bogo_manifest().targetSdkVersion));
}
