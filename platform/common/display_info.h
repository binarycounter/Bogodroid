#pragma once

// Display geometry shared by the Java stubs, the NDK stubs, the input backend and the SDL window.
//
// [device] displayWidth / displayHeight in the config win when both are set (and the window is created at that size,
// as before). When they are not set, the real desktop resolution is detected via SDL and the window is created
// fullscreen, so the game fits whatever screen it runs on. [device] fullscreen = true/false overrides the window mode.
int display_get_width();
int display_get_height();
bool display_use_fullscreen();
