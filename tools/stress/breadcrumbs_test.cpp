// Checks the crash handler's breadcrumb trail: order, wrap-around, truncation, concurrent writers.
#include "stress_util.h"
#include "breadcrumbs.h"
#include <cstring>
#include <string>
#include <sys/wait.h>

static std::string dump()
{
    int fds[2];
    if (pipe(fds) != 0) return "";
    bogo_breadcrumbs_dump(fds[1]);
    close(fds[1]);
    std::string out;
    char buf[4096];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0) out.append(buf, n);
    close(fds[0]);
    return out;
}

static void test_order_and_wrap(double)
{
    for (int i = 0; i < 300; i++) bogo_breadcrumb("event", "n", i);
    std::string d = dump();
    CHECK(d.find("event n 299") != std::string::npos);   // newest is there
    CHECK(d.find("event n 100") == std::string::npos);   // 300 - 128 = 172 oldest kept; 100 was overwritten
    CHECK(d.find("event n 180") != std::string::npos);
    CHECK(d.find("event n 180") < d.find("event n 299")); // oldest first
}

static void test_truncation_and_null(double)
{
    bogo_breadcrumb("no detail");
    bogo_breadcrumb(nullptr, nullptr, 0);
    bogo_breadcrumb("long detail", "0123456789012345678901234567890123456789012345678901234567890123456789");
    std::string d = dump();
    CHECK(d.find("no detail") != std::string::npos);
    CHECK(d.find("long detail 012345678901234567890123456789012345678") != std::string::npos);   // 39 characters kept
    CHECK(d.find("0123456789012345678901234567890123456789\n") == std::string::npos);            // the 40th is cut
}

// Writers on many threads while another thread dumps, as in a crash while the game is running.
static void test_concurrent(double secs)
{
    Watchdog wd("test_concurrent", 60);
    auto start = std::chrono::steady_clock::now();
    std::atomic<bool> stop{false};
    std::thread dumper([&]() {
        while (!stop) {
            std::string d = dump();
            CHECK(d.size() < 100000);
        }
    });
    while (time_left(start, secs)) {
        run_threads(6, [&](int i) {
            for (int k = 0; k < 2000; k++) bogo_breadcrumb("concurrent", i % 2 ? "odd" : "even", k);
        });
    }
    stop = true;
    dumper.join();
}

int main()
{
    RUN(test_order_and_wrap);
    RUN(test_truncation_and_null);
    RUN(test_concurrent);
    return g_failures ? 1 : 0;
}
