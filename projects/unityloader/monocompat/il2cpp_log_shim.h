// To generate the C++ file for this, place the il2cpp folder from your Unity installation in projects/unityloader/il2cpp and run tools/gen_il2cpp_log_shim.py

#pragma once
#include "so_util.h"

void il2cpp_log_shim_init(so_module* mod, DynLibFunction* fallback_table);
extern DynLibFunction symtable_il2cpp_log[];
extern DynLibFunction* so_dynamic_libraries[];
