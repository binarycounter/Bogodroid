#pragma once
#include "glad_egl.h"

void sdl_initialize_gles();
void* getProc(const char* sym);

// Creates the shared SDL window + GL context if they don't exist yet. Safe to call repeatedly.
bool sdl_ensure_window();
// Shows a static BMP on the window (letterboxed) so the screen isn't black while the game loads.
// Creates the window early; the context is released afterwards so Unity's render thread can take it.
void splash_show(const char* bmpPath);
extern bool g_splash_shown;
