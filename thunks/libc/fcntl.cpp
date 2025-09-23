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

ABI_ATTR int open_impl(const char *filename, int flags)
{
    verbose("NATIVE","Opening file %s",filename);

    if (strcmp(filename,"/proc/cpuinfo") == 0)
    {
        filename = "../support_files/cpuinfo.txt";
        verbose("NATIVE","Changing cpuinfo request to fake cpuinfo.txt");
    }

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

    int fd = open(filename, flags);
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
    verbose("NATIVE","writing %zu bytes to file %d",count,fd);
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

char* clean_jar_path(const char* path) {
    if (!path) return NULL;

    size_t len = strlen(path);
    char* clean_path = (char*)malloc(len + 1);
    if (!clean_path) return NULL;

    const char* needles[] = {"jar:file:/!", "jar:file://!"};
    size_t num_needles = sizeof(needles) / sizeof(needles[0]);

    const char* src = path;
    char* dst = clean_path;

    while (*src) {
        int matched = 0;
        for (size_t i = 0; i < num_needles; ++i) {
            size_t needle_len = strlen(needles[i]);
            if (strncmp(src, needles[i], needle_len) == 0) {
                src += needle_len; // skip the substring
                matched = 1;
                break;
            }
        }
        if (!matched) {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return clean_path;
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
    char* clean_path = clean_jar_path(path);
    if (!clean_path) {
        return -1;
    }
    int ret = fstatat(dirfd, clean_path, buf, flags);
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




