/* movie.c -- cutscene video playback (MO_* native methods)
 *
 * A worker thread demuxes the .mp4 with FFmpeg, decodes the video stream and
 * scales each frame to RGBA into a small ring; audio is decoded to 48 kHz
 * stereo and mixed into the CRI/audout stream (movie_mix_audio).
 *
 * Rendering is done BY THE GAME: on Android, MainActivity.MO_CreateTexture
 * makes a GL texture that MediaPlayer streams into via SurfaceTexture, and
 * the engine's MO_Render draws it (with the player's rotate button, aspect
 * fit and UI layering intact). Here the main loop calls that same
 * MO_CreateTexture export and movie_gl_tick uploads the decoded frame that
 * is due into it; MO_UpdateTexture (jni.c) hands the engine an identity
 * transform matrix.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <switch.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <threads.h>

#include <GLES2/gl2.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

#include "config.h"
#include "util.h"
#include "movie.h"
#include "libc_shim.h" // asset_path

#define NUM_VFRAMES 12

// the Switch audio-out device always runs at 48 kHz stereo; movie audio is
// resampled to that and mixed into CRI's stream by movie_mix_audio
#define MOVIE_MIX_RATE 48000

typedef struct {
  uint8_t *rgba;
  double pts; // seconds
} VideoFrame;

static struct {
  volatile int active;
  volatile int paused;
  volatile int stop_req;
  volatile int decode_eof;

  thrd_t thread;
  int thread_running;

  // in-memory source (the selected file region) + its AVIO
  uint8_t *src;
  int64_t src_size;
  int64_t src_pos;
  AVIOContext *avio;

  AVFormatContext *fmt;
  AVCodecContext *vctx;
  struct SwsContext *sws;
  int vstream;
  double video_tb;

  int width, height;

  VideoFrame frames[NUM_VFRAMES];
  int frame_read, frame_write, frame_count;
  mtx_t lock;
  cnd_t can_produce;

  // audio: decoded to 48 kHz stereo s16 into a ring, consumed by
  // movie_mix_audio on the OpenSL output thread
  AVCodecContext *actx;
  struct SwrContext *swr;
  int astream;
  int has_audio;
  float volume;
  int16_t *aring;       // interleaved stereo
  int a_cap, a_r, a_w;  // in sample frames

  // game-side texture (created by the MO_CreateTexture export, owned by the
  // engine); pending_create asks the main loop to create it on the GL thread
  int pending_create;
  unsigned int game_tex;
  int tex_alloc; // storage allocated

  // wall clock, anchored to the first presented frame
  u64 start_tick;
  u64 paused_ticks;
  u64 pause_tick;
  double pts_base;
  double cur_pts;

  int frame_shown;
} mp;

// guards the audio ring fields AND mp teardown vs. the OpenSL thread's mixing;
// lives outside mp so free_session's memset can't clobber it
static Mutex g_audio_lock;

// ---------------------------------------------------------------------------
// memory AVIO over the selected file region
// ---------------------------------------------------------------------------

static int mem_read(void *opaque, uint8_t *buf, int size) {
  (void)opaque;
  int64_t remain = mp.src_size - mp.src_pos;
  if (remain <= 0)
    return AVERROR_EOF;
  if (size > remain)
    size = (int)remain;
  memcpy(buf, mp.src + mp.src_pos, size);
  mp.src_pos += size;
  return size;
}

static int64_t mem_seek(void *opaque, int64_t off, int whence) {
  (void)opaque;
  if (whence == AVSEEK_SIZE)
    return mp.src_size;
  int64_t base = 0;
  if (whence == SEEK_CUR) base = mp.src_pos;
  else if (whence == SEEK_END) base = mp.src_size;
  mp.src_pos = base + off;
  if (mp.src_pos < 0) mp.src_pos = 0;
  if (mp.src_pos > mp.src_size) mp.src_pos = mp.src_size;
  return mp.src_pos;
}

// ---------------------------------------------------------------------------
// clock
// ---------------------------------------------------------------------------

static double movie_clock(void) {
  const u64 now = mp.paused ? mp.pause_tick : armGetSystemTick();
  return mp.pts_base + (double)(now - mp.start_tick - mp.paused_ticks) / (double)armGetSystemTickFreq();
}

// ---------------------------------------------------------------------------
// decoder thread
// ---------------------------------------------------------------------------

static void push_video_frame(AVFrame *frm) {
  mtx_lock(&mp.lock);
  while (mp.frame_count == NUM_VFRAMES && !mp.stop_req)
    cnd_wait(&mp.can_produce, &mp.lock);
  if (mp.stop_req) {
    mtx_unlock(&mp.lock);
    return;
  }
  VideoFrame *slot = &mp.frames[mp.frame_write];
  uint8_t *dst[1] = { slot->rgba };
  int dst_stride[1] = { mp.width * 4 };
  sws_scale(mp.sws, (const uint8_t *const *)frm->data, frm->linesize, 0, mp.height, dst, dst_stride);
  int64_t ts = frm->best_effort_timestamp;
  if (ts == AV_NOPTS_VALUE)
    ts = frm->pts;
  slot->pts = (ts == AV_NOPTS_VALUE) ? 0.0 : (double)ts * mp.video_tb;
  mp.frame_write = (mp.frame_write + 1) % NUM_VFRAMES;
  mp.frame_count++;
  mtx_unlock(&mp.lock);
}

// convert one decoded audio frame to 48 kHz stereo s16 and append to the ring.
// the video ring (~0.4 s deep) backpressures the demuxer, so the 4 s audio
// ring practically never fills; overflow drops samples rather than blocking.
static void push_audio_frame(const AVFrame *frm) {
  static int16_t tmp[8192 * 2]; // decoder thread only
  uint8_t *outp[1] = { (uint8_t *)tmp };
  const int out = swr_convert(mp.swr, outp, 8192, (const uint8_t **)frm->data, frm->nb_samples);
  if (out <= 0)
    return;
  mutexLock(&g_audio_lock);
  for (int i = 0; i < out; i++) {
    const int next = (mp.a_w + 1) % mp.a_cap;
    if (next == mp.a_r)
      break; // ring full
    mp.aring[mp.a_w * 2 + 0] = tmp[i * 2 + 0];
    mp.aring[mp.a_w * 2 + 1] = tmp[i * 2 + 1];
    mp.a_w = next;
  }
  mutexUnlock(&g_audio_lock);
}

static void drain_codec(AVCodecContext *ctx, AVFrame *frm, int is_video) {
  while (avcodec_receive_frame(ctx, frm) == 0) {
    if (mp.stop_req)
      return;
    if (is_video)
      push_video_frame(frm);
    else
      push_audio_frame(frm);
  }
}

static int decoder_main(void *arg) {
  (void)arg;
  // same priority as the render thread so CRI's workers can't starve the
  // decode mid-movie (all default-priority otherwise)
  svcSetThreadPriority(threadGetCurHandle(), 0x2C);
  AVPacket *pkt = av_packet_alloc();
  AVFrame *frm = av_frame_alloc();

  while (!mp.stop_req) {
    if (mp.paused) {
      thrd_sleep(&(struct timespec){ .tv_nsec = 10 * 1000 * 1000 }, NULL);
      continue;
    }
    if (av_read_frame(mp.fmt, pkt) < 0)
      break;
    if (pkt->stream_index == mp.vstream) {
      if (avcodec_send_packet(mp.vctx, pkt) == 0)
        drain_codec(mp.vctx, frm, 1);
    } else if (mp.has_audio && pkt->stream_index == mp.astream) {
      if (avcodec_send_packet(mp.actx, pkt) == 0)
        drain_codec(mp.actx, frm, 0);
    }
    av_packet_unref(pkt);
  }

  if (!mp.stop_req) {
    avcodec_send_packet(mp.vctx, NULL);
    drain_codec(mp.vctx, frm, 1);
    if (mp.has_audio) {
      avcodec_send_packet(mp.actx, NULL);
      drain_codec(mp.actx, frm, 0);
    }
  }

  av_frame_free(&frm);
  av_packet_free(&pkt);
  mp.decode_eof = 1;
  return 0;
}

// ---------------------------------------------------------------------------
// GL: upload decoded frames into the texture the GAME created
// ---------------------------------------------------------------------------

int movie_pending_texture(int *w, int *h) {
  if (!mp.active || !mp.pending_create)
    return 0;
  mp.pending_create = 0;
  *w = mp.width;
  *h = mp.height;
  return 1;
}

void movie_set_texture(unsigned int tex) {
  mp.game_tex = tex;
  mp.tex_alloc = 0;
}

void movie_dims(int *w, int *h) {
  *w = mp.active ? mp.width : 0;
  *h = mp.active ? mp.height : 0;
}

void movie_gl_tick(void) {
  if (!mp.active || !mp.game_tex)
    return;

  // adopt newest frame due by the clock; always consume so the decoder, which
  // blocks on a full ring, can keep going
  int adopted = -1;
  mtx_lock(&mp.lock);
  if (!mp.frame_shown) {
    if (mp.frame_count > 0) {
      adopted = mp.frame_read;
      mp.frame_read = (mp.frame_read + 1) % NUM_VFRAMES;
      mp.frame_count--;
      // anchor the playback clock to the first presented frame so decoder
      // warm-up time doesn't register as the clock running ahead
      mp.pts_base = mp.frames[adopted].pts;
      mp.start_tick = armGetSystemTick();
      mp.paused_ticks = 0;
    }
  } else {
    double now = movie_clock();
    // after sleep mode or the HOME menu the wall clock has jumped far ahead;
    // re-anchor at the current frame instead of fast-forwarding the gap
    if (now > mp.cur_pts + 2.0) {
      mp.pts_base = mp.cur_pts;
      mp.start_tick = armGetSystemTick();
      mp.paused_ticks = 0;
      now = movie_clock();
    }
    while (mp.frame_count > 0 && mp.frames[mp.frame_read].pts <= now) {
      adopted = mp.frame_read;
      mp.frame_read = (mp.frame_read + 1) % NUM_VFRAMES;
      mp.frame_count--;
      if (mp.frame_count == 0 || mp.frames[mp.frame_read].pts > now)
        break;
    }
  }
  if (adopted >= 0) {
    mp.cur_pts = mp.frames[adopted].pts;

    GLint prev_tex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    glBindTexture(GL_TEXTURE_2D, mp.game_tex);
    if (!mp.tex_alloc) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mp.width, mp.height, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, mp.frames[adopted].rgba);
      mp.tex_alloc = 1;
    } else {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mp.width, mp.height,
                      GL_RGBA, GL_UNSIGNED_BYTE, mp.frames[adopted].rgba);
    }
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);

    cnd_signal(&mp.can_produce);
    mp.frame_shown = 1;
  }
  mtx_unlock(&mp.lock);
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

static void free_session(void) {
  // hold the mix lock for the whole teardown so the OpenSL thread can't read
  // the audio ring (or mp.active) mid-free
  mutexLock(&g_audio_lock);
  for (int i = 0; i < NUM_VFRAMES; i++)
    free(mp.frames[i].rgba);
  if (mp.sws)
    sws_freeContext(mp.sws);
  if (mp.vctx)
    avcodec_free_context(&mp.vctx);
  if (mp.swr)
    swr_free(&mp.swr);
  if (mp.actx)
    avcodec_free_context(&mp.actx);
  free(mp.aring);
  if (mp.fmt)
    avformat_close_input(&mp.fmt);
  if (mp.avio) {
    av_freep(&mp.avio->buffer);
    avio_context_free(&mp.avio);
  }
  free(mp.src);
  mtx_destroy(&mp.lock);
  cnd_destroy(&mp.can_produce);
  memset(&mp, 0, sizeof(mp));
  mutexUnlock(&g_audio_lock);
}

void movie_close(void) {
  if (!mp.active)
    return;
  mp.stop_req = 1;
  mtx_lock(&mp.lock);
  cnd_broadcast(&mp.can_produce);
  mtx_unlock(&mp.lock);
  if (mp.thread_running) {
    int res;
    thrd_join(mp.thread, &res);
  }
  free_session();
  debugPrintf("movie: closed\n");
}

static int open_codec(void) {
  AVStream *stream = mp.fmt->streams[mp.vstream];
  const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!codec)
    return -1;
  mp.vctx = avcodec_alloc_context3(codec);
  if (!mp.vctx)
    return -1;
  if (avcodec_parameters_to_context(mp.vctx, stream->codecpar) < 0)
    return -1;
  mp.vctx->thread_count = 3;
  if (avcodec_open2(mp.vctx, codec, NULL) < 0)
    return -1;
  return 0;
}

// audio is optional; on any failure the movie just plays silent
static void open_audio(void) {
  mp.astream = av_find_best_stream(mp.fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  if (mp.astream < 0)
    return;
  AVStream *as = mp.fmt->streams[mp.astream];
  const AVCodec *ac = avcodec_find_decoder(as->codecpar->codec_id);
  if (!ac)
    return;
  mp.actx = avcodec_alloc_context3(ac);
  if (!mp.actx)
    return;
  if (avcodec_parameters_to_context(mp.actx, as->codecpar) < 0 ||
      avcodec_open2(mp.actx, ac, NULL) < 0)
    return;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
  AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
  if (swr_alloc_set_opts2(&mp.swr, &out_layout, AV_SAMPLE_FMT_S16, MOVIE_MIX_RATE,
                          &mp.actx->ch_layout, mp.actx->sample_fmt, mp.actx->sample_rate, 0, NULL) < 0)
    mp.swr = NULL;
#else
  const uint64_t in_layout = mp.actx->channel_layout ?
      mp.actx->channel_layout : av_get_default_channel_layout(mp.actx->channels);
  mp.swr = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, MOVIE_MIX_RATE,
                              in_layout, mp.actx->sample_fmt, mp.actx->sample_rate, 0, NULL);
#endif
  if (!mp.swr || swr_init(mp.swr) != 0) {
    debugPrintf("movie: audio resampler setup failed, playing silent\n");
    return;
  }

  mp.a_cap = MOVIE_MIX_RATE * 4; // 4 s ring
  mp.aring = malloc((size_t)mp.a_cap * 2 * sizeof(int16_t));
  mp.has_audio = (mp.aring != NULL);
}

int movie_open(const char *rel_path, int64_t offs, int64_t size) {
  if (mp.active)
    movie_close();

  memset(&mp, 0, sizeof(mp));
  mtx_init(&mp.lock, mtx_plain);
  cnd_init(&mp.can_produce);
  mp.volume = 1.0f;

  // read the selected region into memory
  FILE *f = fopen(asset_path(rel_path), "rb");
  if (!f) {
    debugPrintf("movie: cannot open %s\n", rel_path);
    goto fail;
  }
  fseek(f, 0, SEEK_END);
  const long total = ftell(f);
  if (size <= 0)
    size = total - offs;
  if (offs < 0 || offs + size > total) {
    fclose(f);
    debugPrintf("movie: bad region offs=%lld size=%lld total=%ld\n",
                (long long)offs, (long long)size, total);
    goto fail;
  }
  mp.src = malloc(size);
  if (!mp.src) {
    fclose(f);
    goto fail;
  }
  fseek(f, offs, SEEK_SET);
  if (fread(mp.src, 1, size, f) != (size_t)size) {
    fclose(f);
    goto fail;
  }
  fclose(f);
  mp.src_size = size;
  mp.src_pos = 0;

  const int avio_bufsz = 64 * 1024;
  uint8_t *avio_buf = av_malloc(avio_bufsz);
  mp.avio = avio_alloc_context(avio_buf, avio_bufsz, 0, NULL, mem_read, NULL, mem_seek);
  if (!mp.avio)
    goto fail;

  mp.fmt = avformat_alloc_context();
  mp.fmt->pb = mp.avio;
  if (avformat_open_input(&mp.fmt, NULL, NULL, NULL) < 0) {
    debugPrintf("movie: avformat_open_input failed\n");
    mp.fmt = NULL;
    goto fail;
  }
  if (avformat_find_stream_info(mp.fmt, NULL) < 0)
    goto fail;

  mp.vstream = av_find_best_stream(mp.fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (mp.vstream < 0)
    goto fail;
  if (open_codec() < 0)
    goto fail;

  mp.width = mp.vctx->width;
  mp.height = mp.vctx->height;
  mp.video_tb = av_q2d(mp.fmt->streams[mp.vstream]->time_base);
  if (mp.width <= 0 || mp.height <= 0)
    goto fail;

  mp.sws = sws_getContext(mp.width, mp.height, mp.vctx->pix_fmt,
                          mp.width, mp.height, AV_PIX_FMT_RGBA,
                          SWS_BILINEAR, NULL, NULL, NULL);
  if (!mp.sws)
    goto fail;

  for (int i = 0; i < NUM_VFRAMES; i++) {
    mp.frames[i].rgba = malloc((size_t)mp.width * mp.height * 4);
    if (!mp.frames[i].rgba)
      goto fail;
  }

  open_audio();

  // ask the main loop to create the game-side texture on the GL thread
  mp.pending_create = 1;

  mp.start_tick = armGetSystemTick();

  if (thrd_create(&mp.thread, decoder_main, NULL) != thrd_success)
    goto fail;
  mp.thread_running = 1;
  mp.active = 1;
  debugPrintf("movie: playing %s (%dx%d, audio=%d)\n", rel_path, mp.width, mp.height, mp.has_audio);
  return 1;

fail:
  debugPrintf("movie: failed to start %s\n", rel_path ? rel_path : "(null)");
  mp.stop_req = 1;
  free_session();
  return 0;
}

int movie_active(void) {
  if (!mp.active)
    return 0;
  // finished when decode hit EOF and the ring drained
  if (mp.decode_eof && mp.frame_count == 0 && mp.frame_shown) {
    movie_close();
    return 0;
  }
  return 1;
}

int movie_position_ms(void) {
  if (!mp.active)
    return 0;
  return (int)(mp.cur_pts * 1000.0);
}

void movie_pause(int paused) {
  if (!mp.active || mp.paused == paused)
    return;
  if (paused) {
    mp.pause_tick = armGetSystemTick();
    mp.paused = 1;
  } else {
    mp.paused_ticks += armGetSystemTick() - mp.pause_tick;
    mp.paused = 0;
  }
}

void movie_set_volume(float vol) {
  if (vol < 0.0f) vol = 0.0f;
  if (vol > 1.0f) vol = 1.0f;
  mp.volume = vol;
}

void movie_mix_audio(int16_t *dst, int frames) {
  mutexLock(&g_audio_lock);
  // gate on frame_shown so the pre-rolled audio starts with the first frame
  if (!mp.active || !mp.has_audio || !mp.frame_shown || mp.paused) {
    mutexUnlock(&g_audio_lock);
    return;
  }
  const float vol = mp.volume;
  for (int i = 0; i < frames && mp.a_r != mp.a_w; i++) {
    int l = dst[i * 2 + 0] + (int)((float)mp.aring[mp.a_r * 2 + 0] * vol);
    int r = dst[i * 2 + 1] + (int)((float)mp.aring[mp.a_r * 2 + 1] * vol);
    if (l > 32767) l = 32767; else if (l < -32768) l = -32768;
    if (r > 32767) r = 32767; else if (r < -32768) r = -32768;
    dst[i * 2 + 0] = (int16_t)l;
    dst[i * 2 + 1] = (int16_t)r;
    mp.a_r = (mp.a_r + 1) % mp.a_cap;
  }
  mutexUnlock(&g_audio_lock);
}
