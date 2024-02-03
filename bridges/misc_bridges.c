#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <locale.h>
#include <signal.h>
#include <setjmp.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <syscall.h>
#include <signal.h>
#include <sys/utsname.h>
#include <inttypes.h>

#include "platform.h"
#include "so_util.h"
#include "gles2_bridge.h"
#include <sys/stat.h>

extern void *__gnu_Unwind_Find_exidx;

extern void *_ZdaPv;
extern void *_ZdlPv;
extern void *_Znaj;
extern void *_Znwj;

extern void *__assert2;
extern void *__cxa_atexit;
extern void *__cxa_thread_atexit_impl;
extern void *__cxa_finalize;
extern void *__cxa_pure_virtual;
extern void *__cxa_end_catch;
extern void *__cxa_allocate_exception;
extern void *__cxa_throw;
extern void *__cxa_rethrow;
extern void *__cxa_free_exception;
extern void *__cxa_guard_acquire;
extern void *__cxa_guard_release;
extern void *_ZTVN10__cxxabiv117__class_type_infoE;
extern void *_ZTVN10__cxxabiv120__si_class_type_infoE;
extern void *_ZNSt12length_errorD1Ev;
extern void *_ZTVSt12length_error;
extern void *_ZNSt13runtime_errorD1Ev;
static char *fake__progname = "libyoyo.so";

extern void *__stack_chk_fail;
extern void *__stack_chk_guard;

static int __stack_chk_guard_fake = 0xD2424242;

const char* const sys_signame[32] = {
#define __BIONIC_SIGDEF(signal_number, unused) [ signal_number ] = #signal_number + 3,
__BIONIC_SIGDEF(SIGHUP,    "Hangup")
__BIONIC_SIGDEF(SIGINT,    "Interrupt")
__BIONIC_SIGDEF(SIGQUIT,   "Quit")
__BIONIC_SIGDEF(SIGILL,    "Illegal instruction")
__BIONIC_SIGDEF(SIGTRAP,   "Trap")
__BIONIC_SIGDEF(SIGABRT,   "Aborted")
__BIONIC_SIGDEF(SIGFPE,    "Floating point exception")
__BIONIC_SIGDEF(SIGKILL,   "Killed")
__BIONIC_SIGDEF(SIGBUS,    "Bus error")
__BIONIC_SIGDEF(SIGSEGV,   "Segmentation fault")
__BIONIC_SIGDEF(SIGPIPE,   "Broken pipe")
__BIONIC_SIGDEF(SIGALRM,   "Alarm clock")
__BIONIC_SIGDEF(SIGTERM,   "Terminated")
__BIONIC_SIGDEF(SIGUSR1,   "User signal 1")
__BIONIC_SIGDEF(SIGUSR2,   "User signal 2")
__BIONIC_SIGDEF(SIGCHLD,   "Child exited")
__BIONIC_SIGDEF(SIGPWR,    "Power failure")
__BIONIC_SIGDEF(SIGWINCH,  "Window size changed")
__BIONIC_SIGDEF(SIGURG,    "Urgent I/O condition")
__BIONIC_SIGDEF(SIGIO,     "I/O possible")
__BIONIC_SIGDEF(SIGSTOP,   "Stopped (signal)")
__BIONIC_SIGDEF(SIGTSTP,   "Stopped")
__BIONIC_SIGDEF(SIGCONT,   "Continue")
__BIONIC_SIGDEF(SIGTTIN,   "Stopped (tty input)")
__BIONIC_SIGDEF(SIGTTOU,   "Stopped (tty output)")
__BIONIC_SIGDEF(SIGVTALRM, "Virtual timer expired")
__BIONIC_SIGDEF(SIGPROF,   "Profiling timer expired")
__BIONIC_SIGDEF(SIGXCPU,   "CPU time limit exceeded")
__BIONIC_SIGDEF(SIGXFSZ,   "File size limit exceeded")
__BIONIC_SIGDEF(SIGSTKFLT, "Stack fault")
__BIONIC_SIGDEF(SIGSYS,    "Bad system call")
};

ABI_ATTR static void aeabi_memclr_impl(void *dst, size_t len)
{
    memset(dst, 0, len);
}

ABI_ATTR static void aeabi_memset_impl(void *s, size_t n, int c)
{
    memset(s, c, n);
}

