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

    char resolved1[PATH_MAX];
    char resolved2[PATH_MAX];
    realpath(filename, resolved1);

    so_module* head = so_get_head();
    while(head)
    {
        printf("Checking %s\n", head->path);
        realpath(head->path, resolved2);
        if (strcmp(resolved1, resolved2) == 0)
            return head;
        head = head->next;
    }


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

    void* addr = (void*)so_resolve_link((so_module*)handle, name);
    printf("dlsym(%p, %s) = 0x%p\n", handle, name, addr);
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

extern ABI_ATTR long sysconf_impl(int name)
{
switch (name) {
    case 0x0000: return sysconf(_SC_ARG_MAX);
    case 0x0001: return sysconf(_SC_BC_BASE_MAX);
    case 0x0002: return sysconf(_SC_BC_DIM_MAX);
    case 0x0003: return sysconf(_SC_BC_SCALE_MAX);
    case 0x0004: return sysconf(_SC_BC_STRING_MAX);
    case 0x0005: return sysconf(_SC_CHILD_MAX);
    case 0x0006: return sysconf(_SC_CLK_TCK);
    case 0x0007: return sysconf(_SC_COLL_WEIGHTS_MAX);
    case 0x0008: return sysconf(_SC_EXPR_NEST_MAX);
    case 0x0009: return sysconf(_SC_LINE_MAX);
    case 0x000a: return sysconf(_SC_NGROUPS_MAX);
    case 0x000b: return sysconf(_SC_OPEN_MAX);
    case 0x000c: return sysconf(_SC_PASS_MAX);
    case 0x000d: return sysconf(_SC_2_C_BIND);
    case 0x000e: return sysconf(_SC_2_C_DEV);
    case 0x000f: return sysconf(_SC_2_C_VERSION);
    case 0x0010: return sysconf(_SC_2_CHAR_TERM);
    case 0x0011: return sysconf(_SC_2_FORT_DEV);
    case 0x0012: return sysconf(_SC_2_FORT_RUN);
    case 0x0013: return sysconf(_SC_2_LOCALEDEF);
    case 0x0014: return sysconf(_SC_2_SW_DEV);
    case 0x0015: return sysconf(_SC_2_UPE);
    case 0x0016: return sysconf(_SC_2_VERSION);
    case 0x0017: return sysconf(_SC_JOB_CONTROL);
    case 0x0018: return sysconf(_SC_SAVED_IDS);
    case 0x0019: return sysconf(_SC_VERSION);
    case 0x001a: return sysconf(_SC_RE_DUP_MAX);
    case 0x001b: return sysconf(_SC_STREAM_MAX);
    case 0x001c: return sysconf(_SC_TZNAME_MAX);
    case 0x001d: return sysconf(_SC_XOPEN_CRYPT);
    case 0x001e: return sysconf(_SC_XOPEN_ENH_I18N);
    case 0x001f: return sysconf(_SC_XOPEN_SHM);
    case 0x0020: return sysconf(_SC_XOPEN_VERSION);
    case 0x0021: return sysconf(_SC_XOPEN_XCU_VERSION);
    case 0x0022: return sysconf(_SC_XOPEN_REALTIME);
    case 0x0023: return sysconf(_SC_XOPEN_REALTIME_THREADS);
    case 0x0024: return sysconf(_SC_XOPEN_LEGACY);
    case 0x0025: return sysconf(_SC_ATEXIT_MAX);
    case 0x0026: return sysconf(_SC_IOV_MAX);
    case 0x0027: return sysconf(_SC_PAGESIZE);
    case 0x0028: return sysconf(_SC_PAGE_SIZE);
    case 0x0029: return sysconf(_SC_XOPEN_UNIX);
    case 0x002a: return sysconf(_SC_XBS5_ILP32_OFF32);
    case 0x002b: return sysconf(_SC_XBS5_ILP32_OFFBIG);
    case 0x002c: return sysconf(_SC_XBS5_LP64_OFF64);
    case 0x002d: return sysconf(_SC_XBS5_LPBIG_OFFBIG);
    case 0x002e: return sysconf(_SC_AIO_LISTIO_MAX);
    case 0x002f: return sysconf(_SC_AIO_MAX);
    case 0x0030: return sysconf(_SC_AIO_PRIO_DELTA_MAX);
    case 0x0031: return sysconf(_SC_DELAYTIMER_MAX);
    case 0x0032: return sysconf(_SC_MQ_OPEN_MAX);
    case 0x0033: return sysconf(_SC_MQ_PRIO_MAX);
    case 0x0034: return sysconf(_SC_RTSIG_MAX);
    case 0x0035: return sysconf(_SC_SEM_NSEMS_MAX);
    case 0x0036: return sysconf(_SC_SEM_VALUE_MAX);
    case 0x0037: return sysconf(_SC_SIGQUEUE_MAX);
    case 0x0038: return sysconf(_SC_TIMER_MAX);
    case 0x0039: return sysconf(_SC_ASYNCHRONOUS_IO);
    case 0x003a: return sysconf(_SC_FSYNC);
    case 0x003b: return sysconf(_SC_MAPPED_FILES);
    case 0x003c: return sysconf(_SC_MEMLOCK);
    case 0x003d: return sysconf(_SC_MEMLOCK_RANGE);
    case 0x003e: return sysconf(_SC_MEMORY_PROTECTION);
    case 0x003f: return sysconf(_SC_MESSAGE_PASSING);
    case 0x0040: return sysconf(_SC_PRIORITIZED_IO);
    case 0x0041: return sysconf(_SC_PRIORITY_SCHEDULING);
    case 0x0042: return sysconf(_SC_REALTIME_SIGNALS);
    case 0x0043: return sysconf(_SC_SEMAPHORES);
    case 0x0044: return sysconf(_SC_SHARED_MEMORY_OBJECTS);
    case 0x0045: return sysconf(_SC_SYNCHRONIZED_IO);
    case 0x0046: return sysconf(_SC_TIMERS);
    case 0x0047: return sysconf(_SC_GETGR_R_SIZE_MAX);
    case 0x0048: return sysconf(_SC_GETPW_R_SIZE_MAX);
    case 0x0049: return sysconf(_SC_LOGIN_NAME_MAX);
    case 0x004a: return sysconf(_SC_THREAD_DESTRUCTOR_ITERATIONS);
    case 0x004b: return sysconf(_SC_THREAD_KEYS_MAX);
    case 0x004c: return sysconf(_SC_THREAD_STACK_MIN);
    case 0x004d: return sysconf(_SC_THREAD_THREADS_MAX);
    case 0x004e: return sysconf(_SC_TTY_NAME_MAX);
    case 0x004f: return sysconf(_SC_THREADS);
    case 0x0050: return sysconf(_SC_THREAD_ATTR_STACKADDR);
    case 0x0051: return sysconf(_SC_THREAD_ATTR_STACKSIZE);
    case 0x0052: return sysconf(_SC_THREAD_PRIORITY_SCHEDULING);
    case 0x0053: return sysconf(_SC_THREAD_PRIO_INHERIT);
    case 0x0054: return sysconf(_SC_THREAD_PRIO_PROTECT);
    case 0x0055: return sysconf(_SC_THREAD_SAFE_FUNCTIONS);
    case 0x0060: return sysconf(_SC_NPROCESSORS_CONF);
    case 0x0061: return sysconf(_SC_NPROCESSORS_ONLN);
    case 0x0062: return sysconf(_SC_PHYS_PAGES);
    case 0x0063: return sysconf(_SC_AVPHYS_PAGES);
    case 0x0064: return sysconf(_SC_MONOTONIC_CLOCK);
    case 0x0065: return sysconf(_SC_2_PBS);
    case 0x0066: return sysconf(_SC_2_PBS_ACCOUNTING);
    case 0x0067: return sysconf(_SC_2_PBS_CHECKPOINT);
    case 0x0068: return sysconf(_SC_2_PBS_LOCATE);
    case 0x0069: return sysconf(_SC_2_PBS_MESSAGE);
    case 0x006a: return sysconf(_SC_2_PBS_TRACK);
    case 0x006b: return sysconf(_SC_ADVISORY_INFO);
    case 0x006c: return sysconf(_SC_BARRIERS);
    case 0x006d: return sysconf(_SC_CLOCK_SELECTION);
    case 0x006e: return sysconf(_SC_CPUTIME);
    case 0x006f: return sysconf(_SC_HOST_NAME_MAX);
    case 0x0070: return sysconf(_SC_IPV6);
    case 0x0071: return sysconf(_SC_RAW_SOCKETS);
    case 0x0072: return sysconf(_SC_READER_WRITER_LOCKS);
    case 0x0073: return sysconf(_SC_REGEXP);
    case 0x0074: return sysconf(_SC_SHELL);
    case 0x0075: return sysconf(_SC_SPAWN);
    case 0x0076: return sysconf(_SC_SPIN_LOCKS);
    case 0x0077: return sysconf(_SC_SPORADIC_SERVER);
    case 0x0078: return sysconf(_SC_SS_REPL_MAX);
    case 0x0079: return sysconf(_SC_SYMLOOP_MAX);
    case 0x007a: return sysconf(_SC_THREAD_CPUTIME);
    case 0x007b: return sysconf(_SC_THREAD_PROCESS_SHARED);
    case 0x007c: return sysconf(_SC_THREAD_ROBUST_PRIO_INHERIT);
    case 0x007d: return sysconf(_SC_THREAD_ROBUST_PRIO_PROTECT);
    case 0x007e: return sysconf(_SC_THREAD_SPORADIC_SERVER);
    case 0x007f: return sysconf(_SC_TIMEOUTS);
    case 0x0080: return sysconf(_SC_TRACE);
    case 0x0081: return sysconf(_SC_TRACE_EVENT_FILTER);
    case 0x0082: return sysconf(_SC_TRACE_EVENT_NAME_MAX);
    case 0x0083: return sysconf(_SC_TRACE_INHERIT);
    case 0x0084: return sysconf(_SC_TRACE_LOG);
    case 0x0085: return sysconf(_SC_TRACE_NAME_MAX);
    case 0x0086: return sysconf(_SC_TRACE_SYS_MAX);
    case 0x0087: return sysconf(_SC_TRACE_USER_EVENT_MAX);
    case 0x0088: return sysconf(_SC_TYPED_MEMORY_OBJECTS);
    case 0x0089: return sysconf(_SC_V7_ILP32_OFF32);
    case 0x008a: return sysconf(_SC_V7_ILP32_OFFBIG);
    case 0x008b: return sysconf(_SC_V7_LP64_OFF64);
    case 0x008c: return sysconf(_SC_V7_LPBIG_OFFBIG);
    case 0x008d: return sysconf(_SC_XOPEN_STREAMS);
    //case 0x008e: return sysconf(_SC_XOPEN_UUCP); TODO: Not supported on Linux
    case 0x008f: return sysconf(_SC_LEVEL1_ICACHE_SIZE);
    case 0x0090: return sysconf(_SC_LEVEL1_ICACHE_ASSOC);
    case 0x0091: return sysconf(_SC_LEVEL1_ICACHE_LINESIZE);
    case 0x0092: return sysconf(_SC_LEVEL1_DCACHE_SIZE);
    case 0x0093: return sysconf(_SC_LEVEL1_DCACHE_ASSOC);
    case 0x0094: return sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
    case 0x0095: return sysconf(_SC_LEVEL2_CACHE_SIZE);
    case 0x0096: return sysconf(_SC_LEVEL2_CACHE_ASSOC);
    case 0x0097: return sysconf(_SC_LEVEL2_CACHE_LINESIZE);
    case 0x0098: return sysconf(_SC_LEVEL3_CACHE_SIZE);
    case 0x0099: return sysconf(_SC_LEVEL3_CACHE_ASSOC);
    case 0x009a: return sysconf(_SC_LEVEL3_CACHE_LINESIZE);
    case 0x009b: return sysconf(_SC_LEVEL4_CACHE_SIZE);
    case 0x009c: return sysconf(_SC_LEVEL4_CACHE_ASSOC);
    case 0x009d: return sysconf(_SC_LEVEL4_CACHE_LINESIZE);
    //case 0x009e: return sysconf(_SC_NSIG); TODO: Not supported on Linux
    default: {
        long result = sysconf(name);
        printf("sysconf(%d) returned %ld\n", name, result);
        return result;
    }
}
}

extern ABI_ATTR int strerror_r_impl(int errnum, char *buf, size_t buflen)
{
    char* ret = strerror_r(errnum, buf, buflen);
    if(ret == 0)
        return -1;
    else
       return errnum;
}
 #include <fnmatch.h>
ABI_ATTR int fnmatch_impl(const char *pattern, const char *string, int flags)
{
    return fnmatch(pattern, string, flags);
}