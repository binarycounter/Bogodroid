// Stress test for the bionic pthread / semaphore shims (thunks/libc/pthread.cpp, semaphores.cpp).
// These are what the game's threads use for every lock, so a race here is a random crash after minutes.
#include "stress_util.h"
#include "platform.h"
#include "thunk_pthread.h"
#include <cerrno>
#include <cstring>
#include <memory>
#include <semaphore.h>
#include <time.h>

ABI_ATTR int pthread_mutex_init_impl(BIONIC_pthread_mutex_t*, pthread_mutexattr_t**);
ABI_ATTR int pthread_mutex_destroy_impl(BIONIC_pthread_mutex_t*);
ABI_ATTR int pthread_mutex_lock_impl(BIONIC_pthread_mutex_t*);
ABI_ATTR int pthread_mutex_unlock_impl(BIONIC_pthread_mutex_t*);
ABI_ATTR int pthread_mutex_trylock_impl(BIONIC_pthread_mutex_t*);
ABI_ATTR int pthread_cond_wait_impl(pthread_cond_t**, BIONIC_pthread_mutex_t*);
ABI_ATTR int pthread_cond_timedwait_impl(pthread_cond_t**, BIONIC_pthread_mutex_t*, const struct timespec*);
ABI_ATTR int pthread_cond_broadcast_impl(pthread_cond_t**);
ABI_ATTR int pthread_cond_signal_impl(pthread_cond_t**);
ABI_ATTR int pthread_cond_destroy_impl(pthread_cond_t**);
ABI_ATTR int pthread_once_impl(volatile int*, void (*)(void));
ABI_ATTR int pthread_mutexattr_init_impl(pthread_mutexattr_t**);
ABI_ATTR int pthread_mutexattr_settype_impl(pthread_mutexattr_t**, int);
ABI_ATTR int pthread_mutexattr_destroy_impl(pthread_mutexattr_t**);
int pthread_create_impl(pthread_t*, const BIONIC_pthread_attr_t*, void* (*)(void*), void*);
int pthread_attr_init_impl(BIONIC_pthread_attr_t*);
int pthread_attr_setdetachstate_impl(BIONIC_pthread_attr_t*, int);

typedef struct {
    uint32_t count;
} BIONIC_sem_t;
int sem_init_impl(BIONIC_sem_t*, int, unsigned);
int sem_destroy_impl(BIONIC_sem_t*);
int sem_wait_impl(BIONIC_sem_t*);
int sem_post_impl(BIONIC_sem_t*);

// Bionic's pthread_mutex_t only needs 4-byte alignment, so guests embed it at offsets that are not a multiple of 8;
// half of the mutexes here are deliberately misplaced like that (ARM64 faults on misaligned atomics).
static BIONIC_pthread_mutex_t* new_zero_mutex()
{
    static std::atomic<int> n{0};
    char* raw = (char*)calloc(1, 48);
#ifdef __SANITIZE_THREAD__
    return (BIONIC_pthread_mutex_t*)raw; // TSan would (rightly) call the fence-based misaligned path a data race
#else
    return (BIONIC_pthread_mutex_t*)(raw + (n++ % 2 ? 4 : 0));
#endif
}

static void free_zero_mutex(void* p) { free((void*)((uintptr_t)p & ~(uintptr_t)15)); } // calloc blocks are 16-aligned

// Many threads take a never-initialised mutex for the first time at once; the counter must not lose updates.
static void test_mutex_lazy_init(double secs)
{
    Watchdog wd("test_mutex_lazy_init", 60);
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        auto* m = new_zero_mutex();
        int counter = 0;
        run_threads(8, [&](int) {
            for (int i = 0; i < 300; i++) {
                pthread_mutex_lock_impl(m);
                counter++;
                pthread_mutex_unlock_impl(m);
            }
        });
        CHECK(counter == 8 * 300);
        pthread_mutex_destroy_impl(m);
        free_zero_mutex(m);
    }
}

// Bionic's static PTHREAD_RECURSIVE_MUTEX_INITIALIZER has 0x4000 in the first word (error-checking: 0x8000).
static void test_static_recursive(double)
{
    auto* r = new_zero_mutex();
    { uintptr_t v = 0x4000; memcpy(r, &v, sizeof(v)); }
    CHECK(pthread_mutex_lock_impl(r) == 0);
    CHECK(pthread_mutex_trylock_impl(r) == 0); // recursive: the owner can lock again
    CHECK(pthread_mutex_unlock_impl(r) == 0);
    CHECK(pthread_mutex_unlock_impl(r) == 0);
    pthread_mutex_destroy_impl(r);
    free_zero_mutex(r);

    auto* e = new_zero_mutex();
    { uintptr_t v = 0x8000; memcpy(e, &v, sizeof(v)); }
    CHECK(pthread_mutex_lock_impl(e) == 0);
    CHECK(pthread_mutex_trylock_impl(e) == EBUSY);
    CHECK(pthread_mutex_unlock_impl(e) == 0);
    pthread_mutex_destroy_impl(e);
    free_zero_mutex(e);
}