ABI_ATTR static int impl__android_log_print(int prio, const char *tag, const char *fmt, ...)
{
    char andlog[2048] = {};
    va_list va;
    va_start(va, fmt);
    warning("LOG[%s]: ", tag);
    vsnprintf(andlog, 2047, fmt, va);
    warning("%s", andlog);
    va_end(va);
    printf("\n", stderr);
}

ABI_ATTR static int impl__android_log_write(int prio, const char *tag, const char *fmt, ...)
{
    char andlog[2048] = {};
    va_list va;
    va_start(va, fmt);
    warning("LOG[%s]: ", tag);
    vsnprintf(andlog, 2047, fmt, va);
    warning("%s", andlog);
    va_end(va);
    printf("\n", stderr);
}

ABI_ATTR static int impl__android_log_vprint(int prio, const char *tag, const char *fmt, va_list va)
{
    char andlog[2048] = {};
    warning("LOG[%s]: ", tag);
    vsnprintf(andlog, 2047, fmt, va);
    warning("%s", andlog);
    printf("\n", stderr);
}

ABI_ATTR static int __system_property_get(const char *name, char *value)
{
    warning("[NATIVE] system get property stub %s\n", name);
    return -1;
}

ABI_ATTR static int ret0(void)
{
    return 0;
}

ABI_ATTR static int ret1(void)
{
    return 1;
}

ABI_ATTR static int atexit_fake(void (*__func)(void))
{
    ;
}


extern so_module *loaded_modules[];
extern int loaded_modules_count;

// ABI_ATTR static void mono_set_assemblies_path_null_separated_replace_data(const char* path)
// {
//     printf("[NATIVE] mono assemblies_path being set, patching in new path\n");
//     printf("[NATIVE] %s\n",path);
//     for (int i = 0; i < loaded_modules_count; i++)
//     {
//         void (*ptr)(const char* path) = so_symbol(loaded_modules[i], "mono_set_assemblies_path_null_separated");

//         if (ptr != NULL)
//         {
//             // printf("%p\n",ptr);
//             return (*ptr)("afakepath\0yetanotherfakepath\0");
//         }
//     }
// }


ABI_ATTR static void *dlsym_fake(void *handle, const char *name)
{
    verbose("NATIVE","Symbol lookup: %s", name);
    // if starts with gl, or xgl (such as egl or vgl)
    // if ((strncmp(name, "gl", 2) == 0) || (name && strncmp(name + 1, "gl", 2) == 0))
    // {
    //     void *funct = (void *)resolve_gl_symbol(name);
    //     if (funct)
    //         return funct;
    // }

    // if(strcmp(name,"mono_set_assemblies_path_null_separated")==0)
    // {
    //     return &mono_set_assemblies_path_null_separated_replace_data;
    // }

    for (int i = 0; i < loaded_modules_count; i++)
    {
        uintptr_t ptr = so_symbol(loaded_modules[i], name);

        if (ptr != NULL)
        {
            // printf("%p\n",ptr);
            return ptr;
        }
    }

    return NULL;
}

ABI_ATTR static void *dlopen_fake(const char *filename, int flag)
{
    verbose("NATIVE","Library Load Stub: %s", filename);
    // TODO:: Add proper dlopen mechanism
    return (void *)0xAAAAAAAA;
}

#ifndef __time64_t
typedef int64_t __time64_t;
#endif

struct tm *__localtime64(const __time64_t *time)
{
    time_t _t = *time;
    return localtime(&_t);
}

typedef int64_t Year;
typedef __time64_t Time64_T;
struct tm *gmtime64_impl(const __time64_t *time)
{
    __time_t _t = *time;
    return gmtime(&_t);
}

struct TM
{
    int tm_sec;   /* Seconds.	[0-60] (1 leap second) */
    int tm_min;   /* Minutes.	[0-59] */
    int tm_hour;  /* Hours.	[0-23] */
    int tm_mday;  /* Day.		[1-31] */
    int tm_mon;   /* Month.	[0-11] */
    int tm_year;  /* Year	- 1900.  */
    int tm_wday;  /* Day of week.	[0-6] */
    int tm_yday;  /* Days in year.[0-365]	*/
    int tm_isdst; /* DST.		[-1/0/1]*/

    long int __tm_gmtoff;  /* Seconds east of UTC.  */
    const char *__tm_zone; /* Timezone abbreviation.  */
};

