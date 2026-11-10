// libc pieces Unity 7 asks for that the table didn't have: POSIX regex (bionic's structs and flag values differ
// from glibc's, so it's wrapped), openat/statvfs, and a few math functions (same ABI on aarch64, mapped directly).
#include "platform.h"
#include "logging.h"
#include <regex.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <stdarg.h>

char* clean_jar_path(const char* path);

///// regex

// bionic's (NetBSD-derived) layout
struct bionic_regex_t {
    int re_magic;
    size_t re_nsub;
    const char* re_endp;
    void* re_g;          // we keep the host regex_t here
};
struct bionic_regmatch_t {
    long rm_so;          // regoff_t is off_t (64-bit) on bionic, int on glibc
    long rm_eo;
};

static const int BIONIC_REG_EXTENDED = 0x01, BIONIC_REG_ICASE = 0x02, BIONIC_REG_NOSUB = 0x04, BIONIC_REG_NEWLINE = 0x08;
static const int BIONIC_REG_NOTBOL = 0x01, BIONIC_REG_NOTEOL = 0x02;

extern "C" ABI_ATTR int regcomp_impl(bionic_regex_t* preg, const char* pattern, int cflags)
{
    int f = 0;
    if (cflags & BIONIC_REG_EXTENDED) f |= REG_EXTENDED;
    if (cflags & BIONIC_REG_ICASE) f |= REG_ICASE;
    if (cflags & BIONIC_REG_NOSUB) f |= REG_NOSUB;
    if (cflags & BIONIC_REG_NEWLINE) f |= REG_NEWLINE;
    regex_t* r = (regex_t*)malloc(sizeof(regex_t));
    int ret = regcomp(r, pattern, f);
    if (ret != 0) { free(r); preg->re_g = nullptr; return ret; }
    preg->re_magic = 0x62706572;
    preg->re_nsub = r->re_nsub;
    preg->re_endp = nullptr;
    preg->re_g = r;
    return 0;
}

extern "C" ABI_ATTR int regexec_impl(const bionic_regex_t* preg, const char* string, size_t nmatch, bionic_regmatch_t* pmatch, int eflags)
{
    if (!preg || !preg->re_g) return REG_BADPAT;
    int f = 0;
    if (eflags & BIONIC_REG_NOTBOL) f |= REG_NOTBOL;
    if (eflags & BIONIC_REG_NOTEOL) f |= REG_NOTEOL;
    regmatch_t* m = nmatch && pmatch ? (regmatch_t*)calloc(nmatch, sizeof(regmatch_t)) : nullptr;
    int ret = regexec((regex_t*)preg->re_g, string, m ? nmatch : 0, m, f);
    if (m) {
        for (size_t i = 0; i < nmatch; i++) { pmatch[i].rm_so = m[i].rm_so; pmatch[i].rm_eo = m[i].rm_eo; }
        free(m);
    }
    return ret;
}

extern "C" ABI_ATTR void regfree_impl(bionic_regex_t* preg)
{
    if (preg && preg->re_g) { regfree((regex_t*)preg->re_g); free(preg->re_g); preg->re_g = nullptr; }
}

extern "C" ABI_ATTR size_t regerror_impl(int errcode, const bionic_regex_t* preg, char* buf, size_t size)
{
    return regerror(errcode, preg ? (const regex_t*)preg->re_g : nullptr, buf, size);
}

///// files

extern "C" ABI_ATTR int openat_impl(int dirfd, const char* path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    verbose("NATIVE", "openat(%d, %s)", dirfd, path);
    char* clean = clean_jar_path(path);
    int fd = openat(dirfd, clean ? clean : path, flags, mode);
    free(clean);
    return fd;
}

extern "C" ABI_ATTR int statvfs_impl(const char* path, struct statvfs* buf)
{
    // struct statvfs has the same layout in bionic and glibc on aarch64
    char* clean = clean_jar_path(path);
    int r = statvfs(clean ? clean : path, buf);
    free(clean);
    return r;
}
