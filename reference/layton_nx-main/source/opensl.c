/* opensl.c -- minimal OpenSL ES implementation for the Switch
 *
 * Implements the slice of OpenSL ES 1.0.1 that CRI ADX2's Android SLES
 * backend uses: slCreateEngine, an engine object, an output mix and a PCM
 * buffer-queue audio player. PCM the game enqueues is converted to the
 * device format (48 kHz / stereo / s16) and pushed out through libnx audout
 * on a per-player worker thread.
 *
 * The interface structs below MUST keep the exact method order of the OpenSL
 * ES headers the game was built against: the game calls methods by their slot
 * offset, not by name.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <switch.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>

#include "opensl.h"
#include "util.h"
#include "movie.h" // cutscene audio is mixed into this stream

#define SL_RESULT_SUCCESS            0
#define SL_RESULT_PARAMETER_INVALID  13
#define SL_RESULT_FEATURE_UNSUPPORTED 12
#define SL_RESULT_MEMORY_FAILURE     2

#define SL_BOOLEAN_FALSE 0
#define SL_BOOLEAN_TRUE  1

#define SL_PLAYSTATE_STOPPED 1
#define SL_PLAYSTATE_PAUSED  2
#define SL_PLAYSTATE_PLAYING 3

#define SL_DATAFORMAT_PCM 2

typedef uint32_t SLuint32;
typedef int32_t  SLint32;
typedef uint8_t  SLboolean;
typedef uint32_t SLresult;

// ---------------------------------------------------------------------------
// data structures passed to CreateAudioPlayer (subset)
// ---------------------------------------------------------------------------

typedef struct {
  SLuint32 formatType;
  SLuint32 numChannels;
  SLuint32 samplesPerSec; // millihertz!
  SLuint32 bitsPerSample;
  SLuint32 containerSize;
  SLuint32 channelMask;
  SLuint32 endianness;
} SLDataFormat_PCM;

typedef struct {
  void *pLocator;
  void *pFormat;
} SLDataSource, SLDataSink;

// ---------------------------------------------------------------------------
// interface ids: opaque, compared by pointer identity. the game holds these
// as exported pointer variables and hands them to GetInterface.
// ---------------------------------------------------------------------------

static const int iid_engine, iid_play, iid_bufferqueue, iid_volume, iid_androidcfg;
const void *SL_IID_ENGINE                  = &iid_engine;
const void *SL_IID_PLAY                    = &iid_play;
const void *SL_IID_BUFFERQUEUE             = &iid_bufferqueue;
const void *SL_IID_VOLUME                  = &iid_volume;
const void *SL_IID_ANDROIDCONFIGURATION    = &iid_androidcfg;
const void *SL_IID_ANDROIDSIMPLEBUFFERQUEUE = &iid_bufferqueue; // treated as the same queue

// ---------------------------------------------------------------------------
// audout: opened once, shared 48 kHz / stereo / s16 device
// ---------------------------------------------------------------------------

static int audout_ready = 0;
static int audout_rate = 48000;

int opensl_init(void) {
  if (audout_ready)
    return 0;
  Result rc = audoutInitialize();
  if (R_FAILED(rc)) {
    debugPrintf("opensl: audoutInitialize failed %08x\n", rc);
    return -1;
  }
  audoutStartAudioOut();
  audout_rate = audoutGetSampleRate();
  audout_ready = 1;
  return 0;
}

void opensl_exit(void) {
  if (!audout_ready)
    return;
  audoutStopAudioOut();
  audoutExit();
  audout_ready = 0;
}

// ---------------------------------------------------------------------------
// buffer-queue player
// ---------------------------------------------------------------------------

#define QUEUE_LEN 64
#define NUM_DEVBUFS 8
#define DEVBUF_SIZE (64 * 1024) // bytes, must be 0x1000-aligned

typedef void (*slBufferQueueCallback)(void *bq, void *context);
typedef void (*slPlayCallback)(void *play, void *context, SLuint32 event);

typedef struct {
  const void *data;
  SLuint32 size;
} QueuedBuf;

typedef struct Player {
  // interface vtable pointers handed to the game
  const void *objItf;
  const void *playItf;
  const void *bqItf;
  const void *volItf;
  const void *cfgItf;

  // source format
  int channels;
  int rate; // Hz

  // pending PCM the game enqueued (pointers it still owns until callback)
  QueuedBuf queue[QUEUE_LEN];
  volatile int q_head, q_tail;
  Mutex q_lock;
  CondVar q_cond;

  slBufferQueueCallback bq_cb;
  void *bq_ctx;

  volatile int state;   // SL_PLAYSTATE_*
  volatile int running; // worker alive

  Thread thread;

  // pool of device-format buffers fed to audout; submitted[i]=1 while buffer i
  // is queued in the driver and must not be reused
  AudioOutBuffer devbuf[NUM_DEVBUFS];
  int submitted[NUM_DEVBUFS];
} Player;

// each "object" the game sees is a pointer to one of these interface-pointer
// slots; GetInterface returns the address of the matching slot so that
// (*itf)->Method(itf, ...) dispatches through our vtables
typedef struct { const void *vtbl; Player *self; } Itf;

static Itf *itf_new(const void *vtbl, Player *p) {
  Itf *i = calloc(1, sizeof(*i));
  i->vtbl = vtbl;
  i->self = p;
  return i;
}

#define SELF(itf) (((Itf *)(itf))->self)

// --- audout feeding ---------------------------------------------------------

// convert one enqueued PCM16 buffer to 48 kHz stereo s16 into dst (device
// buffer), returning the number of bytes produced
static u64 convert_to_device(Player *p, const int16_t *src, SLuint32 src_bytes, int16_t *dst, u64 dst_cap_bytes) {
  const int in_ch = p->channels >= 2 ? 2 : 1;
  const int in_frames = (int)(src_bytes / (sizeof(int16_t) * in_ch));
  if (in_frames <= 0)
    return 0;

  const int out_rate = audout_rate;
  // nearest-neighbour resample is good enough for ADX2's already-mixed output
  int64_t out_frames = ((int64_t)in_frames * out_rate) / (p->rate > 0 ? p->rate : out_rate);
  const u64 max_out_frames = dst_cap_bytes / (sizeof(int16_t) * 2);
  if ((u64)out_frames > max_out_frames)
    out_frames = max_out_frames;

  for (int64_t i = 0; i < out_frames; i++) {
    const int64_t si = (i * in_frames) / out_frames;
    int16_t l, r;
    if (in_ch == 2) {
      l = src[si * 2 + 0];
      r = src[si * 2 + 1];
    } else {
      l = r = src[si];
    }
    dst[i * 2 + 0] = l;
    dst[i * 2 + 1] = r;
  }
  return (u64)out_frames * sizeof(int16_t) * 2;
}

static int devbuf_index(Player *p, AudioOutBuffer *b) {
  for (int i = 0; i < NUM_DEVBUFS; i++)
    if (&p->devbuf[i] == b)
      return i;
  return -1;
}

// reclaim any buffers the driver has finished with, back into the pool
static void reclaim_buffers(Player *p) {
  AudioOutBuffer *rel = NULL;
  u32 n = 0;
  while (R_SUCCEEDED(audoutGetReleasedAudioOutBuffer(&rel, &n)) && n > 0 && rel) {
    const int idx = devbuf_index(p, rel);
    if (idx >= 0)
      p->submitted[idx] = 0;
    rel = NULL;
    n = 0;
  }
}

static void player_thread(void *arg) {
  Player *p = arg;
  // this thread calls CRI's buffer-queue callback (game code with a stack
  // canary), so it needs a bionic TLS just like the engine threads
  uint8_t tls[BIONIC_TLS_SIZE];
  install_bionic_tls(tls);

  while (p->running) {
    reclaim_buffers(p);

    // wait for a CRI buffer or shutdown
    mutexLock(&p->q_lock);
    while (p->running && (p->state != SL_PLAYSTATE_PLAYING || p->q_head == p->q_tail))
      condvarWaitTimeout(&p->q_cond, &p->q_lock, 5000000ULL); // 5 ms
    if (!p->running) {
      mutexUnlock(&p->q_lock);
      break;
    }
    if (p->q_head == p->q_tail) {
      mutexUnlock(&p->q_lock);
      continue;
    }
    QueuedBuf qb = p->queue[p->q_head];
    p->q_head = (p->q_head + 1) % QUEUE_LEN;
    mutexUnlock(&p->q_lock);

    // grab a free device buffer; if the whole pool is queued in the driver,
    // block until one finishes playing -- this is what paces us to real time
    // (and keeps several buffers queued so playback is gapless)
    int idx = -1;
    while (p->running) {
      for (int i = 0; i < NUM_DEVBUFS; i++) {
        if (!p->submitted[i]) { idx = i; break; }
      }
      if (idx >= 0)
        break;
      AudioOutBuffer *rel = NULL;
      u32 n = 0;
      audoutWaitPlayFinish(&rel, &n, 100000000ull); // 100 ms
      if (n && rel) {
        const int ri = devbuf_index(p, rel);
        if (ri >= 0)
          p->submitted[ri] = 0;
      }
    }
    if (!p->running)
      break;

    AudioOutBuffer *db = &p->devbuf[idx];
    db->data_size = convert_to_device(p, qb.data, qb.size, db->buffer, db->buffer_size);
    db->data_offset = 0;
    if (db->data_size > 0) {
      // overlay cutscene audio onto CRI's stream (single audout device)
      movie_mix_audio((int16_t *)db->buffer, (int)(db->data_size / 4));
      audoutAppendAudioOutBuffer(db);
      p->submitted[idx] = 1;
    }

    // qb.data has been copied into the device buffer; tell the game it can refill
    if (p->bq_cb)
      p->bq_cb((void *)p->bqItf, p->bq_ctx);
  }
}

// --- SLBufferQueueItf --------------------------------------------------------

static SLresult bq_Enqueue(void *self, const void *pBuffer, SLuint32 size) {
  Player *p = SELF(self);
  mutexLock(&p->q_lock);
  const int next = (p->q_tail + 1) % QUEUE_LEN;
  if (next == p->q_head) {
    mutexUnlock(&p->q_lock);
    return SL_RESULT_MEMORY_FAILURE; // queue full
  }
  p->queue[p->q_tail].data = pBuffer;
  p->queue[p->q_tail].size = size;
  p->q_tail = next;
  condvarWakeOne(&p->q_cond);
  mutexUnlock(&p->q_lock);
  return SL_RESULT_SUCCESS;
}

static SLresult bq_Clear(void *self) {
  Player *p = SELF(self);
  mutexLock(&p->q_lock);
  p->q_head = p->q_tail = 0;
  mutexUnlock(&p->q_lock);
  return SL_RESULT_SUCCESS;
}

static SLresult bq_GetState(void *self, void *pState) {
  Player *p = SELF(self);
  if (pState) {
    SLuint32 *s = pState;
    mutexLock(&p->q_lock);
    s[0] = (p->q_tail - p->q_head + QUEUE_LEN) % QUEUE_LEN; // count
    s[1] = 0; // playIndex
    mutexUnlock(&p->q_lock);
  }
  return SL_RESULT_SUCCESS;
}

static SLresult bq_RegisterCallback(void *self, slBufferQueueCallback cb, void *ctx) {
  Player *p = SELF(self);
  p->bq_cb = cb;
  p->bq_ctx = ctx;
  return SL_RESULT_SUCCESS;
}

static const void *bq_vtbl[] = {
  (void *)bq_Enqueue,
  (void *)bq_Clear,
  (void *)bq_GetState,
  (void *)bq_RegisterCallback,
};

// --- SLPlayItf ---------------------------------------------------------------

static SLresult play_SetPlayState(void *self, SLuint32 state) {
  Player *p = SELF(self);
  p->state = state;
  if (state == SL_PLAYSTATE_PLAYING) {
    mutexLock(&p->q_lock);
    condvarWakeOne(&p->q_cond);
    mutexUnlock(&p->q_lock);
  }
  return SL_RESULT_SUCCESS;
}
static SLresult play_GetPlayState(void *self, SLuint32 *pState) {
  if (pState) *pState = SELF(self)->state;
  return SL_RESULT_SUCCESS;
}
static SLresult play_GetDuration(void *self, SLuint32 *pMsec) { (void)self; if (pMsec) *pMsec = 0; return SL_RESULT_SUCCESS; }
static SLresult play_GetPosition(void *self, SLuint32 *pMsec) { (void)self; if (pMsec) *pMsec = 0; return SL_RESULT_SUCCESS; }
static SLresult play_RegisterCallback(void *self, slPlayCallback cb, void *ctx) { (void)self; (void)cb; (void)ctx; return SL_RESULT_SUCCESS; }
// extra register args (if any) are simply ignored under AAPCS64
static SLresult play_ret(void *self) { (void)self; return SL_RESULT_SUCCESS; }

static const void *play_vtbl[] = {
  (void *)play_SetPlayState,
  (void *)play_GetPlayState,
  (void *)play_GetDuration,
  (void *)play_GetPosition,
  (void *)play_RegisterCallback,
  (void *)play_ret, // SetCallbackEventsMask
  (void *)play_ret, // GetCallbackEventsMask
  (void *)play_ret, // SetMarkerPosition
  (void *)play_ret, // ClearMarkerPosition
  (void *)play_ret, // GetMarkerPosition
  (void *)play_ret, // SetPositionUpdatePeriod
  (void *)play_ret, // GetPositionUpdatePeriod
};

// --- SLVolumeItf -------------------------------------------------------------

static SLresult vol_ret(void *self) { (void)self; return SL_RESULT_SUCCESS; }

static const void *vol_vtbl[] = {
  (void *)vol_ret, // SetVolumeLevel
  (void *)vol_ret, // GetVolumeLevel
  (void *)vol_ret, // GetMaxVolumeLevel
  (void *)vol_ret, // SetMute
  (void *)vol_ret, // GetMute
  (void *)vol_ret, // EnableStereoPosition
  (void *)vol_ret, // IsEnabledStereoPosition
  (void *)vol_ret, // SetStereoPosition
  (void *)vol_ret, // GetStereoPosition
};

// --- SLAndroidConfigurationItf ----------------------------------------------

static SLresult cfg_ret(void *self) { (void)self; return SL_RESULT_SUCCESS; }

static const void *cfg_vtbl[] = {
  (void *)cfg_ret, // SetConfiguration
  (void *)cfg_ret, // GetConfiguration
  (void *)cfg_ret, // AcquireJavaProxy
  (void *)cfg_ret, // ReleaseJavaProxy
};

// --- SLObjectItf (shared by engine, output mix, player) ----------------------

static SLresult obj_Realize(void *self, SLboolean async) { (void)self; (void)async; return SL_RESULT_SUCCESS; }
static SLresult obj_Resume(void *self, SLboolean async) { (void)self; (void)async; return SL_RESULT_SUCCESS; }
static SLresult obj_GetState(void *self, SLuint32 *pState) { (void)self; if (pState) *pState = 2 /*REALIZED*/; return SL_RESULT_SUCCESS; }
static SLresult obj_GetInterface(void *self, const void *iid, void *pInterface);
static SLresult obj_RegisterCallback(void *self, void *cb, void *ctx) { (void)self; (void)cb; (void)ctx; return SL_RESULT_SUCCESS; }
static void     obj_AbortAsyncOperation(void *self) { (void)self; }
static void     obj_Destroy(void *self);
static SLresult obj_ret(void *self) { (void)self; return SL_RESULT_SUCCESS; }