#define MAX_SAFE_YEAR 2037
#define MIN_SAFE_YEAR 1971
static const int length_of_year[2] = {365, 366};
/* Some numbers relating to the gregorian cycle */
static const Year years_in_gregorian_cycle = 400;
#define days_in_gregorian_cycle ((365 * 400) + 100 - 4 + 1)
static const Time64_T seconds_in_gregorian_cycle = days_in_gregorian_cycle * 60LL * 60LL * 24LL;
#define IS_LEAP(n) ((!(((n) + 1900) % 400) || (!(((n) + 1900) % 4) && (((n) + 1900) % 100))) != 0)

static const int julian_days_by_month[2][12] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
};

/* 28 year Julian calendar cycle */
#define SOLAR_CYCLE_LENGTH 28
/* Year cycle from MAX_SAFE_YEAR down. */
static const int safe_years_high[SOLAR_CYCLE_LENGTH] = {
    2016, 2017, 2018, 2019,
    2020, 2021, 2022, 2023,
    2024, 2025, 2026, 2027,
    2028, 2029, 2030, 2031,
    2032, 2033, 2034, 2035,
    2036, 2037, 2010, 2011,
    2012, 2013, 2014, 2015};
/* Year cycle from MIN_SAFE_YEAR up */
static const int safe_years_low[SOLAR_CYCLE_LENGTH] = {
    1996,
    1997,
    1998,
    1971,
    1972,
    1973,
    1974,
    1975,
    1976,
    1977,
    1978,
    1979,
    1980,
    1981,
    1982,
    1983,
    1984,
    1985,
    1986,
    1987,
    1988,
    1989,
    1990,
    1991,
    1992,
    1993,
    1994,
    1995,
};

#define TM_to_tm(dst, src)            \
    *(struct tm *)&dst = (struct tm){ \
        .tm_sec = src.tm_sec,         \
        .tm_min = src.tm_min,         \
        .tm_hour = src.tm_hour,       \
        .tm_mday = src.tm_mday,       \
        .tm_mon = src.tm_mon,         \
        .tm_year = (Year)src.tm_year, \
        .tm_wday = src.tm_wday,       \
        .tm_yday = src.tm_yday,       \
        .tm_isdst = src.tm_isdst};

#define is_exception_century(y) (((y) % 100) == 0 && ((y) % 400) == 0)

static Time64_T seconds_between_years(Year left_year, Year right_year)
{
    int increment = (left_year > right_year) ? 1 : -1;
    Time64_T seconds = 0;
    int cycles;
    if (left_year > 2400)
    {
        cycles = (left_year - 2400) / 400;
        left_year -= cycles * 400;
        seconds += cycles * seconds_in_gregorian_cycle;
    }
    else if (left_year < 1600)
    {
        cycles = (left_year - 1600) / 400;
        left_year += cycles * 400;
        seconds += cycles * seconds_in_gregorian_cycle;
    }
    while (left_year != right_year)
    {
        seconds += length_of_year[IS_LEAP(right_year - 1900)] * 60 * 60 * 24;
        right_year += increment;
    }
    return seconds * increment;
}

static Year cycle_offset(Year year)
{
    const Year start_year = 2000;
    Year year_diff = year - start_year;
    Year exceptions;

    if (year > start_year)
        year_diff--;

    exceptions = year_diff / 100;
    exceptions -= year_diff / 400;

    return exceptions * 16;
}

static int safe_year(const Year year)
{
    int safe_year = 0;
    Year year_cycle;

    if (year >= MIN_SAFE_YEAR && year <= MAX_SAFE_YEAR)
    {
        return (int)year;
    }

    year_cycle = year + cycle_offset(year);

    /* safe_years_low is off from safe_years_high by 8 years */
    if (year < MIN_SAFE_YEAR)
        year_cycle -= 8;

    /* Change non-leap xx00 years to an equivalent */
    if (is_exception_century(year))
        year_cycle += 11;

    /* Also xx01 years, since the previous year will be wrong */
    if (is_exception_century(year - 1))
        year_cycle += 17;

    year_cycle %= SOLAR_CYCLE_LENGTH;

    if (year_cycle < 0)
        year_cycle = SOLAR_CYCLE_LENGTH + year_cycle;

    if (year < MIN_SAFE_YEAR)
        safe_year = safe_years_low[year_cycle];
    else if (year > MAX_SAFE_YEAR)
        safe_year = safe_years_high[year_cycle];

    return safe_year;
}

