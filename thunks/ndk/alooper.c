#include "alooper.h"
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

typedef struct FdEntry {
    int fd;
    int ident;
    int events;
    ALooper_callbackFunc callback;
    void* data;
    struct FdEntry* next;
} FdEntry;

struct ALooper {
    int epoll_fd;
    int wake_fd;
    int refcount;
    pthread_mutex_t mutex;
    FdEntry* fd_entries;
};

static pthread_key_t looper_key;
static pthread_once_t looper_key_once = PTHREAD_ONCE_INIT;

static void looper_destructor(void* ptr) {
    ALooper* looper = (ALooper*)ptr;
    if (looper) ALooper_release(looper);
}

static void create_looper_key() {
    pthread_key_create(&looper_key, looper_destructor);
}

ALooper* ALooper_prepare(int opts) {
    pthread_once(&looper_key_once, create_looper_key);
    
    ALooper* looper = (ALooper*)pthread_getspecific(looper_key);
    if (!looper) {
        looper = malloc(sizeof(ALooper));
        looper->epoll_fd = epoll_create1(0);
        looper->wake_fd = eventfd(0, EFD_NONBLOCK);
        looper->refcount = 1;
        pthread_mutex_init(&looper->mutex, NULL);
        looper->fd_entries = NULL;

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = looper->wake_fd;
        epoll_ctl(looper->epoll_fd, EPOLL_CTL_ADD, looper->wake_fd, &ev);
        
        pthread_setspecific(looper_key, looper);
    }
    return looper;
}

ALooper* ALooper_forThread() {
    pthread_once(&looper_key_once, create_looper_key);
    return (ALooper*)pthread_getspecific(looper_key);
}

void ALooper_acquire(ALooper* looper) {
    pthread_mutex_lock(&looper->mutex);
    looper->refcount++;
    pthread_mutex_unlock(&looper->mutex);
}

void ALooper_release(ALooper* looper) {
    pthread_mutex_lock(&looper->mutex);
    if (--looper->refcount == 0) {
        close(looper->epoll_fd);
        close(looper->wake_fd);
        
        FdEntry* entry = looper->fd_entries;
        while (entry) {
            FdEntry* next = entry->next;
            free(entry);
            entry = next;
        }
        
        pthread_mutex_unlock(&looper->mutex);
        pthread_mutex_destroy(&looper->mutex);
        free(looper);
    } else {
        pthread_mutex_unlock(&looper->mutex);
    }
}

int ALooper_addFd(ALooper* looper, int fd, int ident, int events,
                 ALooper_callbackFunc callback, void* data) {
    struct epoll_event ev;
    uint32_t epoll_events = 0;
    
    if (events & ALOOPER_EVENT_INPUT) epoll_events |= EPOLLIN;
    if (events & ALOOPER_EVENT_OUTPUT) epoll_events |= EPOLLOUT;
    epoll_events |= EPOLLERR | EPOLLHUP;

    ev.events = epoll_events;
    ev.data.fd = fd;

    pthread_mutex_lock(&looper->mutex);
    
    // Remove existing entry if present
    FdEntry** entry_ptr = &looper->fd_entries;
    while (*entry_ptr) {
        if ((*entry_ptr)->fd == fd) {
            FdEntry* existing = *entry_ptr;
            *entry_ptr = existing->next;
            epoll_ctl(looper->epoll_fd, EPOLL_CTL_DEL, existing->fd, NULL);
            free(existing);
            break;
        }
        entry_ptr = &(*entry_ptr)->next;
    }

    FdEntry* new_entry = malloc(sizeof(FdEntry));
    if (!new_entry) {
        pthread_mutex_unlock(&looper->mutex);
        return -1;
    }
    
    *new_entry = (FdEntry){
        .fd = fd,
        .ident = callback ? ALOOPER_POLL_CALLBACK : ident,
        .events = events,
        .callback = callback,
        .data = data,
        .next = looper->fd_entries
    };
    
    looper->fd_entries = new_entry;

    if (epoll_ctl(looper->epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        free(new_entry);
        pthread_mutex_unlock(&looper->mutex);
        return -1;
    }

    pthread_mutex_unlock(&looper->mutex);
    return 1;
}

int ALooper_removeFd(ALooper* looper, int fd) {
    pthread_mutex_lock(&looper->mutex);
    
    FdEntry** entry_ptr = &looper->fd_entries;
    while (*entry_ptr) {
        FdEntry* entry = *entry_ptr;
        if (entry->fd == fd) {
            *entry_ptr = entry->next;
            int rc = epoll_ctl(looper->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            free(entry);
            pthread_mutex_unlock(&looper->mutex);
            return rc == 0 ? 1 : -1;
        }
        entry_ptr = &entry->next;
    }
    
    pthread_mutex_unlock(&looper->mutex);
    return 0;
}

void ALooper_wake(ALooper* looper) {
    uint64_t value = 1;
    write(looper->wake_fd, &value, sizeof(value));
}

static int process_events(ALooper* looper, int timeout_ms,
                         int* out_fd, int* out_events, void** out_data) {
    struct epoll_event events[16];
    int num_events = epoll_wait(looper->epoll_fd, events, 16, timeout_ms);
    
    if (num_events == -1) return ALOOPER_POLL_ERROR;

    // Check for wake event first
    for (int i = 0; i < num_events; i++) {
        if (events[i].data.fd == looper->wake_fd) {
            uint64_t value;
            read(looper->wake_fd, &value, sizeof(value));
            return ALOOPER_POLL_WAKE;
        }
    }

    int result = ALOOPER_POLL_TIMEOUT;
    pthread_mutex_lock(&looper->mutex);
    
    for (int i = 0; i < num_events; i++) {
        int fd = events[i].data.fd;
        FdEntry* entry = looper->fd_entries;
        
        while (entry) {
            if (entry->fd == fd) {
                int android_events = 0;
                uint32_t epoll_ev = events[i].events;
                
                if (epoll_ev & EPOLLIN) android_events |= ALOOPER_EVENT_INPUT;
                if (epoll_ev & EPOLLOUT) android_events |= ALOOPER_EVENT_OUTPUT;
                if (epoll_ev & EPOLLERR) android_events |= ALOOPER_EVENT_ERROR;
                if (epoll_ev & EPOLLHUP) android_events |= ALOOPER_EVENT_HANGUP;

                if (entry->callback) {
                    int cb_result = entry->callback(fd, android_events, entry->data);
                    if (!cb_result) ALooper_removeFd(looper, fd);
                    result = ALOOPER_POLL_CALLBACK;
                } else {
                    if (out_fd) *out_fd = fd;
                    if (out_events) *out_events = android_events;
                    if (out_data) *out_data = entry->data;
                    result = entry->ident;
                }
                break;
            }
            entry = entry->next;
        }
    }
    
    pthread_mutex_unlock(&looper->mutex);
    return result;
}

int ALooper_pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    ALooper* looper = ALooper_forThread();
    if (!looper) return ALOOPER_POLL_ERROR;
    return process_events(looper, timeoutMillis, outFd, outEvents, outData);
}

int ALooper_pollAll(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    ALooper* looper = ALooper_forThread();
    if (!looper) return ALOOPER_POLL_ERROR;
    
    while (1) {
        int result = process_events(looper, timeoutMillis, outFd, outEvents, outData);
        if (result != ALOOPER_POLL_CALLBACK && result != ALOOPER_POLL_WAKE) {
            return result;
        }
    }
}