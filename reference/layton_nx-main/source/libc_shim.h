/* libc_shim.h -- bionic-compatible libc wrappers
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef __LIBC_SHIM_H__
#define __LIBC_SHIM_H__

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>

// fortify
void *__memcpy_chk_fake(void *dst, const void *src, size_t n, size_t dstlen);
void *__memmove_chk_fake(void *dst, const void *src, size_t n, size_t dstlen);
char *__strcat_chk_fake(char *dst, const char *src, size_t dstlen);
char *__strcpy_chk_fake(char *dst, const char *src, size_t dstlen);
size_t __strlen_chk_fake(const char *s, size_t slen);
char *__strncpy_chk2_fake(char *dst, const char *src, size_t n, size_t dstlen, size_t srclen);
int __vsnprintf_chk_fake(char *s, size_t maxlen, int flag, size_t slen, const char *fmt, va_list va);
int __vsprintf_chk_fake(char *s, int flag, size_t slen, const char *fmt, va_list va);

// misc bionic
int gettid_fake(void);
long syscall_fake(long number, ...);
void sincosf_fake(float x, float *s, float *c);
void android_set_abort_message_fake(const char *msg);
int __register_atfork_fake(void);

// struct stat conversion (bionic aarch64 layout)
struct bionic_stat;
int stat_fake(const char *path, struct bionic_stat *st);
int fstat_fake(int fd, struct bionic_stat *st);

// memory
int posix_memalign_fake(void **out, size_t align, size_t size);

// stdio over the fake bionic __sF (stdin/stdout/stderr)
extern uint8_t fake_sF[3][0x100];
size_t fwrite_fake(const void *ptr, size_t size, size_t n, FILE *f);
size_t fread_fake(void *ptr, size_t size, size_t n, FILE *f);
int fputc_fake(int c, FILE *f);
int fflush_fake(FILE *f);
int fclose_fake(FILE *f);
int ferror_fake(FILE *f);
int fileno_fake(FILE *f);
int fprintf_fake(FILE *f, const char *fmt, ...);
int vfprintf_fake(FILE *f, const char *fmt, va_list va);
int fseek_fake(FILE *f, long off, int whence);

// AAsset emulation over loose files in the assets folder
const char *asset_path(const char *rel);
void *AAssetManager_fromJava_fake(void *env, void *mgr);
void *AAssetManager_open_fake(void *mgr, const char *path, int mode);
int AAsset_openFileDescriptor64_fake(void *asset, int64_t *outStart, int64_t *outLength);
void AAsset_close_fake(void *a);
int AAsset_read_fake(void *a, void *buf, size_t count);
long AAsset_seek_fake(void *a, long off, int whence);
long AAsset_getLength_fake(void *a);
int64_t AAsset_getLength64_fake(void *a);

// pthread rwlocks (bionic stores them inline; we stash a real object pointer)
int pthread_rwlock_rdlock_fake(void **rw);
int pthread_rwlock_wrlock_fake(void **rw);
int pthread_rwlock_unlock_fake(void **rw);

#endif