Time64_T mktime64_impl(const struct TM *input_date)
{
    struct tm safe_date;
    struct TM date;
    Time64_T time;
    Year year = input_date->tm_year + 1900;
    if (MIN_SAFE_YEAR <= year && year <= MAX_SAFE_YEAR)
    {
        TM_to_tm(input_date, safe_date);
        return (Time64_T)mktime(&safe_date);
    }

    date = *input_date;
    date.tm_year = safe_year(year) - 1900;
    TM_to_tm(date, safe_date);
    time = (Time64_T)mktime(&safe_date);
    time += seconds_between_years(year, (Year)(safe_date.tm_year + 1900));
    return time;
}

Time64_T timegm64_impl(const struct TM *date)
{
    Time64_T days = 0;
    Time64_T seconds = 0;
    Year year;
    Year orig_year = (Year)date->tm_year;
    int cycles = 0;
    if (orig_year > 100)
    {
        cycles = (orig_year - 100) / 400;
        orig_year -= cycles * 400;
        days += (Time64_T)cycles * days_in_gregorian_cycle;
    }
    else if (orig_year < -300)
    {
        cycles = (orig_year - 100) / 400;
        orig_year -= cycles * 400;
        days += (Time64_T)cycles * days_in_gregorian_cycle;
    }

    if (orig_year > 70)
    {
        year = 70;
        while (year < orig_year)
        {
            days += length_of_year[IS_LEAP(year)];
            year++;
        }
    }
    else if (orig_year < 70)
    {
        year = 69;
        do
        {
            days -= length_of_year[IS_LEAP(year)];
            year--;
        } while (year >= orig_year);
    }
    days += julian_days_by_month[IS_LEAP(orig_year)][date->tm_mon];
    days += date->tm_mday - 1;
    seconds = days * 60 * 60 * 24;
    seconds += date->tm_hour * 60 * 60;
    seconds += date->tm_min * 60;
    seconds += date->tm_sec;
    return (seconds);
}
#if 0
int usleep(long usec)
{
    struct timespec ts = {
        .tv_sec = (long int) (usec / 1000000),
        .tv_nsec = (long int) (usec % 1000000) * 1000ul 
    };

    return nanosleep(&ts, NULL);
}
#endif
static int is_prime(int n)
{
    if (n <= 3)
        return 1;

    if (n % 2 == 0 || n % 3 == 0)
        return 0;

    for (int i = 5; i * i <= n; i = i + 6)
    {
        if (n % i == 0 || n % (i + 2) == 0)
            return 0;
    }

    return 1;
}

int _ZNSt6__ndk112__next_primeEj_impl(void *this, int n)
{
    if (n <= 1)
        return 2;

    while (!is_prime(n))
    {
        n++;
    }

    return n;
}

/*-
 * Copyright (c) 2005 Pascal Gloor <pascal.gloor@spale.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
void *
memmem(const void *l, size_t l_len, const void *s, size_t s_len)
{
    register char *cur, *last;
    const char *cl = (const char *)l;
    const char *cs = (const char *)s;

    /* we need something to compare */
    if (l_len == 0 || s_len == 0)
        return NULL;

    /* "s" must be smaller or equal to "l" */
    if (l_len < s_len)
        return NULL;

    /* special case where s_len == 1 */
    if (s_len == 1)
        return memchr(l, (int)*cs, l_len);

    /* the last position where its possible to find "s" in "l" */
    last = (char *)cl + l_len - s_len;

    for (cur = (char *)cl; cur <= last; cur++)
        if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
            return cur;

    return NULL;
}

int getpagesize()
{
    return (int)sysconf(_SC_PAGESIZE);
}

void mmap_bridge(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    verbose("NATIVE","mmap");
    mmap(addr, length, prot, flags, fd, offset);
}

int munmap_bridge(void *addr, size_t length)
{
    verbose("NATIVE","munmap");
    return munmap(addr, length);
}

int madvise_bridge(void *addr, size_t length, int advice)
{
    verbose("NATIVE","madvise");
    return madvise(addr, length, advice);
}

int mprotect_bridge(void *addr, size_t length, int prot)
{
    verbose("NATIVE","mprotect");
    return mprotect(addr, length, prot);
}

pid_t gettid()
{
    return syscall(SYS_gettid);
}

