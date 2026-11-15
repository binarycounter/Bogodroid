#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>
#include <sched.h>
#include <unistd.h>

#include "platform.h"
#include "thunk_pthread.h"
#include <cstring>

ABI_ATTR pthread_t pthread_self_impl()
{
    return pthread_self();
}

ABI_ATTR int pthread_key_create_impl(pthread_key_t *key, void (*destr_function) (void *))
{
    int ret = pthread_key_create(key, destr_function);
    return ret;
}

ABI_ATTR int pthread_key_delete_impl(pthread_key_t key)
{
    return pthread_key_delete(key);
}

ABI_ATTR int pthread_setspecific_impl(pthread_key_t key, const void *__pointer)
{
    // // TODO:: Investigate if this is valid - currently this works around Splash setting the zero key and causing crashes elsewhere.
    // if (key == 0)
    //     return -EINVAL;

    int ret = pthread_setspecific(key, __pointer);
    return ret;
}

ABI_ATTR void* pthread_getspecific_impl(pthread_key_t key)
{
    return pthread_getspecific(key);
}

// The pointer slot sits inside the guest's pthread_mutex_t / pthread_cond_t, whose bionic definition only needs
// 4-byte alignment. ARM64 acquire/release atomics (and __atomic_* in general) fault (SIGBUS) on a misaligned
// address, plain loads and stores don't, so the slot is accessed with memcpy plus explicit fences.
// Naturally aligned slots (the usual case) get real atomics; a misaligned one falls back to memcpy and fences.
template <class T>
static inline T slot_load(T* p)
{
    if (((uintptr_t)p & (sizeof(T) - 1)) == 0)
        return __atomic_load_n(p, __ATOMIC_ACQUIRE);
    T v;
    memcpy(&v, p, sizeof(v));
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return v;
}
template <class T>
static inline void slot_store(T* p, T v)
{
    if (((uintptr_t)p & (sizeof(T) - 1)) == 0) {
        __atomic_store_n(p, v, __ATOMIC_RELEASE);
        return;
    }
    __atomic_thread_fence(__ATOMIC_RELEASE);
    memcpy(p, &v, sizeof(v));
}

// Bionic mutexes/conds are plain structs the guest may initialise statically (all zero, or a type code for
// recursive / error-checking mutexes). Here the first 8 bytes hold a pointer to a real glibc object that is
// created on first use. That lazy creation has to be atomic, or two threads racing on the first lock each
// create their own mutex and the "mutual exclusion" silently stops excluding anything.
static pthread_mutex_t g_lazy_init_lock = PTHREAD_MUTEX_INITIALIZER;

// Static initialiser values of bionic's recursive / error-checking mutexes live in the pointer slot.
#define BIONIC_MUTEX_RECURSIVE_INIT 0x4000
#define BIONIC_MUTEX_ERRORCHECK_INIT 0x8000

static pthread_mutex_t* mutex_create(const pthread_mutexattr_t* attr)
{
    pthread_mutex_t* m = (pthread_mutex_t*)calloc(1, sizeof(pthread_mutex_t));
    if (!m)
        return NULL;
    if (pthread_mutex_init(m, attr) != 0)
    {
        free(m);
        return NULL;
    }
    return m;
}

// Returns the real mutex behind a guest mutex, creating it if this is the first use. NULL on failure.
static pthread_mutex_t* mutex_get(pthread_mutex_t** uid)
{
    pthread_mutex_t* m = slot_load(uid);
    if ((uintptr_t)m > 0x10000)
        return m;

    pthread_mutex_lock(&g_lazy_init_lock);
    m = slot_load(uid);
    if ((uintptr_t)m <= 0x10000)
    {
        pthread_mutexattr_t attr;
        const pthread_mutexattr_t* pattr = NULL;
        if ((uintptr_t)m == BIONIC_MUTEX_RECURSIVE_INIT || (uintptr_t)m == BIONIC_MUTEX_ERRORCHECK_INIT)
        {
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, (uintptr_t)m == BIONIC_MUTEX_RECURSIVE_INIT ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_ERRORCHECK);
            pattr = &attr;
        }
        m = mutex_create(pattr);
        if (pattr)
            pthread_mutexattr_destroy(&attr);
        if (m)
            slot_store(uid, m);
    }
    pthread_mutex_unlock(&g_lazy_init_lock);
    return m;
}

