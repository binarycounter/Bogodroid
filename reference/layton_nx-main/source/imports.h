/* imports.h -- .so import resolution table
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef __IMPORTS_H__
#define __IMPORTS_H__

#include <pthread.h>
#include "so_util.h"

extern DynLibFunction dynlib_functions[];
extern size_t dynlib_numfunctions;

// the engine's two vsync-turnstile mutexes are locked on one thread and
// unlocked on another; register their addresses so the pthread shims route
// them through cross-thread-capable semaphores instead of libnx mutexes
void register_turnstile_mutexes(void *a, void *b);

#endif
