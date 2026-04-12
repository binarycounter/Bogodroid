#define _LARGEFILE64_SOURCE /* See feature_test_macros(7) */
#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <unistd.h>

#include "fcntl.h"
#include <unistd.h>
#include "platform.h"
#include "logging.h"
#include "so_util.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <cerrno>

char* clean_jar_path(const char* path) {
    if (!path) return NULL;

    const char* needles[] = {"jar:file:/!", "jar:file://!"};
    size_t num_needles = sizeof(needles) / sizeof(needles[0]);

    const char* src = path;
    int prefix_at_start = 0;

    // Check if jar prefix is at the start
    for (size_t i = 0; i < num_needles; ++i) {
        size_t needle_len = strlen(needles[i]);
        if (strncmp(path, needles[i], needle_len) == 0) {
            prefix_at_start = 1;
            break;
        }
    }

    // Get current working directory if needed
    char cwd[PATH_MAX];
    if (prefix_at_start) {
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            return NULL;
        }
    }

    size_t len = strlen(path);
    size_t cwd_len = prefix_at_start ? strlen(cwd) : 0;
    char* clean_path = (char*)malloc(len + cwd_len + 2);  // +2 for potential "." and null
    if (!clean_path) return NULL;

    char* dst = clean_path;

    // Prepend CWD if jar prefix was at start
    if (prefix_at_start) {
        strcpy(dst, cwd);
        dst += cwd_len;
    }

    // Copy path while removing jar prefixes
    while (*src) {
        int matched = 0;
        for (size_t i = 0; i < num_needles; ++i) {
            size_t needle_len = strlen(needles[i]);
            if (strncmp(src, needles[i], needle_len) == 0) {
                src += needle_len; // skip just the jar prefix
                matched = 1;
                break;
            }
        }
        if (!matched) {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    // If path starts with "/", check existence
    if (clean_path[0] == '/') {
        struct stat st;
        // Try original path
        if (stat(clean_path, &st) == 0) {
            // Path exists as-is, use it
            return clean_path;
        }
        
        // Try with "." prefix
        char* dot_path = (char*)malloc(strlen(clean_path) + 2);
        if (dot_path) {
            dot_path[0] = '.';
            strcpy(dot_path + 1, clean_path);
            
            if (stat(dot_path, &st) == 0) {
                // Dot version exists, use it
                free(clean_path);
                return dot_path;
            }
            free(dot_path);
        }
        // Neither exists, return original clean_path
    }

    return clean_path;
}
ABI_ATTR int open_impl(const char *filename, int flags, mode_t mode)
{
    verbose("NATIVE","Opening file %s",filename);

    // if (strcmp(filename,"/proc/cpuinfo") == 0)
    // {
    //     filename = "../support_files/cpuinfo.txt";
    //     verbose("NATIVE","Changing cpuinfo request to fake cpuinfo.txt");
    // }

    if (strcmp(filename,"/sys/devices/system/cpu/present") == 0)
    {
        filename = "../support_files/cpu_present.txt";
        verbose("NATIVE","Changing cpu/present request to fake cpu_present.txt");
    }

    if (strcmp(filename,"/sys/devices/system/cpu/possible") == 0)
    {
        filename = "../support_files/cpu_possible.txt";
        verbose("NATIVE","Changing cpu/possible request to fake cpu_possible.txt");
    }

    // if (strcmp(filename,"/proc/self/maps") == 0)
    // {
    //     verbose("NATIVE","No maps for you >:)");
    //     return -1;
    // }
    
    char* clean_path = clean_jar_path(filename);
    int fd = open(clean_path, flags, mode);
    verbose("NATIVE","Got file descriptor %d",fd);
    return fd;
}


ABI_ATTR ssize_t read_impl(int fd, void *buf, size_t count)
{
    verbose("NATIVE","reading %zu bytes from file %d",count,fd);
    int ret=read(fd, buf, count);
    // int i;
    // for (i = 0; i < count; i++)
    // {
    //     if (i > 0) printf(":");
    //     printf("%02X", ((char*)buf)[i]);
    // }
    // printf("\n");
    return ret;
}

ABI_ATTR 


ABI_ATTR ssize_t write_impl(int fd, void *buf, size_t count)
{
    // verbose("NATIVE","writing %zu bytes to file %d",count,fd);
    // int i;
    // for (i = 0; i < count; i++)
    // {
    //     if (i > 0) printf(":");
    //     printf("%02X", ((char*)buf)[i]);
    // }
    // printf("\n");
    return write(fd, buf, count);
}

ABI_ATTR int close_impl(int fd)
{
    verbose("NATIVE","Closing file %d",fd);
    return close(fd);
}

ABI_ATTR DIR* opendir_impl(const char* path) {
    char* clean_path = clean_jar_path(path);
    if (!clean_path) return NULL;
    DIR* dir = opendir(clean_path);
    free(clean_path);
    return dir;
}

// fstatat_impl
ABI_ATTR int fstatat_impl(int dirfd, const char* path, struct stat* buf, int flags) {
    verbose("NATIVE","fstatat(%d, %s, flags=%d)", dirfd, path, flags);
    char* clean_path = clean_jar_path(path);
    if (!clean_path) {
        return -1;
    }
    int ret = fstatat(dirfd, clean_path, buf, flags);
    free(clean_path);
    return ret;
}

// stat
ABI_ATTR int stat_impl(const char* path, struct stat* buf) {
    verbose("NATIVE", "stat(%s)", path);
    char* clean_path = clean_jar_path(path);
    if (!clean_path) {
        return -1;
    }
    int ret = stat(clean_path, buf);
    free(clean_path);
    return ret;
}

// lstat
ABI_ATTR int lstat_impl(const char* path, struct stat* buf) {
    verbose("NATIVE","lstat(%s)", path);
    char* clean_path = clean_jar_path(path);
    if (!clean_path) {
        return -1;
    }
    int ret = lstat(clean_path, buf);
    free(clean_path);
    return ret;
}

// ABI_ATTR int chdir_bridge(const char* dir)
// {
//     verbose("NATIVE","Changing directory to %s",dir);
//     return chdir(dir);
// }


// //Maybe this could be int fd, int unused, long offset, int whence 
// //needs to be investigated
// ABI_ATTR long lseek64_bridge()
// {
//     //Father, forgive me for i have sinned
//     int stackPointer, fd, off_low, off_high;
//     asm volatile (
//         "mov %0, sp\n"
//         "mov %1, r0\n"
//         "mov %2, r2\n"
//         "mov %3, r3\n"
//         : "=r" (stackPointer), "=r" (fd), "=r" (off_low), "=r" (off_high)
//         :
//         : "memory", "r0", "r2", "r3"
//     );
//     int whence=*((int*)stackPointer+0x9);
//     //Do not put any lines of code before this point, this is very fragile
//     verbose("NATIVE","fd=%d whence=%d off_low=%d off_high=%d",fd,whence,off_low,off_high);    
//     long new_off=lseek64(fd,off_low+(off_high<<32),whence);
//     return new_off;
// }


/* flock() operation flags */
#ifndef LOCK_SH
#define LOCK_SH 1    /* shared lock */
#endif
#ifndef LOCK_EX
#define LOCK_EX 2    /* exclusive lock */
#endif
#ifndef LOCK_UN
#define LOCK_UN 8    /* unlock */
#endif
#ifndef LOCK_NB
#define LOCK_NB 4    /* non-blocking */
#endif

ABI_ATTR int flock_impl(int fd, int operation) {
    struct flock fl = {0};

    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;   /* whole file */

    switch (operation & (LOCK_SH | LOCK_EX | LOCK_UN)) {
        case LOCK_SH:
            fl.l_type = F_RDLCK;
            break;
        case LOCK_EX:
            fl.l_type = F_WRLCK;
            break;
        case LOCK_UN:
            fl.l_type = F_UNLCK;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    int cmd = (operation & LOCK_NB) ? F_SETLK : F_SETLKW;

    return fcntl(fd, cmd, &fl);
}