ssize_t readlink_bridge(const char *restrict pathname, char *restrict buf,
                        size_t bufsiz)
{
    return syscall(SYS_readlink,pathname,buf,bufsiz);
}


long syscall_bridge(long number, ...)
{
    va_list args;
    // printf("[NATIVE] syscall %d\n",number);
    va_start(args, number);
    return syscall(number, args);
    va_end(args);
}

int access_bridge(const char *__name, int __type)
{
    verbose("NATIVE","access query to %s", __name);
    if (strcmp(__name, "/proc/cpuinfo") == 0)
    {
        __name = "../support_files/cpuinfo.txt";
        verbose("NATIVE","Changing cpuinfo request to fake cpuinfo.txt");
    }

    if (strcmp(__name, "/sys/devices/system/cpu/present") == 0)
    {
        __name = "../support_files/cpu_present.txt";
        verbose("NATIVE","Changing cpu/present request to fake cpu_present.txt");
    }

    if (strcmp(__name, "/sys/devices/system/cpu/possible") == 0)
    {
        __name = "../support_files/cpu_possible.txt";
        verbose("NATIVE","Changing cpu/possible request to fake cpu_possible.txt");
    }

    // if (strcmp(__name, "/proc/self/maps") == 0)
    // {
    //     return 0;
    // }
    return access(__name, __type);
}

void *mmap2_bridge(void *__addr, size_t __len, int __prot, int __flags, int __fd, off_t __offset)
{
    return mmap(__addr,__len,__prot,__flags,__fd,__offset*4096);
}

void nothing()
{}