ABI_ATTR int pthread_mutex_init_impl(BIONIC_pthread_mutex_t *_uid, pthread_mutexattr_t **mutexattr)
{
    pthread_mutex_t **uid = (pthread_mutex_t**)_uid;
    if (uid < (pthread_mutex_t**)0x1000)
        return EINVAL;

    pthread_mutexattr_t *attr = mutexattr ? *mutexattr : NULL;
    pthread_mutex_t *m = mutex_create(attr);
    if (!m)
        return ENOMEM;

    slot_store(uid, m);
    return 0;
}

ABI_ATTR int pthread_mutex_destroy_impl(BIONIC_pthread_mutex_t *_uid)
{
    pthread_mutex_t **uid = (pthread_mutex_t**)_uid;

    pthread_mutex_t* m = uid ? slot_load(uid) : NULL;
    if ((uintptr_t)m > 0x10000)
    {
        pthread_mutex_destroy(m);
        free(m);
        slot_store(uid, (pthread_mutex_t*)NULL);
    }
    return 0;
}

ABI_ATTR int pthread_mutex_lock_impl(BIONIC_pthread_mutex_t *_uid)
{
    pthread_mutex_t **uid = (pthread_mutex_t**)_uid;

    if (uid < (pthread_mutex_t**)0x1000)
        return -1;

    pthread_mutex_t* m = mutex_get(uid);
    if (!m)
        return ENOMEM;
    return pthread_mutex_lock(m);
}

ABI_ATTR int pthread_mutex_unlock_impl(BIONIC_pthread_mutex_t *_uid)
{
    pthread_mutex_t **uid = (pthread_mutex_t**)_uid;

    if (uid < (pthread_mutex_t**)0x1000)
        return -1;

    pthread_mutex_t* m = mutex_get(uid);
    if (!m)
        return ENOMEM;
    return pthread_mutex_unlock(m);
}

ABI_ATTR int pthread_mutex_trylock_impl(BIONIC_pthread_mutex_t *_uid)
{
    pthread_mutex_t **uid = (pthread_mutex_t**)_uid;

    // Sanity check the handle's address
    if (uid < (pthread_mutex_t**)0x1000)
        return EINVAL; // Return a proper errno value

    pthread_mutex_t* m = mutex_get(uid);
    if (!m)
        return ENOMEM;
    return pthread_mutex_trylock(m);
}

static pthread_cond_t* cond_create(const pthread_condattr_t* attr)
{
    pthread_cond_t* c = (pthread_cond_t*)calloc(1, sizeof(pthread_cond_t));
    if (!c)
        return NULL;
    if (pthread_cond_init(c, attr) != 0)
    {
        free(c);
        return NULL;
    }
    return c;
}

// Same lazy, race-free creation as for mutexes (static PTHREAD_COND_INITIALIZER is all zero).
static pthread_cond_t* cond_get(pthread_cond_t** cnd)
{
    pthread_cond_t* c = slot_load(cnd);
    if ((uintptr_t)c > 0x10000)
        return c;

    pthread_mutex_lock(&g_lazy_init_lock);
    c = slot_load(cnd);
    if ((uintptr_t)c <= 0x10000)
    {
        c = cond_create(NULL);
        if (c)
            slot_store(cnd, c);
    }
    pthread_mutex_unlock(&g_lazy_init_lock);
    return c;
}

ABI_ATTR int pthread_cond_init_impl(pthread_cond_t **cnd, const int *condattr)
{
    if (!cnd)
        return EINVAL;
    // Bionic and glibc encode the clock (monotonic vs. realtime) the same way in the attribute, so pass it on:
    // otherwise a timed wait with a CLOCK_MONOTONIC deadline is read as a realtime one and returns immediately.
    pthread_cond_t *c = cond_create((const pthread_condattr_t*)condattr);
    if (!c)
        return ENOMEM;

    slot_store(cnd, c);

    return 0;
}

ABI_ATTR int pthread_cond_broadcast_impl(pthread_cond_t **cnd)
{
    pthread_cond_t* c = cond_get(cnd);
    if (!c)
        return ENOMEM;
    return pthread_cond_broadcast(c);
}

