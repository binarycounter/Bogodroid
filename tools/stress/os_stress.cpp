// Stress test for the Android os emulation: Looper, Handler, Message, HandlerThread and the Thread they run on
// (javastubs/android_os.cpp, javac.cpp). The Choreographer that drives Unity's frames is built on these.
#include "stress_util.h"
#include "android.h"
#include "javac.h"
#include "baron/baron.h"
#include "toml++/toml.hpp"
#include "java_security.h"
#include <string>

// ---- what the code under test needs from the rest of the loader ----
toml::table config;
Baron::Jvm vm;
void request_quit(const char*, bool) { }
std::string bogo_host_time_zone() { return "UTC"; }
void InitJNISecurityClasses(FakeJni::Jvm*) { }
extern "C" void* so_load_library(const char*, void*) { return nullptr; }
std::shared_ptr<jnivm::java::io::File> jnivm::android::content::Context::getExternalFilesDirInternal() { return nullptr; }

using namespace jnivm;
using android::os::Handler;
using android::os::HandlerThread;
using android::os::Looper;
using android::os::Message;
using java::lang::LambdaRunnable;

static std::shared_ptr<HandlerThread> start_thread(const char* name)
{
    auto ht = std::make_shared<HandlerThread>(std::make_shared<FakeJni::JString>(name));
    ht->start();
    return ht;
}

static bool wait_for(const std::function<bool()>& cond, int ms = 10000)
{
    for (int i = 0; i < ms; i++) {
        if (cond()) return true;
        usleep(1000);
    }
    return false;
}

// Many threads post to one looper; every runnable must run exactly once.
static void test_post_from_many_threads(double secs)
{
    Watchdog wd("test_post_from_many_threads", 120);
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        auto ht = start_thread("poster");
        auto handler = std::make_shared<Handler>(ht->getLooper());
        std::atomic<int> ran{0};
        run_threads(6, [&](int i) {
            for (int k = 0; k < 100; k++) {
                if (k % 3 == 0)
                    handler->postDelayed(LambdaRunnable::Create([&]() { ran++; }), k % 4);
                else
                    handler->post(LambdaRunnable::Create([&]() { ran++; }));
            }
        });
        CHECK(wait_for([&]() { return ran == 600; }));
        CHECK(ran == 600);
        ht->quit();
        ht->join();
    }
}

struct CountingHandler : Handler {
    std::atomic<int>* count;
    CountingHandler(std::shared_ptr<Looper> l, std::atomic<int>* c) : Handler(l), count(c) { }
    void handleMessage(std::shared_ptr<Message>) override { ++*count; }
};

// Message.sendToTarget used to queue the message twice.
static void test_send_to_target_once(double)
{
    Watchdog wd("test_send_to_target_once", 60);
    auto ht = start_thread("msg");
    std::atomic<int> handled{0};
    auto handler = std::make_shared<CountingHandler>(ht->getLooper(), &handled);
    for (int i = 0; i < 200; i++) {
        auto m = handler->obtainMessage(i);
        m->sendToTarget();
    }
    CHECK(wait_for([&]() { return handled >= 200; }));
    usleep(50 * 1000); // time for any duplicates to arrive
    CHECK(handled == 200);
    ht->quit();
    ht->join();
}

// An exception in one callback must not stop the looper (the Choreographer's frames would silently stop).
static void test_looper_survives_exceptions(double)
{
    Watchdog wd("test_looper_survives_exceptions", 60);
    auto ht = start_thread("throws");
    auto handler = std::make_shared<Handler>(ht->getLooper());
    std::atomic<int> after{0};
    for (int i = 0; i < 20; i++) {
        handler->post(LambdaRunnable::Create([]() { throw std::runtime_error("boom"); }));
        handler->post(LambdaRunnable::Create([&]() { after++; }));
    }
    CHECK(wait_for([&]() { return after == 20; }));
    ht->quit();
    ht->join();
}

// Messages obtained and recycled from many threads at once, and Java-side recycle() calls on live messages.
static void test_message_pool(double secs)
{
    Watchdog wd("test_message_pool", 60);
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        run_threads(6, [&](int) {
            for (int k = 0; k < 500; k++) {
                auto m = Message::obtain();
                m->what = k;
                m->recycleFromJava(); // Java code calling recycle() must not put a live message in the pool
                CHECK(m->what == k);
            }
        });
    }
}

struct SelfEnding : java::lang::Thread {
    std::atomic<int>* ran;
    SelfEnding(std::atomic<int>* r) : Thread(std::make_shared<FakeJni::JString>("self")), ran(r) { }
    void run() override
    {
        usleep(2000);
        ++*ran;
    }
};

// The game may drop its last reference to a Thread right after start(); the thread must keep itself alive.
static void test_thread_outlives_owner(double secs)
{
    Watchdog wd("test_thread_outlives_owner", 60);
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        std::atomic<int> ran{0};
        for (int i = 0; i < 20; i++) {
            auto t = std::make_shared<SelfEnding>(&ran);
            t->start();
        } // reference dropped here
        CHECK(wait_for([&]() { return ran == 20; }));
    }
}

// HandlerThreads that are never started, or started and quit straight away, must not hang or terminate.
static void test_handlerthread_lifecycle(double secs)
{
    Watchdog wd("test_handlerthread_lifecycle", 120);
    {
        auto never = std::make_shared<HandlerThread>(std::make_shared<FakeJni::JString>("never"));
        auto t0 = std::chrono::steady_clock::now();
        never.reset(); // destructor: quit() must not wait forever for a looper that never comes
        CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(5));
    }
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        for (int i = 0; i < 10; i++) {
            auto ht = start_thread("short");
            if (i % 2) ht->getLooper();
            ht->quit();
            ht->join();
        }
    }
}

int main()
{
    RUN(test_post_from_many_threads);
    RUN(test_send_to_target_once);
    RUN(test_looper_survives_exceptions);
    RUN(test_message_pool);
    RUN(test_thread_outlives_owner);
    RUN(test_handlerthread_lifecycle);
    return g_failures ? 1 : 0;
}
