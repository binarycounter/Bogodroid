#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <cstdarg>

// Bionic's sem_t structure (simplified 4-byte atomic counter)
typedef struct {
    uint32_t count;
} BIONIC_sem_t;

// The shadow counter in the bionic struct is updated by several threads at once.
static inline void sem_count_dec(BIONIC_sem_t *sem) {
    uint32_t c = __atomic_load_n(&sem->count, __ATOMIC_RELAXED);
    while (c > 0 && !__atomic_compare_exchange_n(&sem->count, &c, c - 1, true, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
    }
}

// Translation table entry
typedef struct {
    BIONIC_sem_t *bionic_sem;
    sem_t *glibc_sem;
    int is_allocated;
} sem_mapping_t;

// Simple hash table for mapping (you may want a more sophisticated implementation)
// Generous: the shim never gives up a slot until the guest destroys the semaphore, and bionic's sem_destroy is a
// no-op that many programs skip, so slots can leak; running out would make every semaphore call fail.
#define SEM_MAPPING_SIZE 16384
static sem_mapping_t sem_table[SEM_MAPPING_SIZE];
static pthread_mutex_t sem_table_lock = PTHREAD_MUTEX_INITIALIZER;

// The glibc semaphores are never handed back to malloc: another thread can still be returning from sem_wait()
// on a semaphore the guest has just destroyed, and freeing it under that thread is a use-after-free.
// Destroyed ones go on a free list and get reused instead.
typedef struct sem_node {
    sem_t sem; // must stay first: a sem_t* is cast back to its node
    struct sem_node *next;
} sem_node;
static sem_node *sem_free_list = NULL;

static sem_t *sem_node_alloc(void) { // call with sem_table_lock held
    sem_node *n = sem_free_list;
    if (n) {
        sem_free_list = n->next;
    } else {
        n = (sem_node *)malloc(sizeof(sem_node));
        if (!n)
            return NULL;
    }
    n->next = NULL;
    return &n->sem;
}

static void sem_node_release(sem_t *sem) { // call with sem_table_lock held
    sem_node *n = (sem_node *)sem;
    n->next = sem_free_list;
    sem_free_list = n;
}

// Hash function for pointer
static inline size_t hash_ptr(void *ptr) {
    uintptr_t p = (uintptr_t)ptr;
    return (p >> 3) % SEM_MAPPING_SIZE;
}

// Find or create a mapping
static sem_t *get_glibc_sem(BIONIC_sem_t *bionic_sem, int create) {
    pthread_mutex_lock(&sem_table_lock);
    
    size_t idx = hash_ptr(bionic_sem);
    size_t start_idx = idx;
    
    do {
        if (sem_table[idx].bionic_sem == bionic_sem && sem_table[idx].is_allocated) {
            pthread_mutex_unlock(&sem_table_lock);
            return sem_table[idx].glibc_sem;
        }
        
        if (create && sem_table[idx].bionic_sem == NULL) {
            sem_t *fresh = sem_node_alloc();
            if (!fresh) {
                pthread_mutex_unlock(&sem_table_lock);
                return NULL;
            }
            sem_table[idx].bionic_sem = bionic_sem;
            sem_table[idx].glibc_sem = fresh;
            sem_table[idx].is_allocated = 1;
            pthread_mutex_unlock(&sem_table_lock);
            return fresh;
        }
        
        idx = (idx + 1) % SEM_MAPPING_SIZE;
    } while (idx != start_idx);
    
    pthread_mutex_unlock(&sem_table_lock);
    return NULL;
}

// Remove a mapping
static void remove_mapping(BIONIC_sem_t *bionic_sem) {
    pthread_mutex_lock(&sem_table_lock);
    
    size_t idx = hash_ptr(bionic_sem);
    size_t start_idx = idx;
    
    do {
        if (sem_table[idx].bionic_sem == bionic_sem && sem_table[idx].is_allocated) {
            sem_node_release(sem_table[idx].glibc_sem);
            sem_table[idx].bionic_sem = NULL;
            sem_table[idx].glibc_sem = NULL;
            sem_table[idx].is_allocated = 0;
            pthread_mutex_unlock(&sem_table_lock);
            return;
        }
        
        idx = (idx + 1) % SEM_MAPPING_SIZE;
    } while (idx != start_idx);
    
    pthread_mutex_unlock(&sem_table_lock);
}

// Initialize an unnamed semaphore
int sem_init_impl(BIONIC_sem_t *sem, int pshared, unsigned int value) {
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    sem_t *glibc_sem = get_glibc_sem(sem, 1);
    if (glibc_sem == NULL) {
        errno = ENOMEM;
        return -1;
    }
    
    int ret = sem_init(glibc_sem, pshared, value);
    if (ret == 0) {
        // Store the initial value in the bionic structure for consistency
        __atomic_store_n(&sem->count, value, __ATOMIC_RELAXED);
    } else {
        remove_mapping(sem);
    }
    
    return ret;
}

// Destroy an unnamed semaphore
int sem_destroy_impl(BIONIC_sem_t *sem) {
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    sem_t *glibc_sem = get_glibc_sem(sem, 0);
    if (glibc_sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = sem_destroy(glibc_sem);
    if (ret == 0) {
        remove_mapping(sem);
    }
    
    return ret;
}

// Wait on a semaphore (blocking)
int sem_wait_impl(BIONIC_sem_t *sem) {
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    sem_t *glibc_sem = get_glibc_sem(sem, 0);
    if (glibc_sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = sem_wait(glibc_sem);
    if (ret == 0) {
        sem_count_dec(sem);
    }
    
    return ret;
}

// Try to wait on a semaphore (non-blocking)
int sem_trywait_impl(BIONIC_sem_t *sem) {
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    sem_t *glibc_sem = get_glibc_sem(sem, 0);
    if (glibc_sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = sem_trywait(glibc_sem);
    if (ret == 0) {
        sem_count_dec(sem);
    }
    
    return ret;
}

// Wait on a semaphore with timeout
int sem_timedwait_impl(BIONIC_sem_t *sem, const struct timespec *abs_timeout) {
    if (sem == NULL || abs_timeout == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    sem_t *glibc_sem = get_glibc_sem(sem, 0);
    if (glibc_sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = sem_timedwait(glibc_sem, abs_timeout);
    if (ret == 0) {
        sem_count_dec(sem);
    }
    
    return ret;
}

// Post/increment a semaphore
int sem_post_impl(BIONIC_sem_t *sem) {
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    sem_t *glibc_sem = get_glibc_sem(sem, 0);
    if (glibc_sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = sem_post(glibc_sem);
    if (ret == 0) {
        __atomic_fetch_add(&sem->count, 1, __ATOMIC_RELAXED);
    }
    
    return ret;
}

// Get the current value of a semaphore
int sem_getvalue_impl(BIONIC_sem_t *sem, int *sval) {
    if (sem == NULL || sval == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    sem_t *glibc_sem = get_glibc_sem(sem, 0);
    if (glibc_sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    int ret = sem_getvalue(glibc_sem, sval);
    if (ret == 0) {
        // Sync the bionic count with the actual value
        __atomic_store_n(&sem->count, (uint32_t)*sval, __ATOMIC_RELAXED);
    }
    
    return ret;
}

// Open a named semaphore
// Note: Named semaphores don't use the translation table since they return
// a pointer directly. You may need a different strategy here.
sem_t *sem_open_impl(const char *name, int oflag, ...) {
    mode_t mode = 0;
    unsigned int value = 0;
    
    if (oflag & O_CREAT) {
        va_list ap;
        va_start(ap, oflag);
        mode = va_arg(ap, mode_t);
        value = va_arg(ap, unsigned int);
        va_end(ap);
        
        return sem_open(name, oflag, mode, value);
    } else {
        return sem_open(name, oflag);
    }
}

// Close a named semaphore
int sem_close_impl(sem_t *sem) {
    if (sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    return sem_close(sem);
}

// Remove a named semaphore
int sem_unlink_impl(const char *name) {
    if (name == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    return sem_unlink(name);
}