static const void *obj_vtbl[] = {
  (void *)obj_Realize,
  (void *)obj_Resume,
  (void *)obj_GetState,
  (void *)obj_GetInterface,
  (void *)obj_RegisterCallback,
  (void *)obj_AbortAsyncOperation,
  (void *)obj_Destroy,
  (void *)obj_ret, // SetPriority
  (void *)obj_ret, // GetPriority
  (void *)obj_ret, // SetLossOfControlInterfaces
};

// --- engine object -----------------------------------------------------------
// the engine has no per-instance state; one static Player-less Itf is enough,
// but GetInterface on it must return the engine itf

static SLresult eng_CreateAudioPlayer(void *self, void **pPlayer, SLDataSource *pSrc, SLDataSink *pSnk,
                                      SLuint32 numIfaces, const void *iids, const SLboolean *req);
static SLresult eng_CreateOutputMix(void *self, void **pMix, SLuint32 numIfaces, const void *iids, const SLboolean *req);
static SLresult eng_ret(void *self) { (void)self; return SL_RESULT_FEATURE_UNSUPPORTED; }

static const void *eng_vtbl[] = {
  (void *)eng_ret, // CreateLEDDevice
  (void *)eng_ret, // CreateVibraDevice
  (void *)eng_CreateAudioPlayer,
  (void *)eng_ret, // CreateAudioRecorder
  (void *)eng_ret, // CreateMidiPlayer
  (void *)eng_ret, // CreateListener
  (void *)eng_ret, // Create3DGroup
  (void *)eng_CreateOutputMix,
  (void *)eng_ret, // CreateMetadataExtractor
  (void *)eng_ret, // CreateExtensionObject
  (void *)eng_ret, // QueryNumSupportedInterfaces
  (void *)eng_ret, // QuerySupportedInterfaces
  (void *)eng_ret, // QueryNumSupportedExtensions
  (void *)eng_ret, // QuerySupportedExtension
  (void *)eng_ret, // IsExtensionSupported
};

