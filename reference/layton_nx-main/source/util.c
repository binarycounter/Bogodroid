/* util.c -- misc utility functions
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <switch.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "util.h"
#include "config.h"

int debugPrintf(char *text, ...) {
#ifdef DEBUG_LOG
  // the file stays open across calls; fflush keeps the log durable through a
  // crash. a mutex serializes the threads that log.
  static Mutex lock;
  static FILE *f = NULL;
  mutexLock(&lock);
  if (!f)
    f = fopen(LOG_NAME, "w");
  if (f) {
    va_list list;
    va_start(list, text);
    vfprintf(f, text, list);
    va_end(list);
    fflush(f);
  }
  mutexUnlock(&lock);
#endif
  return 0;
}

void cpu_boost(int on) {
  appletSetCpuBoostMode(on ? ApmCpuBoostMode_FastLoad : ApmCpuBoostMode_Normal);
}

int ret0(void) { return 0; }

void install_bionic_tls(void *buf) {
  memset(buf, 0, BIONIC_TLS_SIZE);
  armSetTlsRw(buf);
}