ABI_ATTR int pthread_cond_signal_impl(pthread_cond_t **cnd)
{
    pthread_cond_t* c = cond_get(cnd);
    if (!c)
        return ENOMEM;
    return pthread_cond_signal(c);
}

ABI_ATTR int pthread_cond_destroy_impl(pthread_cond_t **cnd)
{
    pthread_cond_t* c = cnd ? slot_load(cnd) : NULL;
    if ((uintptr_t)c > 0x10000)
    {
        pthread_cond_destroy(c);
        free(c);
        slot_store(cnd, (pthread_cond_t*)NULL);
    }
    return 0;
}

ABI_ATTR int pthread_cond_wait_impl(pthread_cond_t **cnd, BIONIC_pthread_mutex_t *_mtx)
{
    pthread_mutex_t **mtx = (pthread_mutex_t**)_mtx;

    pthread_cond_t* c = cond_get(cnd);
    pthread_mutex_t* m = mtx ? mutex_get(mtx) : NULL;
    if (!c || !m)
        return EINVAL;
    return pthread_cond_wait(c, m);
}

ABI_ATTR int pthread_cond_timedwait_impl(pthread_cond_t **cnd, BIONIC_pthread_mutex_t *_mtx, const struct timespec *t)
{
    pthread_mutex_t **mtx = (pthread_mutex_t**)_mtx;

    pthread_cond_t* c = cond_get(cnd);
    pthread_mutex_t* m = mtx ? mutex_get(mtx) : NULL;
    if (!c || !m || !t)
        return EINVAL;
    return pthread_cond_timedwait(c, m, t);
}

// Bionic's pthread_once states
#define ONCE_NOT_STARTED 0
#define ONCE_UNDERWAY 1
#define ONCE_COMPLETE 2

