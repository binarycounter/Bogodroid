#include "display_info.h"
#include "SDL2/SDL.h"
#include <mutex>

#include "toml++/toml.hpp"
extern toml::table config;

static std::once_flag g_once;
static int g_width = 640;
static int g_height = 480;
static bool g_configured = false;

static void detect()
{
    auto w = config["device"]["displayWidth"].value<int>();
    auto h = config["device"]["displayHeight"].value<int>();
    if (w && h && *w > 0 && *h > 0) {
        g_width = *w;
        g_height = *h;
        g_configured = true;
        return;
    }

    // Not configured: ask SDL for the real screen size (falls back to 640x480 if that fails).
    SDL_DisplayMode dm;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0 && SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
        g_width = dm.w;
        g_height = dm.h;
    }
}

int display_get_width()
{
    std::call_once(g_once, detect);
    return g_width;
}

int display_get_height()
{
    std::call_once(g_once, detect);
    return g_height;
}

bool display_use_fullscreen()
{
    std::call_once(g_once, detect);
    return config["device"]["fullscreen"].value_or<bool>(!g_configured);
}