static void test_mutexattr(double)
{
    pthread_mutexattr_t* attr = nullptr;
    CHECK(pthread_mutexattr_init_impl(&attr) == 0);
    CHECK(pthread_mutexattr_settype_impl(&attr, PTHREAD_MUTEX_RECURSIVE) == 0);
    auto* m = new_zero_mutex();
    CHECK(pthread_mutex_init_impl(m, &attr) == 0);
    CHECK(pthread_mutexattr_destroy_impl(&attr) == 0);
    CHECK(pthread_mutexattr_destroy_impl(&attr) == 0); // a second destroy must be harmless
    CHECK(pthread_mutex_lock_impl(m) == 0);
    CHECK(pthread_mutex_trylock_impl(m) == 0);
    pthread_mutex_unlock_impl(m);
    pthread_mutex_unlock_impl(m);
    pthread_mutex_destroy_impl(m);
    free_zero_mutex(m);
    pthread_mutexattr_t* none = nullptr;
    CHECK(pthread_mutexattr_settype_impl(&none, 1) != 0); // uninitialised attr: error, not a crash
}

static std::atomic<int> g_once_data{0};
static std::atomic<int> g_once_runs{0};
static void once_init()
{
    g_once_runs++;
    usleep(500);
    g_once_data = 42;
}
static volatile int g_rec_ctl = 0;
static void once_recursive() { pthread_once_impl(&g_rec_ctl, once_recursive); }

// Every caller must see the initialiser's effects, and it must run exactly once.
static void test_once(double secs)
{
    Watchdog wd("test_once", 60);
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        volatile int ctl = 0;
        g_once_data = 0;
        g_once_runs = 0;
        run_threads(8, [&](int) {
            pthread_once_impl(&ctl, once_init);
            CHECK(g_once_data == 42);
        });
        CHECK(g_once_runs == 1);
    }
    pthread_once_impl(&g_rec_ctl, once_recursive); // must not deadlock on itself
}

struct CondShared {
    BIONIC_pthread_mutex_t m;
    char pad[32];
    pthread_cond_t* c;
    bool flag;
};

// Waiters and the broadcaster all touch a never-initialised cond at once: they must end up on the same cond.
static void test_cond_lazy_init(double secs)
{
    Watchdog wd("test_cond_lazy_init", 60);
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        auto* s = (CondShared*)calloc(1, sizeof(CondShared));
        run_threads(6, [&](int i) {
            if (i == 0) {
                pthread_mutex_lock_impl(&s->m);
                s->flag = true;
                pthread_mutex_unlock_impl(&s->m);
                pthread_cond_broadcast_impl(&s->c);
            } else {
                pthread_mutex_lock_impl(&s->m);
                while (!s->flag)
                    pthread_cond_wait_impl(&s->c, &s->m);
                pthread_mutex_unlock_impl(&s->m);
            }
        });
        pthread_cond_destroy_impl(&s->c);
        pthread_mutex_destroy_impl(&s->m);
        free(s);
    }
}

static void test_cond_timedwait(double)
{
    auto* s = (CondShared*)calloc(1, sizeof(CondShared));
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 20 * 1000 * 1000;
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
    pthread_mutex_lock_impl(&s->m);
    int r = pthread_cond_timedwait_impl(&s->c, &s->m, &ts);
    pthread_mutex_unlock_impl(&s->m);
    CHECK(r == ETIMEDOUT);
    pthread_cond_destroy_impl(&s->c);
    pthread_mutex_destroy_impl(&s->m);
    free(s);
}

// Semaphores on the stack of many threads, created and destroyed constantly, plus one shared producer/consumer pair.
static void test_semaphores(double secs)
{
    Watchdog wd("test_semaphores", 60);
    auto start = std::chrono::steady_clock::now();
    while (time_left(start, secs)) {
        run_threads(8, [&](int) {
            for (int i = 0; i < 200; i++) {
                BIONIC_sem_t s;
                CHECK(sem_init_impl(&s, 0, 0) == 0);
                CHECK(sem_post_impl(&s) == 0);
                CHECK(sem_wait_impl(&s) == 0);
                CHECK(sem_destroy_impl(&s) == 0);
            }
        });
        BIONIC_sem_t shared;
        sem_init_impl(&shared, 0, 0);
        std::atomic<int> got{0};
        run_threads(2, [&](int i) {
            for (int k = 0; k < 500; k++) {
                if (i == 0) sem_post_impl(&shared);
                else { sem_wait_impl(&shared); got++; }
            }
        });
        CHECK(got == 500);
        sem_destroy_impl(&shared);
    }
}

static std::atomic<int> g_detached_ran{0};
static void* detached_main(void*)
{
    g_detached_ran++;
    return nullptr;
}

static void test_detached_threads(double)
{
    Watchdog wd("test_detached_threads", 60);
    BIONIC_pthread_attr_t attr;
    pthread_attr_init_impl(&attr);
    pthread_attr_setdetachstate_impl(&attr, PTHREAD_CREATE_DETACHED);
    const int n = 300;
    for (int i = 0; i < n; i++) {
        pthread_t t;
        CHECK(pthread_create_impl(&t, &attr, detached_main, nullptr) == 0);
        if (i % 50 == 0) usleep(2000);
    }
    while (g_detached_ran < n) usleep(1000);
    CHECK(g_detached_ran == n);
}

int main()
{
    RUN(test_mutex_lazy_init);
    RUN(test_static_recursive);
    RUN(test_mutexattr);
    RUN(test_once);
    RUN(test_cond_lazy_init);
    RUN(test_cond_timedwait);
    RUN(test_semaphores);
    RUN(test_detached_threads);
    return g_failures ? 1 : 0;
}
