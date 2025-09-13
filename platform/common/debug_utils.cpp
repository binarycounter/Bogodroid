#include "debug_utils.h"

#include "gles2.h"
#include "glad.h"
#include "glad_egl.h"

#include <iostream>
#include <csignal>
#include <execinfo.h>
#include <unistd.h>

void print_native_callbacks(ANativeActivity nActivity)
{
    printf("onStart: %p\n", (void*)nActivity.callbacks->onStart);
    printf("onResume: %p\n", (void*)nActivity.callbacks->onResume);
    printf("onSaveInstanceState: %p\n", (void*)nActivity.callbacks->onSaveInstanceState);
    printf("onPause: %p\n", (void*)nActivity.callbacks->onPause);
    printf("onStop: %p\n", (void*)nActivity.callbacks->onStop);
    printf("onDestroy: %p\n", (void*)nActivity.callbacks->onDestroy);
    printf("onWindowFocusChanged: %p\n", (void*)nActivity.callbacks->onWindowFocusChanged);
    printf("onNativeWindowCreated: %p\n", (void*)nActivity.callbacks->onNativeWindowCreated);
    printf("onNativeWindowResized: %p\n", (void*)nActivity.callbacks->onNativeWindowResized);
    printf("onNativeWindowRedrawNeeded: %p\n", (void*)nActivity.callbacks->onNativeWindowRedrawNeeded);
    printf("onNativeWindowDestroyed: %p\n", (void*)nActivity.callbacks->onNativeWindowDestroyed);
    printf("onInputQueueCreated: %p\n", (void*)nActivity.callbacks->onInputQueueCreated);
    printf("onInputQueueDestroyed: %p\n", (void*)nActivity.callbacks->onInputQueueDestroyed);
    printf("onContentRectChanged: %p\n", (void*)nActivity.callbacks->onContentRectChanged);
    printf("onConfigurationChanged: %p\n", (void*)nActivity.callbacks->onConfigurationChanged);
    printf("onLowMemory: %p\n", (void*)nActivity.callbacks->onLowMemory);
}

void segfault_handler(int signal) {
    void *array[50];  // Array to store stack trace addresses
    size_t size;

    // Get the stack trace
    size = backtrace(array, 50);

    // Print the stack trace
    std::cerr << "Error: signal " << signal << ":\n";
    backtrace_symbols_fd(array, size, STDERR_FILENO);

    // Exit the program
    exit(1);
}

void exit_handler(int signal) {
    printf("Caught signal %d, exiting...\n", signal);
    exit(0);
}

void print_backtrace_on_segfault()
{
    signal(SIGSEGV, segfault_handler);
}

void exit_on_signals()
{
    struct sigaction sa;
    sa.sa_handler = exit_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}