ABI_ATTR int pthread_once_impl(volatile int *once_control, void (*init_routine)(void))
{
    static thread_local volatile int* t_running = nullptr; // the once this thread is currently initialising

    if (!once_control || !init_routine)
        return EINVAL;

    if (__atomic_load_n(once_control, __ATOMIC_ACQUIRE) == ONCE_COMPLETE)
        return 0;

    int expected = ONCE_NOT_STARTED;
    if (__atomic_compare_exchange_n(once_control, &expected, ONCE_UNDERWAY, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
    {
        volatile int* outer = t_running;
        t_running = once_control;
        (*init_routine)();
        t_running = outer;
        __atomic_store_n(once_control, ONCE_COMPLETE, __ATOMIC_RELEASE);
        return 0;
    }

    // Someone else is running the initialiser: wait for it to finish, otherwise we'd use half-initialised state.
    // (A recursive call from inside the initialiser itself returns immediately instead of deadlocking.)
    if (t_running == once_control)
        return 0;
    for (int spins = 0; __atomic_load_n(once_control, __ATOMIC_ACQUIRE) != ONCE_COMPLETE; spins++)
    {
        if (spins < 100)
            sched_yield();
        else
            usleep(200);
    }
    return 0;
}



ABI_ATTR int pthread_mutexattr_init_impl(pthread_mutexattr_t **attr_ptr)
{
    if (!attr_ptr)
        return EINVAL;
    pthread_mutexattr_t *attr = (pthread_mutexattr_t*)calloc(1, sizeof(pthread_mutexattr_t));
    if (!attr)
        return ENOMEM;
    int r = pthread_mutexattr_init(attr);
    if (r != 0)
    {
        free(attr);
        return r;
    }
    *attr_ptr = attr;

    return 0;
}

ABI_ATTR int pthread_mutexattr_settype_impl(pthread_mutexattr_t **attr_ptr, int kind)
{
    if (!attr_ptr || !*attr_ptr)
        return EINVAL;
    int ret = pthread_mutexattr_settype(*attr_ptr, kind);
    return ret;
}

ABI_ATTR int pthread_mutexattr_destroy_impl(pthread_mutexattr_t **attr_ptr)
{
    if (!attr_ptr || !*attr_ptr)
        return 0;
    int ret = pthread_mutexattr_destroy(*attr_ptr);
    free(*attr_ptr); // allocated in pthread_mutexattr_init_impl
    *attr_ptr = NULL;
    return ret;
}

ABI_ATTR int pthread_join_impl(pthread_t th, void **thread_return)
{
    return pthread_join(th, thread_return);
}

#define PTHREAD_ATTR_FLAG_DETACHED 0x00000001
// pthread_t is an unsigned int, so it should be fine
int pthread_create_impl(pthread_t *thread, const BIONIC_pthread_attr_t *bionic_attr,
                        void *(*entry)(void *), void *arg)
{
    // If the caller provided NULL for the attributes, we do the same.
    // This tells the real pthread_create to use default attributes.
    if (bionic_attr == NULL) {
        return pthread_create(thread, NULL, entry, arg);
    }

    // Only the parts of the attributes that matter for resource use are carried over: the detach state (a thread
    // the game expects to be detached would otherwise keep its stack forever after it exits) and a sane stack size.
    // The explicit stack address, guard size and scheduling are deliberately left at the defaults.
    {
        pthread_attr_t ga;
        if (pthread_attr_init(&ga) != 0)
            return pthread_create(thread, NULL, entry, arg);
        if (bionic_attr->flags & PTHREAD_ATTR_FLAG_DETACHED)
            pthread_attr_setdetachstate(&ga, PTHREAD_CREATE_DETACHED);
        if (bionic_attr->stack_base == NULL && bionic_attr->stack_size >= 16384 && bionic_attr->stack_size <= (256u << 20))
            pthread_attr_setstacksize(&ga, bionic_attr->stack_size);
        int r = pthread_create(thread, &ga, entry, arg);
        pthread_attr_destroy(&ga);
        return r;
    }
}

ABI_ATTR int pthread_getattr_np_impl(pthread_t thread, BIONIC_pthread_attr_t *bionic_attr) {
    pthread_attr_t glibc_attr;
    int result;

    result = pthread_getattr_np(thread, &glibc_attr);
    if (result != 0) {
        return result;
    }

    void*  stack_base;
    size_t stack_size;
    size_t guard_size;
    int    policy;
    struct sched_param param;

    pthread_attr_getstack(&glibc_attr, &stack_base, &stack_size);
    pthread_attr_getguardsize(&glibc_attr, &guard_size);
    pthread_attr_getschedpolicy(&glibc_attr, &policy);
    pthread_attr_getschedparam(&glibc_attr, &param);

    if (bionic_attr) {
        bionic_attr->stack_base = stack_base;
        bionic_attr->stack_size = stack_size;
        bionic_attr->guard_size = guard_size;
        bionic_attr->sched_policy = policy;
        bionic_attr->sched_priority = param.sched_priority;

        bionic_attr->flags = 0;
    }

    pthread_attr_destroy(&glibc_attr);

    return 0; // Return success
}

// Initialize attributes object with default values.
int pthread_attr_init_impl(BIONIC_pthread_attr_t *attr) {
    if (!attr) {
        return EINVAL;
    }
    // Zeroing out the struct is the safest default.
    memset(attr, 0, sizeof(BIONIC_pthread_attr_t));
    return 0;
}

// Destroy attributes object. For our plain data struct, this is a no-op.
int pthread_attr_destroy_impl(BIONIC_pthread_attr_t *attr) {
    // Nothing to free or clean up. The object is just plain data.
    (void)attr; // Suppress unused parameter warning.
    return 0;
}

// --- Detach State ---
int pthread_attr_setdetachstate_impl(BIONIC_pthread_attr_t *attr, int state) {
    if (state == PTHREAD_CREATE_DETACHED) {
        attr->flags |= PTHREAD_ATTR_FLAG_DETACHED;
    } else if (state == PTHREAD_CREATE_JOINABLE) {
        attr->flags &= ~PTHREAD_ATTR_FLAG_DETACHED;
    } else {
        return EINVAL;
    }
    return 0;
}

int pthread_attr_getdetachstate_impl(const BIONIC_pthread_attr_t *attr, int *state) {
    if (attr->flags & PTHREAD_ATTR_FLAG_DETACHED) {
        *state = PTHREAD_CREATE_DETACHED;
    } else {
        *state = PTHREAD_CREATE_JOINABLE;
    }
    return 0;
}

// --- Stack Size ---
int pthread_attr_setstacksize_impl(BIONIC_pthread_attr_t *attr, size_t stacksize) {
    attr->stack_size = stacksize;
    return 0;
}

int pthread_attr_getstacksize_impl(const BIONIC_pthread_attr_t *attr, size_t *stacksize) {
    *stacksize = attr->stack_size;
    return 0;
}

// --- Stack Address & Size ---
int pthread_attr_setstack_impl(BIONIC_pthread_attr_t *attr, void *stackaddr, size_t stacksize) {
    attr->stack_base = stackaddr;
    attr->stack_size = stacksize;
    return 0;
}

int pthread_attr_getstack_impl(const BIONIC_pthread_attr_t *attr, void **stackaddr, size_t *stacksize) {
    *stackaddr = attr->stack_base;
    *stacksize = attr->stack_size;
    return 0;
}

// --- Guard Size ---
int pthread_attr_setguardsize_impl(BIONIC_pthread_attr_t *attr, size_t guardsize) {
    attr->guard_size = guardsize;
    return 0;
}

int pthread_attr_getguardsize_impl(const BIONIC_pthread_attr_t *attr, size_t *guardsize) {
    *guardsize = attr->guard_size;
    return 0;
}

// --- Scheduling Policy ---
int pthread_attr_setschedpolicy_impl(BIONIC_pthread_attr_t *attr, int policy) {
    attr->sched_policy = policy;
    return 0;
}

int pthread_attr_getschedpolicy_impl(const BIONIC_pthread_attr_t *attr, int *policy) {
    *policy = attr->sched_policy;
    return 0;
}

// --- Scheduling Parameters (Priority) ---
int pthread_attr_setschedparam_impl(BIONIC_pthread_attr_t *attr, const struct sched_param *param) {
    attr->sched_priority = param->sched_priority;
    return 0;
}

int pthread_attr_getschedparam_impl(const BIONIC_pthread_attr_t *attr, struct sched_param *param) {
    param->sched_priority = attr->sched_priority;
    return 0;
}


int pthread_attr_setstackaddr_impl(BIONIC_pthread_attr_t *attr, void *stackaddr) {
    attr->stack_base = stackaddr;
    return 0;
}

int pthread_attr_getstackaddr_impl(const BIONIC_pthread_attr_t *attr, void **stackaddr) {
    *stackaddr = attr->stack_base;
    return 0;
}


// /* Return the previously set address for the stack.  */
// ABI_ATTR int pthread_attr_getstackaddr_impl (const BIONIC_pthread_attr_t *attr, void **stackaddr)
// {
//     size_t size;
//     return pthread_attr_getstack_impl(attr, stackaddr, &size);
// }

// /* Set the starting address of the stack of the thread to be created.
//    Depending on whether the stack grows up or down the value must either
//    be higher or lower than all the address in the memory block.  The
//    minimal size of the block must be PTHREAD_STACK_MIN.  */
// ABI_ATTR int pthread_attr_setstackaddr_impl (BIONIC_pthread_attr_t *attr, void *stackaddr)
// {
//     size_t size;
//     pthread_attr_getstacksize(attr, &size); /* lets assume stack size didnt change... */
//     return pthread_attr_setstack(attr, stackaddr, size);
// }


typedef void (*BIONIC__pthread_cleanup_func_t)(void*);
// Define a structure similar to Bionic's internal cleanup handler
struct BIONIC__pthread_cleanup_t {
    BIONIC__pthread_cleanup_func_t __cleanup_routine;
    void* __cleanup_arg;
    BIONIC__pthread_cleanup_t* __cleanup_prev;
};


// Use thread-local storage to maintain a separate cleanup stack for each thread
static thread_local BIONIC__pthread_cleanup_t* __cleanup_stack = nullptr;

extern "C" {

    void __pthread_cleanup_push_impl(BIONIC__pthread_cleanup_t* c, BIONIC__pthread_cleanup_func_t routine, void* arg) {
        c->__cleanup_routine = routine;
        c->__cleanup_arg = arg;
        c->__cleanup_prev = __cleanup_stack;
        __cleanup_stack = c;
    }

    void __pthread_cleanup_pop_impl(BIONIC__pthread_cleanup_t* c, int execute) {
        // The 'c' argument is used by Bionic's implementation to manage the stack frame.
        // In our shim, we can rely on our thread-local stack.
        // We might assert(c == __cleanup_stack) here for robustness.
        __cleanup_stack = c->__cleanup_prev;
        if (execute) {
            c->__cleanup_routine(c->__cleanup_arg);
        }
    }

}