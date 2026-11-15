// A dependency of libprobeplugin.so, to check that the loader loads a plugin's DT_NEEDED libraries first.
__attribute__((visibility("default"))) int probe_dep_value(void) { return 42; }
