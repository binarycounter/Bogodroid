/* opensl.h -- minimal OpenSL ES implementation for the Switch
 *
 * libll1.so plays all of its audio through CRI ADX2, whose Android backend
 * (criNcvAndroidSLES) drives a standard OpenSL ES buffer-queue player. The
 * Switch has no OpenSL ES, so this provides just enough of the 1.0.1 API --
 * an engine, an output mix and a PCM buffer-queue player -- on top of libnx
 * audout.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef __OPENSL_H__
#define __OPENSL_H__

#include <stdint.h>

// brings up audout; safe to call once at startup
int opensl_init(void);
void opensl_exit(void);

// resolved by the import table (exact OpenSL ES symbol names)
uint32_t slCreateEngine(void **pEngine, uint32_t numOptions, const void *pEngineOptions,
                        uint32_t numInterfaces, const void *pInterfaceIds, const uint8_t *pInterfaceRequired);

extern const void *SL_IID_ENGINE;
extern const void *SL_IID_PLAY;
extern const void *SL_IID_BUFFERQUEUE;
extern const void *SL_IID_VOLUME;
extern const void *SL_IID_ANDROIDCONFIGURATION;
extern const void *SL_IID_ANDROIDSIMPLEBUFFERQUEUE;

#endif
