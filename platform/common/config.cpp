#include "toml++/toml.hpp"
#include <filesystem>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <ctype.h>
#include "config.h"

extern toml::table config;

static std::string g_apk_problem;
const std::string& config_apk_problem() { return g_apk_problem; }

namespace {
    namespace fs = std::filesystem;

    // The [section] table of the config, created (or replaced, if the user wrote a non-table there) when missing.
    toml::table& section(const char* name)
    {
        if (auto* t = config[name].as_table())
            return *t;
        config.insert_or_assign(name, toml::table{});
        return *config[name].as_table();
    }

    // Sets a default; leaves it alone when the user's file already has a value for that key.
    // Returns true when the default was applied.
    template <typename T>
    bool set_default(const char* sec, const char* key, T value)
    {
        toml::table& t = section(sec);
        if (t.contains(key))
            return false;
        t.insert(key, value);
        return true;
    }

    bool is_dir(const std::string& p)
    {
        struct stat st;
        return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }

    bool is_file(const std::string& p)
    {
        struct stat st;
        return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }

    void make_dirs(const std::string& p)
    {
        std::error_code ec;
        fs::create_directories(p, ec);
    }
}

// Reads the config and fills in defaults for everything it doesn't set, so an empty file, a broken file or no file at
// all still gives a loadable setup. The layout it assumes (and which the user's own file can override key by key):
//
//   <folder the loader is started from>/   game.toml (optional), splash.bmp (optional)
//       gamedata/                          the game files: one .apk (any name), lib/, assets/ ... (the working directory)
//       conf/                              saves and preferences (Application.persistentDataPath)
//       cache/
//
// Without a gamedata/ folder, the folder the loader is started from is used as the game files folder.
bool init_config(const char* config_path)
{
    std::string startDir;
    {
        char d[4096];
        if (getcwd(d, sizeof(d)))
            startDir = d;
    }

    // Which file: the one given, else game.toml next to where we're started, else none.
    std::string path = config_path ? config_path : "";
    if (path.empty() && is_file("game.toml"))
        path = "game.toml";

    config = toml::table{};
    if (!path.empty()) {
        if (!is_file(path)) {
            std::cerr << "Note: config file " << path << " not found; using defaults" << std::endl;
        } else {
            try {
                config = toml::parse_file(path);
            } catch (const std::exception& e) {
                std::cerr << "Warning: could not read config " << path << " (" << e.what() << "); using defaults" << std::endl;
                config = toml::table{};
            }
        }
    }

    // [paths]
    std::string gameDefault = is_dir("./gamedata") ? "./gamedata/" : "./";
    set_default("paths", "game_files", gameDefault);
    set_default("paths", "support_files", config["paths"]["game_files"].value_or<std::string>(std::string(gameDefault)));
    set_default("paths", "android_package_code", std::string("./"));
    if (set_default("paths", "android_files", startDir + "/conf/"))
        make_dirs(startDir + "/conf/");
    if (set_default("paths", "android_external_files", startDir + "/conf/"))
        make_dirs(startDir + "/conf/");
    if (set_default("paths", "android_cache", startDir + "/cache/"))
        make_dirs(startDir + "/cache/");

    auto game_path = config["paths"]["game_files"].value_or<std::string>(std::string(gameDefault));
    if (chdir(game_path.c_str()) != 0) {
        std::cerr << "Could not change directory to " << game_path << std::endl;
        return false;
    }
    std::cout << "Changed working directory to " << game_path << std::endl;

    // Now in the game files folder: with no [paths] apk, the one .apk file in it (any name) is the game's APK.
    // None is fine (the game files may be extracted already); more than one is ambiguous and reported as a problem.
    if (!config["paths"].as_table()->contains("apk")) {
        std::vector<std::string> apks;
        std::error_code ec;
        for (fs::directory_iterator it(".", ec), end; !ec && it != end; it.increment(ec)) {
            std::string name = it->path().filename().string();
            std::string ext = it->path().extension().string();
            for (auto& c : ext) c = (char)tolower((unsigned char)c);
            std::error_code e2;
            if (ext == ".apk" && fs::is_regular_file(it->path(), e2)) // follows symlinks
                apks.push_back(name);
        }
        std::sort(apks.begin(), apks.end());
        if (apks.size() == 1) {
            section("paths").insert("apk", "./" + apks[0]);
            std::cout << "Using " << apks[0] << " as the game's APK" << std::endl;
        } else if (apks.size() > 1) {
            std::string list;
            for (auto& a : apks) list += (list.empty() ? "" : ", ") + a;
            g_apk_problem = "found " + std::to_string(apks.size()) + " .apk files in " + game_path + " (" + list +
                            "); keep only one there, or set [paths] apk to choose";
            std::cerr << "Error: " << g_apk_problem << std::endl;
        } else {
            std::cout << "Note: no .apk file in " << game_path << "; using the game files as they are" << std::endl;
        }
    }

    // [device] splash: a splash.bmp next to where the loader was started. (Relative paths are taken from there.)
    if (!section("device").contains("splash") && is_file(startDir + "/splash.bmp"))
        section("device").insert("splash", std::string("splash.bmp"));

    return true;
}