// an object instance: an SLObjectItf slot plus a kind tag and (for players)
// the engine interface slot / the contained Player
enum { OBJ_ENGINE, OBJ_MIX, OBJ_PLAYER };

typedef struct {
  const void *objVtbl; // must be first: this is the SLObjectItf the game holds
  int kind;
  const void *engVtbl; // for OBJ_ENGINE: the SLEngineItf slot
  Player *player;      // for OBJ_PLAYER
} Object;

static SLresult obj_GetInterface(void *self, const void *iid, void *pInterface) {
  Object *o = self; // self points at objVtbl which is the first member
  if (!pInterface)
    return SL_RESULT_PARAMETER_INVALID;
  void **out = pInterface;

  if (o->kind == OBJ_ENGINE && iid == SL_IID_ENGINE) {
    *out = &o->engVtbl;
    return SL_RESULT_SUCCESS;
  }
  if (o->kind == OBJ_PLAYER && o->player) {
    Player *p = o->player;
    // each *Itf already holds an Itf* whose first word is the vtable, so the
    // game's (*itf)->method(itf) dispatches correctly with itf as self
    if (iid == SL_IID_PLAY)        { *out = (void *)p->playItf; return SL_RESULT_SUCCESS; }
    if (iid == SL_IID_BUFFERQUEUE) { *out = (void *)p->bqItf;   return SL_RESULT_SUCCESS; }
    if (iid == SL_IID_VOLUME)      { *out = (void *)p->volItf;  return SL_RESULT_SUCCESS; }
    if (iid == SL_IID_ANDROIDCONFIGURATION) { *out = (void *)p->cfgItf; return SL_RESULT_SUCCESS; }
  }
  debugPrintf("opensl: GetInterface unsupported iid %p (kind %d)\n", iid, o ? o->kind : -1);
  return SL_RESULT_FEATURE_UNSUPPORTED;
}

