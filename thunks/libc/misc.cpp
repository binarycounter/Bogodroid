#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

#include "platform.h"
#include "so_util.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bionic_file.h"
#include <sys/syscall.h>
#include <linux/futex.h>

extern "C" ABI_ATTR int login_tty_impl(int fd)
{
    return -1;
}

extern "C" long syscall_impl(long number,
                             long arg1, long arg2, long arg3,
                             long arg4, long arg5, long arg6)
{
    if (number == 0xb2) { // __NR_gettid for aarch64
        // A simple passthrough for gettid
        return syscall(SYS_gettid);
    }

    if (number == 0x62) { // __NR_futex for aarch64
        // Cast arguments to their expected types for futex
        int* uaddr = (int*)arg1;
        int op = (int)arg2;
        unsigned int val = (unsigned int)arg3;
        const struct timespec* timeout = (const struct timespec*)arg4;
        int* uaddr2 = (int*)arg5;
        unsigned int val3 = (unsigned int)arg6;

        // // Isolate the primary operation (e.g., WAIT, WAKE) from flags
        // int op_cmd = op & FUTEX_CMD_MASK; // FUTEX_CMD_MASK is ~FUTEX_PRIVATE_FLAG
        // const char* priv_str = (op & FUTEX_PRIVATE_FLAG) ? "_PRIVATE" : "";

        // // --- Detailed Logging Based on Operation ---
        // switch (op_cmd) {
        //     case FUTEX_WAIT:
        //         fprintf(stderr, "[FUTEX_WAIT] ---> Thread %ld waiting on addr %p. ", syscall(SYS_gettid), uaddr);
        //         fprintf(stderr, "Op: FUTEX_WAIT%s. Expected val: %u. ", priv_str, val);
        //         if (timeout) {
        //             fprintf(stderr, "Timeout: %ld s, %ld ns.\n", timeout->tv_sec, timeout->tv_nsec);
        //         } else {
        //             fprintf(stderr, "Timeout: INFINITE.\n");
        //         }
        //         break;

        //     case FUTEX_WAKE:
        //         fprintf(stderr, "[FUTEX_WAKE] ---> Thread %ld waking up addr %p. ", syscall(SYS_gettid), uaddr);
        //         fprintf(stderr, "Op: FUTEX_WAKE%s. Wake count: %u.\n", priv_str, val);
        //         break;

        //     default:
        //         fprintf(stderr, "[FUTEX_OTHER] -> Thread %ld calling futex on addr %p. ", syscall(SYS_gettid), uaddr);
        //         fprintf(stderr, "Op: %d (Unknown). Val: %u.\n", op, val);
        //         break;
        // }

        // --- Make the actual syscall to the host kernel ---
        long ret = syscall(SYS_futex, uaddr, op, val, timeout, uaddr2, val3);

        // // --- Log the result ---
        // if (ret == -1) {
        //     fprintf(stderr, "[FUTEX_RESULT] <--- Call FAILED for addr %p. Return: %ld, Errno: %d (%s)\n",
        //             uaddr, ret, errno, strerror(errno));
        // } else {
        //     fprintf(stderr, "[FUTEX_RESULT] <--- Call SUCCEEDED for addr %p. Return: %ld (woken threads)\n",
        //             uaddr, ret);
        // }

        return ret;
    }

    {printf("UNIMPLEMENTED SYSCALL %ld\n", number);}

    return syscall(number,arg1,arg2,arg3,arg4,arg5,arg6);
}

extern "C" ABI_ATTR void abort_impl(void)
{
    fatal_error("Guest called abort!\n");
    exit(-1);
}

extern "C" ABI_ATTR void* dlopen_impl(const char* filename, int flags)
{
    printf("Guest called dlopen for %s\n", filename);

    if (filename == NULL)
        return NULL;

    // char *fn = strdup(filename);
    // char *ex = basename(fn);
    // int ret = strncmp(ex, "libEGL", 6) == 0 ||
    //           strncmp(ex, "libGL", 5) == 0;

    // return (ret) ? (void*)0xDEAD : NULL;
    return (void*)0xDEAD;
}

extern "C" ABI_ATTR char* dlerror_impl(void)
{
    WARN_STUB
    return NULL;
}

extern "C" ABI_ATTR int dlclose_impl(void* handle)
{
    /* ... */
    return 0;
}

extern "C" ABI_ATTR int dladdr_impl(const void* addr, Dl_info* info)
{
    /* THIS IS TERRIBLE LOL */
    WARN_STUB
    return 0;
}

extern "C" ABI_ATTR void* dlsym_impl(void* handle, const char* name)
{
    // printf("App is attempting to resolve symbol %s => ",name);
    void* addr = (void*)so_resolve_link(NULL, name);
    // printf("%p\n",addr);
    return addr;
}

