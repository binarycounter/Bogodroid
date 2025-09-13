#ifndef __LOGGING_H__
#define __LOGGING_H__

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