DynLibFunction symtable_misc[] = {
    {"__gnu_Unwind_Find_exidx", (uintptr_t)&__gnu_Unwind_Find_exidx},
    {"dl_unwind_find_exidx", (uintptr_t)&__gnu_Unwind_Find_exidx},

    {"__aeabi_atexit", (uintptr_t)&atexit_fake},
    {"__aeabi_memclr", (uintptr_t)&aeabi_memclr_impl},
    {"__aeabi_memclr4", (uintptr_t)&aeabi_memclr_impl},
    {"__aeabi_memclr8", (uintptr_t)&aeabi_memclr_impl},
    {"__aeabi_memcpy", (uintptr_t)&memcpy},
    {"__aeabi_memcpy4", (uintptr_t)&memcpy},
    {"__aeabi_memcpy8", (uintptr_t)&memcpy},
    {"__aeabi_memmove", (uintptr_t)&memmove},
    {"__aeabi_memmove4", (uintptr_t)&memmove},
    {"__aeabi_memmove8", (uintptr_t)&memmove},
    {"__aeabi_memset", (uintptr_t)&aeabi_memset_impl},
    {"__aeabi_memset4", (uintptr_t)&aeabi_memset_impl},
    {"__aeabi_memset8", (uintptr_t)&aeabi_memset_impl},

    {"mmap", (uintptr_t)&mmap_bridge},
    {"munmap", (uintptr_t)&munmap_bridge},
    {"madvise", (uintptr_t)&madvise_bridge},
    {"mprotect", (uintptr_t)&mprotect_bridge},
    {"__mmap2", (uintptr_t)&mmap2_bridge},

    {"__android_log_print", (uintptr_t)&impl__android_log_print},
    {"__android_log_write", (uintptr_t)&impl__android_log_write},
    {"__android_log_vprint", (uintptr_t)&impl__android_log_vprint},
    {"__system_property_get", (uintptr_t)&__system_property_get},
    {"strerror_r", (uintptr_t)&strerror_r},

    {"__cxa_atexit", (uintptr_t)&__cxa_atexit},
    {"__cxa_thread_atexit_impl", (uintptr_t)&__cxa_thread_atexit_impl},
    {"__cxa_finalize", (uintptr_t)&__cxa_finalize},
    {"__cxa_pure_virtual", (uintptr_t)&__cxa_pure_virtual},
    {"__cxa_end_catch", (uintptr_t)&__cxa_end_catch},
    {"__cxa_allocate_exception", (uintptr_t)&__cxa_allocate_exception},
    {"__cxa_throw", (uintptr_t)&__cxa_throw},
    {"__cxa_rethrow", (uintptr_t)&__cxa_rethrow},
    {"__cxa_free_exception", (uintptr_t)&__cxa_free_exception},
    {"__cxa_guard_acquire", (uintptr_t)&__cxa_guard_acquire},
    {"__cxa_guard_release", (uintptr_t)&__cxa_guard_release},

    {"__errno", (uintptr_t)&__errno_location},
    {"__stack_chk_fail", (uintptr_t)&__stack_chk_fail},
    {"__stack_chk_guard", (uintptr_t)&__stack_chk_guard_fake},

    {"syscall", (uintptr_t)&syscall_bridge},
    {"sysconf", (uintptr_t)&sysconf},
    {"sigaction", (uintptr_t)&ret0},
    {"sigemptyset", (uintptr_t)&ret0},
    {"sigprocmask", (uintptr_t)&ret0},

    {"atoi", (uintptr_t)&atoi},
    {"atol", (uintptr_t)&atol},

    {"calloc", (uintptr_t)&calloc},
    {"free", (uintptr_t)&free},
    {"malloc", (uintptr_t)&malloc},
    {"mallinfo", (uintptr_t)&mallinfo},
    {"realloc", (uintptr_t)&realloc},

    {"clock", (uintptr_t)&clock},
    {"clock_getres", (uintptr_t)&clock_getres},
    {"clock_gettime", (uintptr_t)&clock_gettime},
    {"ctime", (uintptr_t)&ctime},
    {"gettimeofday", (uintptr_t)&gettimeofday},
    {"gmtime", (uintptr_t)&gmtime},
    {"gmtime64", (uintptr_t)&gmtime64_impl},
    {"timegm64", (uintptr_t)&timegm64_impl},
    {"mktime64", (uintptr_t)&timegm64_impl},
    {"localtime_r", (uintptr_t)&localtime_r},
    {"localtime64", (uintptr_t)&__localtime64},
    {"time", (uintptr_t)&time},
    {"strftime", (uintptr_t)&strftime},
    {"setlocale", (uintptr_t)&setlocale},
    {"newlocale", (uintptr_t)&newlocale},

    {"eglGetDisplay", (uintptr_t)&ret0},
    {"eglGetProcAddress", (uintptr_t)&ret0},
    {"eglQueryString", (uintptr_t)&ret0},

    {"abort", (uintptr_t)&abort},
    {"exit", (uintptr_t)&exit},
    {"raise", (uintptr_t)&raise},
    {"mktime", (uintptr_t)&mktime},
    {"remove", (uintptr_t)&remove},

    {"dlsym", (uintptr_t)&dlsym_fake},
    {"dlopen", (uintptr_t)&dlopen_fake},
    {"dlclose", (uintptr_t)&ret0},
    {"dlerror", (uintptr_t)&ret0},

    {"getenv", (uintptr_t)&getenv},
    {"environ", (uintptr_t)&__environ},
    {"getpagesize", (uintptr_t)&getpagesize},

    {"longjmp", (uintptr_t)&longjmp},
    {"setjmp", (uintptr_t)&setjmp},

    {"memcpy", (uintptr_t)&memcpy},
    {"memmove", (uintptr_t)&memmove},
    {"memset", (uintptr_t)&memset},
    {"memchr", (uintptr_t)&memchr},
    {"memcmp", (uintptr_t)&memcmp},
    {"memmem", (uintptr_t)&memmem},
    {"memalign", (uintptr_t)&memalign},

    {"puts", (uintptr_t)&puts},
    {"qsort", (uintptr_t)&qsort},
    {"rand", (uintptr_t)&rand},
    {"srand", (uintptr_t)&srand},
    {"nanosleep", (uintptr_t)&nanosleep},
    {"getpid", (uintptr_t)&getpid},
    {"getcwd", (uintptr_t)&getcwd},
    {"gettid", (uintptr_t)&gettid},
    {"usleep", (uintptr_t)&usleep},
    {"__progname", (uintptr_t)&fake__progname},
    {"strtok_r", (uintptr_t)&__strtok_r},

    {"uname", (uintptr_t)&uname},
    {"access", (uintptr_t)&access_bridge},
    {"fstat", (uintptr_t)&fstat},
    {"readlink", (uintptr_t)&readlink_bridge},

    {"prctl", (uintptr_t)&prctl},
    {"getpriority", (uintptr_t)&getpriority},

    {"sigaltstack", (uintptr_t)&sigaltstack},
    {"sys_signame", (uintptr_t)&sys_signame},
    {"bsd_signal", (uintptr_t)&signal},

    {"strtoimax", (uintptr_t)&strtoimax},

    {NULL, (uintptr_t)NULL}};