/* util.h -- misc utility functions
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>

int debugPrintf(char *text, ...);

void cpu_boost(int on);

int ret0(void);

// Point TPIDR_EL0 at a zeroed block so the game's stack-protector prologues
// (which read the canary from TPIDR_EL0+0x28) have a valid TLS. `buf` must
// outlive the thread. libnx keeps its own thread state in TPIDR_RO_EL0, so
// commandeering TPIDR_EL0 is safe.
void install_bionic_tls(void *buf);
#define BIONIC_TLS_SIZE 0x100

static inline void armSetTlsRw(void *addr) {
  __asm__  ("msr s3_3_c13_c0_2, %0" : : "r"(addr));
}

static inline uint64_t umin(uint64_t a, uint64_t b) {
  return (a < b) ? a : b;
}

#endif
