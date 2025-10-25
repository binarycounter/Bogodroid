#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>

#include "platform.h"
#include "logging.h"
#include "so_util.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>
#include <inttypes.h>
#include <link.h>
#include <stdbool.h>




#include "bionic_file.h"
#include <linux/futex.h>
#include <sys/syscall.h>
#include <cstring>

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

    {
        verbose("SYSCALLS","Unimplemented syscall: %ld\n", number);
    }

    return syscall(number, arg1, arg2, arg3, arg4, arg5, arg6);
}

extern "C" ABI_ATTR void abort_impl(void)
{
    fatal_error("Guest called abort!\n");
    abort();
    // exit(-1);
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
    void* addr = (void*)so_resolve_link(NULL, name);
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
    //   abort();
}

extern "C" ABI_ATTR int __system_property_get_impl(const char* name, char* value)
{
    WARN_STUB;
    value[0] = 0;
    return 0;
}

extern "C" ABI_ATTR void syslog_impl(int priority, const char* format, ...)
{
    WARN_STUB;
}

ABI_ATTR int open_impl(const char *filename, int flags, mode_t mode);
extern "C" ABI_ATTR int __open_2_impl(const char* pathname, int flags)
{
    return open_impl(pathname, flags, NULL);
}
char* clean_jar_path(const char* path);

