#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <stdio.h>
#include <stdlib.h>

// Chatty diagnostics (dlopen/dlsym traces, JNI type checks, GL extension list, ...) are off by default.
// Set BOGODROID_VERBOSE=1 in the environment to turn them on, BOGODROID_FPS=1 for just the FPS counter.
static inline int bogo_verbose() { static int v = -1; if (v < 0) v = getenv("BOGODROID_VERBOSE") ? 1 : 0; return v; }
static inline int bogo_show_fps() { static int v = -1; if (v < 0) v = (getenv("BOGODROID_FPS") || getenv("BOGODROID_VERBOSE")) ? 1 : 0; return v; }
// Milliseconds since startup, for timing lines in the log.
long bogo_uptime_ms();
#define bogo_log(...) do { if (bogo_verbose()) printf(__VA_ARGS__); } while (0)

#ifdef DEBUG
    #define fatal_error(msg, ...) { fprintf(stderr, "%s:%d: " msg, __FILE__, __LINE__, ##__VA_ARGS__); }
    #define warning(msg, ...) { fprintf(stderr, msg, ##__VA_ARGS__); }
    #define WARN_STUB fprintf(stderr, "Warning, stubbed function \"%s\".\n", __FUNCTION__);
#else
    #define fatal_error(msg, ...) { fprintf(stderr, "%s:%d: " msg, __FILE__, __LINE__, ##__VA_ARGS__); }
    #define warning(msg, ...) { fprintf(stderr, msg, ##__VA_ARGS__); }
    #define WARN_STUB
#endif

#ifdef VERBOSE_LOG
#define verbose(tag, format, ...) printf("[" tag "] " format "\n", ##__VA_ARGS__)
#else
#define verbose(tag, format, ...)
#endif

#endif