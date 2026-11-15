// Stress test for the native ALooper emulation (thunks/ndk/alooper.c).
#include "stress_util.h"
#include "alooper.h"
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <cstdint>
#include <unistd.h>

static void poke(int fd)
{
    uint64_t v = 1;
    (void)!write(fd, &v, 8);
}

static int cb_remove_self(int fd, int, void* data)
{
    uint64_t v;
    (void)!read(fd, &v, 8);
    ++*(std::atomic<int>*)data;
    return 0; // 0 = unregister me
}

static ALooper* g_for_cb;
static int g_second_fd;
static int cb_adds_fd(int fd, int, void* data)
{
    uint64_t v;
    (void)!read(fd, &v, 8);
    ++*(std::atomic<int>*)data;
    // A callback may register and unregister descriptors on its own looper.
    ALooper_addFd(g_for_cb, g_second_fd, 0, ALOOPER_EVENT_INPUT, cb_remove_self, data);
    ALooper_removeFd(g_for_cb, fd);
    return 1;
}

// The callback used to run with the looper's (non-recursive) mutex held, so removing itself deadlocked.
static void test_callbacks_reenter(double)
{
    Watchdog wd("test_callbacks_reenter", 30);
    std::thread([]() {
        ALooper* l = ALooper_prepare(0);
        std::atomic<int> count{0};
        int efd = eventfd(0, EFD_NONBLOCK);
        CHECK(ALooper_addFd(l, efd, 0, ALOOPER_EVENT_INPUT, cb_remove_self, &count) == 1);
        poke(efd);
        CHECK(ALooper_pollOnce(1000, nullptr, nullptr, nullptr) == ALOOPER_POLL_CALLBACK);
        CHECK(count == 1);
        poke(efd);
        CHECK(ALooper_pollOnce(20, nullptr, nullptr, nullptr) == ALOOPER_POLL_TIMEOUT); // it removed itself
        close(efd);

        int a = eventfd(0, EFD_NONBLOCK);
        g_second_fd = eventfd(0, EFD_NONBLOCK);
        g_for_cb = l;
        count = 0;
        CHECK(ALooper_addFd(l, a, 0, ALOOPER_EVENT_INPUT, cb_adds_fd, &count) == 1);
        poke(a);
        CHECK(ALooper_pollOnce(1000, nullptr, nullptr, nullptr) == ALOOPER_POLL_CALLBACK);
        poke(g_second_fd);
        CHECK(ALooper_pollOnce(1000, nullptr, nullptr, nullptr) == ALOOPER_POLL_CALLBACK);
        CHECK(count == 2);
        close(a);
        close(g_second_fd);
    }).join();
}

// A failed addFd must not leave a dangling entry in the fd list.
static void test_addfd_failure(double)
{
    std::thread([]() {
        ALooper* l = ALooper_prepare(0);
        for (int i = 0; i < 100; i++)
            CHECK(ALooper_addFd(l, -1, 0, ALOOPER_EVENT_INPUT, nullptr, nullptr) == -1);
        int efd = eventfd(0, EFD_NONBLOCK);
        CHECK(ALooper_addFd(l, efd, 7, ALOOPER_EVENT_INPUT, nullptr, (void*)0x1234) == 1);
        poke(efd);
        int fd = 0, ev = 0;
        void* data = nullptr;
        CHECK(ALooper_pollOnce(1000, &fd, &ev, &data) == 7);
        CHECK(fd == efd && data == (void*)0x1234);
        CHECK(ALooper_removeFd(l, efd) == 1);
        close(efd);
    }).join();
}

// Other threads wake a looper while its own thread goes away; with a reference held that must stay valid.
static void test_wake_after_owner_exit(double secs)
{
    Watchdog wd("test_wake_after_owner_exit", 60);
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        std::atomic<ALooper*> shared{nullptr};
        std::thread owner([&]() {
            ALooper* l = ALooper_prepare(0);
            ALooper_acquire(l);
            shared = l;
            ALooper_pollOnce(5, nullptr, nullptr, nullptr);
        }); // thread exit releases the TLS reference
        while (!shared) { }
        ALooper* l = shared;
        for (int i = 0; i < 200; i++)
            ALooper_wake(l);
        owner.join();
        for (int i = 0; i < 200; i++)
            ALooper_wake(l); // owner is gone, our reference keeps it alive
        ALooper_release(l);
    }
}

// Many threads, each with its own looper, woken from everywhere.
static void test_many_loopers(double secs)
{
    Watchdog wd("test_many_loopers", 60);
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        run_threads(8, [&](int) {
            ALooper* l = ALooper_prepare(0);
            for (int i = 0; i < 100; i++) {
                ALooper_wake(l);
                ALooper_pollOnce(10, nullptr, nullptr, nullptr);
            }
        });
    }
}

int main()
{
    RUN(test_callbacks_reenter);
    RUN(test_addfd_failure);
    RUN(test_wake_after_owner_exit);
    RUN(test_many_loopers);
    return g_failures ? 1 : 0;
}