extern "C" ABI_ATTR const void*
memchr_impl(const void* __s, int __c, size_t __n)
{
    return __builtin_memchr(__s, __c, __n);
}

extern "C" ABI_ATTR int sigsetmask_impl(int mask)
{
    WARN_STUB
    return -1;
}

extern "C" ABI_ATTR char* tempnam_impl(const char* dir, const char* pfx)
{
    WARN_STUB
    return NULL;
}

extern "C" ABI_ATTR char* tmpnam_impl(char* s)
{
    WARN_STUB
    return NULL;
}

extern "C" ABI_ATTR char* mktemp_impl(char* _template)
{
    WARN_STUB
    return NULL;
}

extern "C" ABI_ATTR int* __errno_impl(void)
{
    return __errno_location();
}

extern "C" ABI_ATTR int __android_log_write_impl(int prio, const char* tag, const char* text)
{
    char andlog[2048] = {};
    warning("LOG[%s]: %s\n", tag, text);
    return 1;
}

extern "C" ABI_ATTR int __android_log_print_impl(int prio, const char* tag, const char* fmt, ...)
{
    char andlog[2048] = {};
    va_list va;
    va_start(va, fmt);
    warning("LOG[%s]: ", tag);
    int r = vsnprintf(andlog, 2047, fmt, va);
    warning("%s\n", andlog);
    va_end(va);
    return r;
}

extern "C" ABI_ATTR int __android_log_vprint_impl(int prio, const char* tag, const char* fmt, va_list va)
{
    char andlog[2048] = {};
    warning("LOG[%s]: ", tag);
    int r = vsnprintf(andlog, 2047, fmt, va);
    warning("%s\n", andlog);
    return r;
}

extern "C" ABI_ATTR const char* __strchr_chk(const char* __s, int __ch, size_t __n) { return strchr(__s, __ch); }
extern "C" ABI_ATTR const char* __strrchr_chk(const char* __s, int __ch, size_t __n) { return strrchr(__s, __ch); }
extern "C" ABI_ATTR size_t __strlen_chk(const char* __s, size_t __n) { return strnlen(__s, __n); }

extern "C" ABI_ATTR void android_set_abort_message_impl(const char* msg)
{
    fatal_error("%s", msg);
    abort();
}

extern "C" ABI_ATTR int __system_property_get_impl(const char* name, char* value)
{
    WARN_STUB;
    value[0] = 0;
    return 0;
}

extern "C" ABI_ATTR int __open_2_impl(const char* pathname, int flags)
{
    return open(pathname, flags);
}

// Taken from https://github.com/libhybris/libhybris/blob/master/hybris/common/hooks.c
ABI_ATTR int scandirat_impl(int fd, const char* dir,
    struct bionic_dirent*** namelist,
    int (*filter)(const struct bionic_dirent*),
    int (*compar)(const struct bionic_dirent**,
        const struct bionic_dirent**))
{
    struct dirent** namelist_r;
    struct bionic_dirent** result;
    struct bionic_dirent* filter_r;

    int i = 0;
    size_t nItems = 0;

    int res = scandirat(fd, dir, &namelist_r, NULL, NULL);

    if (res > 0 && namelist_r != NULL) {
        result = (bionic_dirent**)malloc(res * sizeof(struct bionic_dirent));
        if (!result)
            return -1;

        for (i = 0; i < res; i++) {
            filter_r = (bionic_dirent*)malloc(sizeof(struct bionic_dirent));
            if (!filter_r) {
                while (i-- > 0)
                    free(result[i]);
                free(result);
                return -1;
            }

            filter_r->d_ino = namelist_r[i]->d_ino;
            filter_r->d_off = namelist_r[i]->d_off;
            filter_r->d_reclen = namelist_r[i]->d_reclen;
            filter_r->d_type = namelist_r[i]->d_type;

            strcpy(filter_r->d_name, namelist_r[i]->d_name);
            filter_r->d_name[sizeof(namelist_r[i]->d_name) - 1] = '\0';

            if (filter != NULL && !(*filter)(filter_r)) { // apply filter
                free(filter_r);
                continue;
            }

            result[nItems++] = filter_r;
        }

        if (nItems && compar != NULL) // sort
            qsort(result, nItems, sizeof(struct bionic_dirent*), (__compar_fn_t)compar);

        *namelist = result;
    } else {
        return res;
    }

    return nItems;
}

ABI_ATTR int scandir_impl(const char* dir,
    struct bionic_dirent*** namelist,
    int (*filter)(const struct bionic_dirent*),
    int (*compar)(const struct bionic_dirent**,
        const struct bionic_dirent**))
{
    return scandirat_impl(AT_FDCWD, dir, namelist, filter, compar);
}

ABI_ATTR int prctl_impl(int op, int arg1, int arg2, int arg3)
{
    return 0;
}
