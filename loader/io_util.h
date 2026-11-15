#pragma once
#include "so_util.h"

// optional: the file may legitimately be absent (a mod loader, a helper plugin, ...); say so as a Note, not an Error.
bool load_so_from_file(so_module *mod, const char *filename, uintptr_t addr, bool optional = false);