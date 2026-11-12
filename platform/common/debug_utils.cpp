#include "debug_utils.h"
#include "logging.h"

#include "gles2.h"
#include "glad.h"
#include "glad_egl.h"

#include <iostream>
#include <csignal>
#include <execinfo.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <spawn.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "javac.h"

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

    // Exit immediately: exit() runs atexit/TLS destructors which can abort again and hang on a black screen
    _exit(1);
}

// Milliseconds since the process started (this file's static init is about as early as it gets).
long bogo_uptime_ms()
{
    static const auto t0 = std::chrono::steady_clock::now();
    return (long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
}
static const long g_uptime_init = bogo_uptime_ms(); // starts the clock at load time

extern char** environ;

[[noreturn]] void bogo_exit(int status)
{
    // sync() waits for every dirty page on the system to be written; with a big log on a slow SD card that took from
    // seconds to minutes, and the window stayed up the whole time. Run it in a child the shell doesn't wait for: the
    // data is already in the page cache, so it reaches the card whether we're still around or not.
    printf("[+%ldms] Exiting (status %d); pending writes are synced in the background\n", bogo_uptime_ms(), status);
    fflush(NULL);
    static std::atomic<bool> leaving{false};
    if (!leaving.exchange(true)) {
        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);
        for (int fd = 0; fd <= 2; fd++)
            posix_spawn_file_actions_addopen(&fa, fd, "/dev/null", fd == 0 ? O_RDONLY : O_WRONLY, 0);
        pid_t pid;
        char arg0[] = "sync";
        char* argv[] = { arg0, nullptr };
        if (posix_spawnp(&pid, "sync", &fa, nullptr, argv, environ) != 0)
            sync(); // no sync binary: fall back to doing it ourselves
        posix_spawn_file_actions_destroy(&fa);
    }
    _exit(status);
}

void exit_handler(int signal) {
    // A second signal means "really, now".
    if (g_quit_requested.exchange(true))
        _exit(0);
    static const char msg[] = "Caught signal, shutting down...\n";
    ssize_t ignored = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)ignored;
    // The main loop notices g_quit_requested; the monitor thread started in exit_on_signals() force-exits if it doesn't.
}

void print_backtrace_on_segfault()
{
    signal(SIGSEGV, segfault_handler);
    signal(SIGTRAP, segfault_handler);
    signal(SIGABRT, segfault_handler);
    signal(SIGBUS, segfault_handler);
    signal(SIGILL, segfault_handler);
    signal(SIGFPE, segfault_handler);
}

void exit_on_signals()
{
    // Quit monitor: exit() from a signal handler runs destructors that can abort or hang (guest threads are still
    // running), so the handler only sets a flag, and this thread leaves for good if nothing exits cleanly in time.
    std::thread([]() {
        while (!g_quit_requested)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        bogo_exit(0);
    }).detach();

    struct sigaction sa;
    sa.sa_handler = exit_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}
