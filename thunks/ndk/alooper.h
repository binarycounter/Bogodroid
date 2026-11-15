#ifndef ALOOPER_H
#define ALOOPER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ALOOPER_PREPARE_ALLOW_NON_CALLBACKS = 1<<0
} ALooperPrepareOptions;

typedef enum {
    ALOOPER_POLL_WAKE = -1,
    ALOOPER_POLL_CALLBACK = -2,
    ALOOPER_POLL_TIMEOUT = -3,
    ALOOPER_POLL_ERROR = -4
} ALooperPollResult;

typedef enum {
    ALOOPER_EVENT_INPUT = 1 << 0,
    ALOOPER_EVENT_OUTPUT = 1 << 1,
    ALOOPER_EVENT_ERROR = 1 << 2,
    ALOOPER_EVENT_HANGUP = 1 << 3,
    ALOOPER_EVENT_INVALID = 1 << 4
} ALooperEventFlags;

typedef struct ALooper ALooper;
typedef int (*ALooper_callbackFunc)(int fd, int events, void* data);

// API functions
ALooper* ALooper_prepare(int opts);
ALooper* ALooper_forThread(void);
void ALooper_acquire(ALooper* looper);
void ALooper_release(ALooper* looper);
int ALooper_addFd(ALooper* looper, int fd, int ident, int events,
                 ALooper_callbackFunc callback, void* data);
int ALooper_removeFd(ALooper* looper, int fd);
void ALooper_wake(ALooper* looper);
int ALooper_pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData);
int ALooper_pollAll(int timeoutMillis, int* outFd, int* outEvents, void** outData);

#ifdef __cplusplus
}
#endif

#endif // ALOOPER_H