#pragma once

#include "anative_activity.h"
void print_native_callbacks(ANativeActivity nActivity);
void print_backtrace_on_segfault();
void exit_on_signals();
// Build identification for logs and crash reports: git revision (when built from a checkout) and build time.
const char* bogo_build_id();
