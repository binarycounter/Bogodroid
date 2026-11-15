// Small helpers shared by the stress tests.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>
#include <vector>
#include <unistd.h>

// How long each test keeps hammering (seconds), STRESS_SECONDS in the environment.
inline double stress_seconds()
{
    const char* e = getenv("STRESS_SECONDS");
    return e ? atof(e) : 2.0;
}

inline bool time_left(std::chrono::steady_clock::time_point start, double secs)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < secs;
}

static std::atomic<int> g_failures{0};

#define CHECK(cond)                                                                              \
    do {                                                                                         \
        if (!(cond)) {                                                                           \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                      \
            g_failures++;                                                                        \
        }                                                                                        \
    } while (0)

// Kills the process with a clear message when a test doesn't finish: deadlocks and lost wakeups show up as hangs.
struct Watchdog {
    std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);
    Watchdog(const char* name, int seconds)
    {
        auto d = done;
        std::string n = name;
        std::thread([d, n, seconds]() {
            for (int i = 0; i < seconds * 10 && !*d; i++)
                usleep(100000);
            if (!*d) {
                fprintf(stderr, "FAIL: %s hung for %d s (deadlock or lost wakeup)\n", n.c_str(), seconds);
                _exit(2);
            }
        }).detach();
    }
    ~Watchdog() { *done = true; }
};

// Starts n threads together (so they really collide) and waits for them.
inline void run_threads(int n, const std::function<void(int)>& fn)
{
    std::atomic<bool> go{false};
    std::atomic<int> ready{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < n; i++)
        ts.emplace_back([&, i]() {
            ready++;
            while (!go) { }
            fn(i);
        });
    while (ready < n) { }
    go = true;
    for (auto& t : ts)
        t.join();
}

#define RUN(test)                                                      \
    do {                                                               \
        int before = g_failures;                                       \
        test(stress_seconds());                                        \
        printf("%-28s %s\n", #test, g_failures == before ? "ok" : "FAILED"); \
    } while (0)