static void obj_Destroy(void *self) {
  Object *o = self;
  if (o->kind == OBJ_PLAYER && o->player) {
    Player *p = o->player;
    p->running = 0;
    mutexLock(&p->q_lock);
    condvarWakeOne(&p->q_cond);
    mutexUnlock(&p->q_lock);
    threadWaitForExit(&p->thread);
    threadClose(&p->thread);
    for (int i = 0; i < NUM_DEVBUFS; i++)
      free(p->devbuf[i].buffer);
    free(p);
  }
  free(o);
}

// --- creation ----------------------------------------------------------------

static SLresult eng_CreateOutputMix(void *self, void **pMix, SLuint32 numIfaces, const void *iids, const SLboolean *req) {
  (void)self; (void)numIfaces; (void)iids; (void)req;
  Object *o = calloc(1, sizeof(*o));
  o->objVtbl = obj_vtbl;
  o->kind = OBJ_MIX;
  *pMix = o;
  return SL_RESULT_SUCCESS;
}

static SLresult eng_CreateAudioPlayer(void *self, void **pPlayer, SLDataSource *pSrc, SLDataSink *pSnk,
                                      SLuint32 numIfaces, const void *iids, const SLboolean *req) {
  (void)self; (void)pSnk; (void)numIfaces; (void)iids; (void)req;

  Player *p = calloc(1, sizeof(*p));
  if (!p)
    return SL_RESULT_MEMORY_FAILURE;

  p->channels = 2;
  p->rate = audout_rate;
  if (pSrc && pSrc->pFormat) {
    const SLDataFormat_PCM *fmt = pSrc->pFormat;
    if (fmt->formatType == SL_DATAFORMAT_PCM) {
      p->channels = (int)fmt->numChannels;
      p->rate = (int)(fmt->samplesPerSec / 1000); // millihertz -> hertz
      debugPrintf("opensl: CreateAudioPlayer %d ch, %d Hz, %d bps\n",
          p->channels, p->rate, fmt->bitsPerSample);
    }
  }

  mutexInit(&p->q_lock);
  condvarInit(&p->q_cond);
  p->state = SL_PLAYSTATE_STOPPED;

  // the interface handles the game holds are Itf* whose first word is the
  // vtable, so it can do (*itf)->method(itf) with itf as self
  p->playItf = itf_new(play_vtbl, p);
  p->bqItf   = itf_new(bq_vtbl, p);
  p->volItf  = itf_new(vol_vtbl, p);
  p->cfgItf  = itf_new(cfg_vtbl, p);

  for (int i = 0; i < NUM_DEVBUFS; i++) {
    p->devbuf[i].buffer = memalign(0x1000, DEVBUF_SIZE);
    p->devbuf[i].buffer_size = DEVBUF_SIZE;
    p->devbuf[i].data_size = 0;
    p->devbuf[i].data_offset = 0;
  }

  p->running = 1;
  if (R_FAILED(threadCreate(&p->thread, player_thread, p, NULL, 0x4000, 0x2C, -2))) {
    p->running = 0;
    free(p);
    return SL_RESULT_MEMORY_FAILURE;
  }
  threadStart(&p->thread);

  Object *o = calloc(1, sizeof(*o));
  o->objVtbl = obj_vtbl;
  o->kind = OBJ_PLAYER;
  o->player = p;
  *pPlayer = o;
  return SL_RESULT_SUCCESS;
}

uint32_t slCreateEngine(void **pEngine, uint32_t numOptions, const void *pEngineOptions,
                        uint32_t numInterfaces, const void *pInterfaceIds, const uint8_t *pInterfaceRequired) {
  (void)numOptions; (void)pEngineOptions; (void)numInterfaces; (void)pInterfaceIds; (void)pInterfaceRequired;
  if (!pEngine)
    return SL_RESULT_PARAMETER_INVALID;
  opensl_init();
  Object *o = calloc(1, sizeof(*o));
  o->objVtbl = obj_vtbl;
  o->kind = OBJ_ENGINE;
  o->engVtbl = eng_vtbl;
  *pEngine = o;
  return SL_RESULT_SUCCESS;
}
