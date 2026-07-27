/* imports.c -- .so import resolution table for libll1.so (Level-5 LT1R)
 *
 * Every undefined symbol libll1.so imports (per readelf --dyn-syms) is mapped
 * here to a native implementation: newlib/libnx for plain libc, the bionic
 * shims in libc_shim.c where the ABI differs, mesa for GLES2, the mini OpenSL
 * ES in opensl.c for audio, and the AAsset emulation for asset reads.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#define _GNU_SOURCE // vasprintf

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <GLES2/gl2.h>
#include <switch.h>

#include "config.h"
#include "so_util.h"
#include "util.h"
#include "libc_shim.h"
#include "opensl.h"

extern uint8_t fake_sF[3][0x100];

static uint64_t __stack_chk_guard_fake = 0x4242424242424242;
static char *__ctype_ptr_fake = (char *)&_ctype_;

extern void *__stack_chk_fail;

// ---------------------------------------------------------------------------
// pthread wrappers: bionic stores these objects inline (the storage is wide
// enough on LP64 to stash a newlib pointer in the first word)
// ---------------------------------------------------------------------------

// bionic mutexes are stored inline; the game's static initializers leave a
// small sentinel in the first word (0=normal, 0x4000=recursive,
// 0x8000=errorcheck). create a real newlib mutex of the matching type and
// stash its pointer in that word. NOTE: the type must go through a real
// mutexattr -- writing PTHREAD_RECURSIVE_MUTEX_INITIALIZER then calling
// pthread_mutex_init(m, NULL) silently produces a NORMAL mutex, which makes
// the engine's recursive locks self-deadlock.
// ---------------------------------------------------------------------------
// vsync turnstile mutexes (see OS_Run): the engine locks these on one thread
// and unlocks them on another to keep the render thread and the NitroMain
// logic thread in lockstep. libnx mutexes enforce ownership and won't allow a
// cross-thread unlock, so route just these two through binary semaphores,
// which can be signalled from any thread.
// ---------------------------------------------------------------------------

static void *g_turnstile[2];

void register_turnstile_mutexes(void *a, void *b) {
  g_turnstile[0] = a;
  g_turnstile[1] = b;
}

static int is_turnstile(void *uid) {
  return uid == g_turnstile[0] || uid == g_turnstile[1];
}

static Semaphore *turnstile_sem(pthread_mutex_t **uid) {
  if ((uintptr_t)*uid <= 0x8000) { // still a bionic sentinel, not a real ptr
    Semaphore *s = calloc(1, sizeof(Semaphore));
    semaphoreInit(s, 1); // a mutex starts unlocked
    *uid = (pthread_mutex_t *)s;
  }
  return (Semaphore *)*uid;
}

static int mutex_init_typed(pthread_mutex_t **uid, int type) {
  pthread_mutex_t *m = calloc(1, sizeof(pthread_mutex_t));
  if (!m) return -1;
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, type);
  int ret = pthread_mutex_init(m, &attr);
  pthread_mutexattr_destroy(&attr);
  if (ret != 0) { free(m); return -1; }
  *uid = m;
  return 0;
}
static int bionic_mutex_type(const int *attr) {
  if (attr) {
    if (*attr == 1) return PTHREAD_MUTEX_RECURSIVE;
    if (*attr == 2) return PTHREAD_MUTEX_ERRORCHECK;
  }
  return PTHREAD_MUTEX_NORMAL;
}
static int pthread_mutex_init_fake(pthread_mutex_t **uid, const int *attr) {
  if (is_turnstile(uid)) {
    *uid = NULL;          // drop any stale handle
    turnstile_sem(uid);   // (re)create the semaphore, unlocked
    return 0;
  }
  return mutex_init_typed(uid, bionic_mutex_type(attr));
}
static int pthread_mutex_destroy_fake(pthread_mutex_t **uid) {
  if (is_turnstile(uid))
    return 0; // keep the turnstile semaphore alive for the whole session
  if (uid && *uid && (uintptr_t)*uid > 0x8000) {
    pthread_mutex_destroy(*uid);
    free(*uid);
    *uid = NULL;
  }
  return 0;
}
static int ensure_mutex(pthread_mutex_t **uid) {
  switch ((uintptr_t)*uid) {
    case 0:      return mutex_init_typed(uid, PTHREAD_MUTEX_NORMAL);
    case 0x4000: return mutex_init_typed(uid, PTHREAD_MUTEX_RECURSIVE);
    case 0x8000: return mutex_init_typed(uid, PTHREAD_MUTEX_ERRORCHECK);
    default:     return 0; // already a real mutex pointer
  }
}
int pthread_mutex_lock_fake(pthread_mutex_t **uid) {
  if (is_turnstile(uid)) { semaphoreWait(turnstile_sem(uid)); return 0; }
  int r = ensure_mutex(uid); if (r < 0) return r;
  return pthread_mutex_lock(*uid);
}
int pthread_mutex_unlock_fake(pthread_mutex_t **uid) {
  if (is_turnstile(uid)) { semaphoreSignal(turnstile_sem(uid)); return 0; }
  int r = ensure_mutex(uid); if (r < 0) return r;
  return pthread_mutex_unlock(*uid);
}
static int pthread_cond_init_fake(pthread_cond_t **cnd, const int *attr) {
  (void)attr;
  pthread_cond_t *c = calloc(1, sizeof(pthread_cond_t));
  if (!c) return -1;
  *c = PTHREAD_COND_INITIALIZER;
  if (pthread_cond_init(c, NULL) < 0) { free(c); return -1; }
  *cnd = c;
  return 0;
}
static int pthread_cond_destroy_fake(pthread_cond_t **cnd) {
  if (cnd && *cnd) { pthread_cond_destroy(*cnd); free(*cnd); *cnd = NULL; }
  return 0;
}
static int pthread_cond_signal_fake(pthread_cond_t **cnd) {
  if (!*cnd && pthread_cond_init_fake(cnd, NULL) < 0) return -1;
  return pthread_cond_signal(*cnd);
}
static int pthread_cond_broadcast_fake(pthread_cond_t **cnd) {
  if (!*cnd && pthread_cond_init_fake(cnd, NULL) < 0) return -1;
  return pthread_cond_broadcast(*cnd);
}
static int pthread_cond_wait_fake(pthread_cond_t **cnd, pthread_mutex_t **mtx) {
  if (!*cnd && pthread_cond_init_fake(cnd, NULL) < 0) return -1;
  return pthread_cond_wait(*cnd, *mtx);
}
static int pthread_cond_timedwait_fake(pthread_cond_t **cnd, pthread_mutex_t **mtx, const struct timespec *t) {
  if (!*cnd && pthread_cond_init_fake(cnd, NULL) < 0) return -1;
  return pthread_cond_timedwait(*cnd, *mtx, t);
}
static int pthread_once_fake(volatile int *once, void (*init)(void)) {
  if (!once || !init) return -1;
  if (__sync_lock_test_and_set(once, 1) == 0) init();
  return 0;
}
// every thread that runs game code needs its own bionic TLS in TPIDR_EL0 for
// the stack-protector cookie reads; wrap the entry point to install one. the
// block is leaked on purpose -- it must outlive the thread (TPIDR_EL0 points
// into it until teardown).
typedef struct {
  void *(*entry)(void *);
  void *arg;
  uint8_t tls[BIONIC_TLS_SIZE];
} ThreadStart;

static void *pthread_trampoline(void *p) {
  ThreadStart *s = p;
  void *(*entry)(void *) = s->entry;
  void *arg = s->arg;
  install_bionic_tls(s->tls);
  return entry(arg);
}

static int pthread_create_fake(pthread_t *thread, const void *attr, void *entry, void *arg) {
  (void)attr;
  ThreadStart *s = calloc(1, sizeof(*s));
  if (!s)
    return -1;
  s->entry = entry;
  s->arg = arg;
  return pthread_create(thread, NULL, pthread_trampoline, s);
}

// ---------------------------------------------------------------------------
// misc
// ---------------------------------------------------------------------------

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
#ifdef DEBUG_LOG
  va_list va; static char s[0x1000];
  va_start(va, fmt); vsnprintf(s, sizeof(s), fmt, va); va_end(va);
  debugPrintf("%s: %s\n", tag, s);
#else
  (void)prio; (void)tag; (void)fmt;
#endif
  return 0;
}
int __android_log_vprint(int prio, const char *tag, const char *fmt, va_list va) {
#ifdef DEBUG_LOG
  static char s[0x1000];
  vsnprintf(s, sizeof(s), fmt, va);
  debugPrintf("%s: %s\n", tag, s);
#else
  (void)prio; (void)tag; (void)fmt; (void)va;
#endif
  return 0;
}
int __android_log_write(int prio, const char *tag, const char *text) {
  (void)prio; debugPrintf("%s: %s\n", tag, text); return 0;
}

static int cxa_atexit_fake(void (*fn)(void *), void *arg, void *dso) {
  (void)fn; (void)arg; (void)dso; return 0;
}
static int getpriority_fake(int which, int who) { (void)which; (void)who; return 0; }
static int sched_yield_fake(void) { svcSleepThread(0); return 0; }

// ---------------------------------------------------------------------------
// GL target fixups: the movie texture is a plain GL_TEXTURE_2D here (no
// SurfaceTexture), but the engine's GL_DrawMovie addresses it as
// GL_TEXTURE_EXTERNAL_OES, which mesa rejects
// ---------------------------------------------------------------------------

#define GL_TEXTURE_EXTERNAL_OES_ENUM 0x8D65

static void glBindTexture_hook(GLenum target, GLuint tex) {
  if (target == GL_TEXTURE_EXTERNAL_OES_ENUM)
    target = GL_TEXTURE_2D;
  glBindTexture(target, tex);
}

static void glTexParameteri_hook(GLenum target, GLenum pname, GLint param) {
  if (target == GL_TEXTURE_EXTERNAL_OES_ENUM)
    target = GL_TEXTURE_2D;
  glTexParameteri(target, pname, param);
}

static void glEnable_hook(GLenum cap) {
  if (cap != GL_TEXTURE_EXTERNAL_OES_ENUM)
    glEnable(cap);
}

static void glDisable_hook(GLenum cap) {
  if (cap != GL_TEXTURE_EXTERNAL_OES_ENUM)
    glDisable(cap);
}

// the movie fragment shader samples a SurfaceTexture (samplerExternalOES);
// swap it for the equivalent sampler2D version (same uniform names, so the
// engine's glGetUniformLocation calls keep working)
static const char *patched_frag =
  "precision highp float;"
  "uniform sampler2D texture;uniform vec4 color;varying vec2 vary_uv;"
  "void main(){ gl_FragColor = texture2D(texture, vary_uv) * color; }";

static void glShaderSource_hook(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length) {
  if (count >= 1 && string && string[0] && strstr(string[0], "samplerExternalOES"))
    glShaderSource(shader, 1, &patched_frag, NULL);
  else
    glShaderSource(shader, count, string, length);
}

// ---------------------------------------------------------------------------
// table
// ---------------------------------------------------------------------------

DynLibFunction dynlib_functions[] = {
  { "__cxa_atexit", (uintptr_t)&cxa_atexit_fake },
  { "__cxa_finalize", (uintptr_t)&ret0 },
  { "__errno", (uintptr_t)&__errno },
  { "__sF", (uintptr_t)&fake_sF },
  { "__stack_chk_fail", (uintptr_t)&__stack_chk_fail },
  { "__stack_chk_guard", (uintptr_t)&__stack_chk_guard_fake },
  { "_ctype_", (uintptr_t)&__ctype_ptr_fake },

  { "__memcpy_chk", (uintptr_t)&__memcpy_chk_fake },
  { "__memmove_chk", (uintptr_t)&__memmove_chk_fake },
  { "__strcat_chk", (uintptr_t)&__strcat_chk_fake },
  { "__strcpy_chk", (uintptr_t)&__strcpy_chk_fake },
  { "__strlen_chk", (uintptr_t)&__strlen_chk_fake },
  { "__strncpy_chk2", (uintptr_t)&__strncpy_chk2_fake },
  { "__vsnprintf_chk", (uintptr_t)&__vsnprintf_chk_fake },
  { "__vsprintf_chk", (uintptr_t)&__vsprintf_chk_fake },

  { "__register_atfork", (uintptr_t)&__register_atfork_fake },
  { "android_set_abort_message", (uintptr_t)&android_set_abort_message_fake },
  { "__android_log_print", (uintptr_t)&__android_log_print },
  { "__android_log_vprint", (uintptr_t)&__android_log_vprint },
  { "__android_log_write", (uintptr_t)&__android_log_write },

  // AAsset emulation
  { "AAssetManager_open", (uintptr_t)&AAssetManager_open_fake },
  { "AAssetManager_fromJava", (uintptr_t)&AAssetManager_fromJava_fake },
  { "AAsset_close", (uintptr_t)&AAsset_close_fake },
  { "AAsset_read", (uintptr_t)&AAsset_read_fake },
  { "AAsset_seek", (uintptr_t)&AAsset_seek_fake },
  { "AAsset_getLength", (uintptr_t)&AAsset_getLength_fake },
  { "AAsset_getLength64", (uintptr_t)&AAsset_getLength64_fake },
  { "AAsset_openFileDescriptor64", (uintptr_t)&AAsset_openFileDescriptor64_fake },

  // OpenSL ES (audio)
  { "slCreateEngine", (uintptr_t)&slCreateEngine },
  { "SL_IID_ENGINE", (uintptr_t)&SL_IID_ENGINE },
  { "SL_IID_PLAY", (uintptr_t)&SL_IID_PLAY },
  { "SL_IID_BUFFERQUEUE", (uintptr_t)&SL_IID_BUFFERQUEUE },
  { "SL_IID_VOLUME", (uintptr_t)&SL_IID_VOLUME },
  { "SL_IID_ANDROIDCONFIGURATION", (uintptr_t)&SL_IID_ANDROIDCONFIGURATION },

  // GLES2 (mesa)
  { "glAttachShader", (uintptr_t)&glAttachShader },
  { "glBindTexture", (uintptr_t)&glBindTexture_hook },
  { "glBlendFunc", (uintptr_t)&glBlendFunc },
  { "glClear", (uintptr_t)&glClear },
  { "glClearColor", (uintptr_t)&glClearColor },
  { "glCompileShader", (uintptr_t)&glCompileShader },
  { "glCreateProgram", (uintptr_t)&glCreateProgram },
  { "glCreateShader", (uintptr_t)&glCreateShader },
  { "glCullFace", (uintptr_t)&glCullFace },
  { "glDeleteProgram", (uintptr_t)&glDeleteProgram },
  { "glDeleteShader", (uintptr_t)&glDeleteShader },
  { "glDeleteTextures", (uintptr_t)&glDeleteTextures },
  { "glDepthFunc", (uintptr_t)&glDepthFunc },
  { "glDisable", (uintptr_t)&glDisable_hook },
  { "glDisableVertexAttribArray", (uintptr_t)&glDisableVertexAttribArray },
  { "glDrawArrays", (uintptr_t)&glDrawArrays },
  { "glEnable", (uintptr_t)&glEnable_hook },
  { "glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray },
  { "glGenTextures", (uintptr_t)&glGenTextures },
  { "glGetAttribLocation", (uintptr_t)&glGetAttribLocation },
  { "glGetProgramiv", (uintptr_t)&glGetProgramiv },
  { "glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog },
  { "glGetShaderiv", (uintptr_t)&glGetShaderiv },
  { "glGetUniformLocation", (uintptr_t)&glGetUniformLocation },
  { "glLinkProgram", (uintptr_t)&glLinkProgram },
  { "glScissor", (uintptr_t)&glScissor },
  { "glShaderSource", (uintptr_t)&glShaderSource_hook },
  { "glTexImage2D", (uintptr_t)&glTexImage2D },
  { "glTexParameteri", (uintptr_t)&glTexParameteri_hook },
  { "glUniform4f", (uintptr_t)&glUniform4f },
  { "glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv },
  { "glUseProgram", (uintptr_t)&glUseProgram },
  { "glVertexAttribPointer", (uintptr_t)&glVertexAttribPointer },
  { "glViewport", (uintptr_t)&glViewport },

  // math
  { "acosf", (uintptr_t)&acosf },
  { "asinf", (uintptr_t)&asinf },
  { "atan2", (uintptr_t)&atan2 },
  { "atan2f", (uintptr_t)&atan2f },
  { "cos", (uintptr_t)&cos },
  { "cosf", (uintptr_t)&cosf },
  { "exp2f", (uintptr_t)&exp2f },
  { "expf", (uintptr_t)&expf },
  { "fmodf", (uintptr_t)&fmodf },
  { "log10f", (uintptr_t)&log10f },
  { "logf", (uintptr_t)&logf },
  { "modf", (uintptr_t)&modf },
  { "pow", (uintptr_t)&pow },
  { "powf", (uintptr_t)&powf },
  { "sin", (uintptr_t)&sin },
  { "sinf", (uintptr_t)&sinf },
  { "sincosf", (uintptr_t)&sincosf_fake },
  { "tan", (uintptr_t)&tan },
  { "tanf", (uintptr_t)&tanf },
  { "div", (uintptr_t)&div },

  // memory / stdlib
  { "calloc", (uintptr_t)&calloc },
  { "free", (uintptr_t)&free },
  { "malloc", (uintptr_t)&malloc },
  { "realloc", (uintptr_t)&realloc },
  { "posix_memalign", (uintptr_t)&posix_memalign_fake },
  { "abort", (uintptr_t)&abort },
  { "atoi", (uintptr_t)&atoi },
  { "qsort", (uintptr_t)&qsort },
  { "rand", (uintptr_t)&rand },
  { "srand", (uintptr_t)&srand },

  // mem/str
  { "memchr", (uintptr_t)&memchr },
  { "memcmp", (uintptr_t)&memcmp },
  { "memcpy", (uintptr_t)&memcpy },
  { "memmove", (uintptr_t)&memmove },
  { "memset", (uintptr_t)&memset },
  { "strcasecmp", (uintptr_t)&strcasecmp },
  { "strcat", (uintptr_t)&strcat },
  { "strcmp", (uintptr_t)&strcmp },
  { "strcpy", (uintptr_t)&strcpy },
  { "strlen", (uintptr_t)&strlen },
  { "strncat", (uintptr_t)&strncat },
  { "strncmp", (uintptr_t)&strncmp },
  { "strncpy", (uintptr_t)&strncpy },
  { "strrchr", (uintptr_t)&strrchr },
  { "strstr", (uintptr_t)&strstr },
  { "strtod", (uintptr_t)&strtod },
  { "strtof", (uintptr_t)&strtof },
  { "strtol", (uintptr_t)&strtol },
  { "strtold", (uintptr_t)&strtold },
  { "strtoll", (uintptr_t)&strtoll },
  { "strtoul", (uintptr_t)&strtoul },
  { "strtoull", (uintptr_t)&strtoull },

  // wide
  { "swprintf", (uintptr_t)&swprintf },
  { "wcslen", (uintptr_t)&wcslen },
  { "wcstod", (uintptr_t)&wcstod },
  { "wcstof", (uintptr_t)&wcstof },
  { "wcstol", (uintptr_t)&wcstol },
  { "wcstold", (uintptr_t)&wcstold },
  { "wcstoll", (uintptr_t)&wcstoll },
  { "wcstoul", (uintptr_t)&wcstoul },
  { "wcstoull", (uintptr_t)&wcstoull },
  { "wmemchr", (uintptr_t)&wmemchr },
  { "wmemcmp", (uintptr_t)&wmemcmp },
  { "wmemcpy", (uintptr_t)&wmemcpy },
  { "wmemmove", (uintptr_t)&wmemmove },
  { "wmemset", (uintptr_t)&wmemset },

  // formatted output
  { "snprintf", (uintptr_t)&snprintf },
  { "sscanf", (uintptr_t)&sscanf },
  { "vasprintf", (uintptr_t)&vasprintf },
  { "vfprintf", (uintptr_t)&vfprintf_fake },
  { "vsnprintf", (uintptr_t)&vsnprintf },
  { "vsprintf", (uintptr_t)&vsprintf },

  // stdio (bionic __sF aware)
  { "fopen", (uintptr_t)&fopen },
  { "fclose", (uintptr_t)&fclose_fake },
  { "fdopen", (uintptr_t)&fdopen },
  { "ferror", (uintptr_t)&ferror_fake },
  { "fflush", (uintptr_t)&fflush_fake },
  { "fileno", (uintptr_t)&fileno_fake },
  { "fprintf", (uintptr_t)&fprintf_fake },
  { "fputc", (uintptr_t)&fputc_fake },
  { "fread", (uintptr_t)&fread_fake },
  { "fseek", (uintptr_t)&fseek_fake },
  { "ftell", (uintptr_t)&ftell },
  { "fwrite", (uintptr_t)&fwrite_fake },
  { "clearerr", (uintptr_t)&clearerr },

  // fs / posix
  { "fstat", (uintptr_t)&fstat_fake },
  { "stat", (uintptr_t)&stat_fake },
  { "remove", (uintptr_t)&remove },
  { "rename", (uintptr_t)&rename },
  { "dl_iterate_phdr", (uintptr_t)&so_dl_iterate_phdr },
  { "dlopen", (uintptr_t)&ret0 },
  { "dlsym", (uintptr_t)&ret0 },
  { "dlclose", (uintptr_t)&ret0 },

  // time
  { "clock_gettime", (uintptr_t)&clock_gettime },
  { "localtime", (uintptr_t)&localtime },
  { "time", (uintptr_t)&time },
  { "nanosleep", (uintptr_t)&nanosleep },

  // pthread
  { "pthread_attr_init", (uintptr_t)&ret0 },
  { "pthread_attr_setschedparam", (uintptr_t)&ret0 },
  { "pthread_attr_setschedpolicy", (uintptr_t)&ret0 },
  { "pthread_attr_setstacksize", (uintptr_t)&ret0 },
  { "pthread_condattr_init", (uintptr_t)&ret0 },
  { "pthread_condattr_destroy", (uintptr_t)&ret0 },
  { "pthread_condattr_setclock", (uintptr_t)&ret0 },
  { "pthread_cond_init", (uintptr_t)&pthread_cond_init_fake },
  { "pthread_cond_destroy", (uintptr_t)&pthread_cond_destroy_fake },
  { "pthread_cond_signal", (uintptr_t)&pthread_cond_signal_fake },
  { "pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake },
  { "pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake },
  { "pthread_cond_timedwait", (uintptr_t)&pthread_cond_timedwait_fake },
  { "pthread_create", (uintptr_t)&pthread_create_fake },
  { "pthread_join", (uintptr_t)&pthread_join },
  { "pthread_self", (uintptr_t)&pthread_self },
  { "pthread_getschedparam", (uintptr_t)&ret0 },
  { "pthread_setname_np", (uintptr_t)&ret0 },
  { "pthread_getspecific", (uintptr_t)&pthread_getspecific },
  { "pthread_setspecific", (uintptr_t)&pthread_setspecific },
  { "pthread_key_create", (uintptr_t)&pthread_key_create },
  { "pthread_key_delete", (uintptr_t)&pthread_key_delete },
  { "pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake },
  { "pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake },
  { "pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake },
  { "pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake },
  { "pthread_once", (uintptr_t)&pthread_once_fake },
  { "pthread_rwlock_rdlock", (uintptr_t)&pthread_rwlock_rdlock_fake },
  { "pthread_rwlock_wrlock", (uintptr_t)&pthread_rwlock_wrlock_fake },
  { "pthread_rwlock_unlock", (uintptr_t)&pthread_rwlock_unlock_fake },

  // scheduling / process odds and ends
  { "getpriority", (uintptr_t)&getpriority_fake },
  { "setpriority", (uintptr_t)&ret0 },
  { "sched_yield", (uintptr_t)&sched_yield_fake },
  { "gettid", (uintptr_t)&gettid_fake },
  { "syscall", (uintptr_t)&syscall_fake },
  { "openlog", (uintptr_t)&ret0 },
  { "closelog", (uintptr_t)&ret0 },
  { "syslog", (uintptr_t)&ret0 },
};

size_t dynlib_numfunctions = sizeof(dynlib_functions) / sizeof(*dynlib_functions);