// Taken from https://github.com/libhybris/libhybris/blob/master/hybris/common/hooks.c
ABI_ATTR int scandirat_impl(int fd, const char* dir,
    struct bionic_dirent*** namelist,
    int (*filter)(const struct bionic_dirent*),
    int (*compar)(const struct bionic_dirent**,
        const struct bionic_dirent**))
{
    char* clean_path = clean_jar_path(dir);
    struct dirent** namelist_r;
    struct bionic_dirent** result;
    struct bionic_dirent* filter_r;

    int i = 0;
    size_t nItems = 0;

    int res = scandirat(fd, clean_path, &namelist_r, NULL, NULL);

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

#ifndef LOG_DLPI
#define LOG_DLPI 0
#endif

#define DLPI_LOG(fmt, ...)                                      \
    do {                                                        \
        if (LOG_DLPI)                                           \
            fprintf(stderr, "[dlpi] " fmt "\n", ##__VA_ARGS__); \
    } while (0)

static const char* phdr_type_name(ElfW(Word) t)
{
    switch (t) {
    case PT_NULL:
        return "PT_NULL";
    case PT_LOAD:
        return "PT_LOAD";
    case PT_DYNAMIC:
        return "PT_DYNAMIC";
    case PT_INTERP:
        return "PT_INTERP";
    case PT_NOTE:
        return "PT_NOTE";
    case PT_SHLIB:
        return "PT_SHLIB";
    case PT_PHDR:
        return "PT_PHDR";
    case PT_TLS:
        return "PT_TLS";
    case 0x6474e550u:
        return "PT_GNU_EH_FRAME";
    case 0x6474e551u:
        return "PT_GNU_STACK";
    case 0x6474e552u:
        return "PT_GNU_RELRO";
    default:
        return "PT_<other>";
    }
}

// Thread-local scratch to hold a normalized PHDR view per thread during callback
static thread_local ElfW(Phdr) tl_phdr_scratch[1024];
static inline struct dl_phdr_info make_dl_phdr_info(const struct so_module* m, bool is_main_exe)
{
    struct dl_phdr_info info;
    memset(&info, 0, sizeof(info));

    // This is the correct base address where the library was loaded.
    ElfW(Addr) load_bias = (ElfW(Addr))m->base;

    const char* name = is_main_exe ? "" : (m->soname ? m->soname : "");
    DLPI_LOG("module=%p name=\"%s\" is_main=%d", (void*)m, name, is_main_exe);

    if (!m->ehdr || !m->phdr) {
        DLPI_LOG("ERROR: missing EHDR/PHDR pointers");
        return info;
    }

    ElfW(Half) phnum = m->ehdr->e_phnum;
    if (phnum == 0 || phnum > (sizeof(tl_phdr_scratch) / sizeof(tl_phdr_scratch[0]))) {
        DLPI_LOG("ERROR: phnum=%u out of bounds for scratch buffer", (unsigned)phnum);
        return info;
    }

    for (ElfW(Half) i = 0; i < phnum; i++) {
        tl_phdr_scratch[i] = m->phdr[i]; // Make a copy
        if (tl_phdr_scratch[i].p_vaddr >= load_bias) {
            tl_phdr_scratch[i].p_vaddr -= load_bias;
        }
    }

    // Fill the info struct according to the API contract
    info.dlpi_addr = load_bias;
    info.dlpi_phdr = tl_phdr_scratch; // Point to our corrected, relative headers
    info.dlpi_phnum = phnum;
    info.dlpi_name = name;

    DLPI_LOG("REPORTING: dlpi_addr(load_bias)=0x%" PRIxPTR, (uintptr_t)info.dlpi_addr);

    // Now, log the values as the unwinder will see and calculate them
    bool saw_eh = false;
    for (ElfW(Half) i = 0; i < phnum; i++) {
        const ElfW(Phdr)* ph = &info.dlpi_phdr[i];
        // This calculation should now yield the correct runtime address
        ElfW(Addr) runtime_start = info.dlpi_addr + ph->p_vaddr;
        DLPI_LOG("PHDR[%u]: type=%s p_vaddr(rel)=0x%" PRIxPTR " -> runtime_addr=0x%" PRIxPTR,
            (unsigned)i, phdr_type_name(ph->p_type), (uintptr_t)ph->p_vaddr, (uintptr_t)runtime_start);
        if (ph->p_type == 0x6474e550u) { // PT_GNU_EH_FRAME
            saw_eh = true;
            DLPI_LOG("--> PT_GNU_EH_FRAME found, runtime location will be 0x%" PRIxPTR, (uintptr_t)runtime_start);
        }
    }
    if (!saw_eh)
        DLPI_LOG("INFO: PT_GNU_EH_FRAME not present");

    return info;
}

struct hybrid_state {
    // The original callback and data from the unwinder
    int (*original_callback)(struct dl_phdr_info* info, size_t size, void* data);
    void* original_data;

    // A list of our custom modules
    const struct so_module* guest_modules_head;
};

// This is a new callback that we will pass to the REAL dl_iterate_phdr
static int hybrid_callback(struct dl_phdr_info* info, size_t size, void* data)
{
    struct hybrid_state* state = (struct hybrid_state*)data;

    // Pass the host module info to the unwinder's original callback
    return state->original_callback(info, size, state->original_data);
}

extern "C" ABI_ATTR int dl_iterate_phdr_impl(
    int (*callback)(struct dl_phdr_info* info, size_t size, void* data),
    void* data)
{
    if (!callback)
        return -1;

    DLPI_LOG("dl_iterate_phdr_impl start");

    struct hybrid_state state;
    state.original_callback = callback;
    state.original_data = data;

    DLPI_LOG("dl_iterate_phdr_impl call real dl_iterate_phdr");
    int ret = dl_iterate_phdr(hybrid_callback, &state);
    DLPI_LOG("dl_iterate_phdr_impl real dl_iterate_phdr returns %d",ret);

    // If the original callback asked to stop, we must respect that.
    if (ret != 0) {
        DLPI_LOG("dl_iterate_phdr_impl end early from real function");
        return ret;
    }

    const struct so_module* head = so_get_head();
    const struct so_module* m = head;
    bool is_first = false;

    for (; m != NULL; m = m->next, is_first = false) {
        struct dl_phdr_info info = make_dl_phdr_info(m, is_first);
        // Optional fields like dlpi_adds/subs/tls_* can remain zeroed; callers size-check via 'size'.
        DLPI_LOG("dl_iterate_phdr_impl call callback");
        ret = callback(&info, sizeof(info), data);
        DLPI_LOG("dl_iterate_phdr_impl callback returned %d", ret);
        if (ret != 0)
            break; // stop early if callback asks to stop
    }
    DLPI_LOG("dl_iterate_phdr_impl end");
    return ret; // 0 if all callbacks returned 0, or the callback's nonzero value
}


extern "C" ABI_ATTR void __assert_impl(const char *expression, const char *file, int line) {
    fprintf(stderr, "Guest assertion failed: %s, file %s, line %d\n", expression, file, line);
    abort();
}