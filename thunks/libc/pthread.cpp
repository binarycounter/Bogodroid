#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>

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

ABI_ATTR int pthread_mutex_init_impl(BIONIC_pthread_mutex_t *_uid, pthread_mutexattr_t **mutexattr)
{
    pthread_mutex_t **uid = (pthread_mutex_t**)_uid;

    pthread_mutexattr_t *attr = mutexattr ? *mutexattr : NULL; 
    pthread_mutex_t *m = (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
    *m = PTHREAD_MUTEX_INITIALIZER;
    if (!m)
        return -1;

    int ret = pthread_mutex_init(m, attr);
    if (ret < 0)
    {
        free(m);
        return -1;
    }

    *uid = m;

    return 0;
}

ABI_ATTR int pthread_mutex_destroy_impl(BIONIC_pthread_mutex_t *_uid)
{
    pthread_mutex_t **uid = (pthread_mutex_t**)_uid;

    if (uid && *uid && (uintptr_t)*uid > 0x8000)
    {
        pthread_mutex_destroy(*uid);
        free(*uid);
        *uid = NULL;
    }
    return 0;
}

ABI_ATTR int pthread_mutex_lock_impl(BIONIC_pthread_mutex_t *_uid)
{
    pthread_mutex_t **uid = (pthread_mutex_t**)_uid;

    if (uid < (pthread_mutex_t**)0x1000)
        return -1;

    if (!*uid)
        pthread_mutex_init_impl(_uid, NULL);
    
    return pthread_mutex_lock(*uid);
}

ABI_ATTR int pthread_mutex_unlock_impl(BIONIC_pthread_mutex_t *_uid)
{
    pthread_mutex_t **uid = (pthread_mutex_t**)_uid;

    int ret = 0;
    if (uid < (pthread_mutex_t**)0x1000)
        return -1;

    if (!*uid)
        ret = pthread_mutex_init_impl(_uid, NULL);

    if (ret < 0)
        return ret;
    
    return pthread_mutex_unlock(*uid);
}

ABI_ATTR int pthread_mutex_trylock_impl(BIONIC_pthread_mutex_t *_uid)
{
    pthread_mutex_t **uid = (pthread_mutex_t**)_uid;

    // Sanity check the handle's address
    if (uid < (pthread_mutex_t**)0x1000)
        return EINVAL; // Return a proper errno value

    // Lazy initialization: if the handle is null, create the underlying Glibc mutex
    if (!*uid)
    {
        int ret = pthread_mutex_init_impl(_uid, NULL);
        if (ret != 0)
        {
            // If init fails, return the error. EBUSY is a possible return here
            // if the mutex is locked, but it shouldn't be during init.
            return ret;
        }
    }
    
    // Dereference the handle to get the real Glibc mutex and call the function
    return pthread_mutex_trylock(*uid);
}

ABI_ATTR int pthread_cond_init_impl(pthread_cond_t **cnd, const int *condattr)
{
    pthread_cond_t *c = (pthread_cond_t *)calloc(1, sizeof(pthread_cond_t));
    if (!c)
        return -1;

    *c = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    int ret = pthread_cond_init(c, NULL);
    if (ret < 0)
    {
        free(c);
        return -1;
    }

    *cnd = c;

    return 0;
}

ABI_ATTR int pthread_cond_broadcast_impl(pthread_cond_t **cnd)
{
    if (!*cnd)
    {
        if (pthread_cond_init_impl(cnd, NULL) < 0)
            return -1;
    }
    return pthread_cond_broadcast(*cnd);
}

ABI_ATTR int pthread_cond_signal_impl(pthread_cond_t **cnd)
{
    if (!*cnd)
    {
        if (pthread_cond_init_impl(cnd, NULL) < 0)
            return -1;
    };
    return pthread_cond_signal(*cnd);
}

ABI_ATTR int pthread_cond_destroy_impl(pthread_cond_t **cnd)
{
    if (cnd && *cnd)
    {
        pthread_cond_destroy(*cnd);
        free(*cnd);
        *cnd = NULL;
    }
    return 0;
}

ABI_ATTR int pthread_cond_wait_impl(pthread_cond_t **cnd, BIONIC_pthread_mutex_t *_mtx)
{
    pthread_mutex_t **mtx = (pthread_mutex_t**)_mtx;
    
    if (!*cnd)
    {
        if (pthread_cond_init_impl(cnd, NULL) < 0)
            return -1;
    }
    return pthread_cond_wait(*cnd, *mtx);
}

ABI_ATTR int pthread_cond_timedwait_impl(pthread_cond_t **cnd, BIONIC_pthread_mutex_t *_mtx, const struct timespec *t)
{
    pthread_mutex_t **mtx = (pthread_mutex_t**)_mtx;

    if (!*cnd)
    {
        if (pthread_cond_init_impl(cnd, NULL) < 0)
            return -1;
    }
    return pthread_cond_timedwait(*cnd, *mtx, t);
}

ABI_ATTR int pthread_once_impl(volatile int *once_control, void (*init_routine)(void))
{
    if (!once_control || !init_routine)
        return -1;
    if (__sync_lock_test_and_set(once_control, 1) == 0)
        (*init_routine)();
    return 0;
}



ABI_ATTR int pthread_mutexattr_init_impl(pthread_mutexattr_t **attr_ptr)
{
    pthread_mutexattr_t *attr = (pthread_mutexattr_t*)calloc(1, sizeof(pthread_mutexattr_t));
    int r = pthread_mutexattr_init(attr);
    *attr_ptr = attr;

    return r;
}

ABI_ATTR int pthread_mutexattr_settype_impl(pthread_mutexattr_t **attr_ptr, int kind)
{
    int ret = pthread_mutexattr_settype(*attr_ptr, kind);
    return ret;
}

ABI_ATTR int pthread_mutexattr_destroy_impl(pthread_mutexattr_t **attr_ptr)
{
    int ret = pthread_mutexattr_destroy(*attr_ptr);
    return ret;
    //free(attr_ptr);
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
    if (true || bionic_attr == NULL) {
        return pthread_create(thread, NULL, entry, arg);
    }

    // --- Translation Step ---
    // The caller provided attributes, so we must translate them from our
    // Bionic struct to a real, opaque glibc attribute object.

    int result;
    pthread_attr_t glibc_attr; // The real, opaque glibc struct.

    // 1. Initialize the glibc attribute object.
    if ((result = pthread_attr_init(&glibc_attr)) != 0) {
        return result; // Return the error code from init.
    }

    // 2. Translate each attribute from the Bionic struct to the glibc object
    //    using the REAL glibc pthread_attr_set* functions.

    // Stack attributes
    if (bionic_attr->stack_base != NULL) {
        pthread_attr_setstack(&glibc_attr, bionic_attr->stack_base, bionic_attr->stack_size);
    } else if (bionic_attr->stack_size > 0) {
        pthread_attr_setstacksize(&glibc_attr, bionic_attr->stack_size);
    }

    // Guard size
    if (bionic_attr->guard_size > 0) {
        pthread_attr_setguardsize(&glibc_attr, bionic_attr->guard_size);
    }

    // Detach state
    int detach_state = (bionic_attr->flags & PTHREAD_ATTR_FLAG_DETACHED)
                           ? PTHREAD_CREATE_DETACHED
                           : PTHREAD_CREATE_JOINABLE;
    pthread_attr_setdetachstate(&glibc_attr, detach_state);

    // Scheduling policy and priority
    if (bionic_attr->sched_policy != 0) { // Assuming 0 is the default/unset state
        pthread_attr_setschedpolicy(&glibc_attr, bionic_attr->sched_policy);
        struct sched_param param;
        param.sched_priority = bionic_attr->sched_priority;
        pthread_attr_setschedparam(&glibc_attr, &param);
    }

    // 3. Call the REAL pthread_create with the fully configured glibc attribute object.
    result = pthread_create(thread, &glibc_attr, entry, arg);

    // 4. Clean up the temporary glibc attribute object as required by the API.
    pthread_attr_destroy(&glibc_attr);

    // 5. Return the result of the create call.
    return result;
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