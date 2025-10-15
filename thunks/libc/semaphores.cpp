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

// Translation table entry
typedef struct {
    BIONIC_sem_t *bionic_sem;
    sem_t *glibc_sem;
    int is_allocated;
} sem_mapping_t;

// Simple hash table for mapping (you may want a more sophisticated implementation)
#define SEM_MAPPING_SIZE 1024
static sem_mapping_t sem_table[SEM_MAPPING_SIZE];
static pthread_mutex_t sem_table_lock = PTHREAD_MUTEX_INITIALIZER;

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
            sem_table[idx].bionic_sem = bionic_sem;
            sem_table[idx].glibc_sem = (sem_t *)malloc(sizeof(sem_t));
            sem_table[idx].is_allocated = 1;
            pthread_mutex_unlock(&sem_table_lock);
            return sem_table[idx].glibc_sem;
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
            free(sem_table[idx].glibc_sem);
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
        sem->count = value;
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
        // Update bionic counter (atomic decrement would be more accurate)
        if (sem->count > 0) {
            sem->count--;
        }
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
        if (sem->count > 0) {
            sem->count--;
        }
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
        if (sem->count > 0) {
            sem->count--;
        }
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
        // Update bionic counter (atomic increment would be more accurate)
        sem->count++;
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
        sem->count = (uint32_t)*sval;
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