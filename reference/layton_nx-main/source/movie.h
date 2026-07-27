/* movie.h -- cutscene video playback (MO_* native methods)
 *
 * The game asks the platform layer to play an .mp4 through the JNI MO_*
 * methods and then composites it itself on Android via a SurfaceTexture.
 * Here the loader decodes the file with FFmpeg on a worker thread and draws
 * the current frame over the game's output every loop iteration; the engine's
 * own MO_Render is neutered (see patch_game).
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#ifndef __MOVIE_H__
#define __MOVIE_H__

#include <stdint.h>

// path is relative to the assets dir; offs/size select a sub-region of the
// file (whole file when size <= 0). returns 1 on success.
int movie_open(const char *rel_path, int64_t offs, int64_t size);

// 1 while a movie is loaded (mirrors MO_GetState)
int movie_active(void);

// current playback position in milliseconds (mirrors MO_GetPosition)
int movie_position_ms(void);

void movie_pause(int paused);
void movie_set_volume(float vol);

// tear down the current movie; safe when nothing is playing
void movie_close(void);

// The game renders the movie itself (MO_Render -> GL_DrawMovie), which keeps
// its rotate button, aspect fitting and UI layering working: the loader only
// uploads decoded frames into the texture the game created.

// returns 1 (once per movie) when the game-side texture must be created; the
// main loop then calls the game's exported MO_CreateTexture(w, h) on the GL
// thread and hands the id back via movie_set_texture
int movie_pending_texture(int *w, int *h);
void movie_set_texture(unsigned int tex);

// upload the frame due by the playback clock into the game texture; called
// once per main-loop iteration on the GL thread. no-op when idle.
void movie_gl_tick(void);

// video dimensions of the active movie (0x0 when idle)
void movie_dims(int *w, int *h);

// mix pending movie audio (48 kHz stereo s16) into a device buffer about to be
// submitted to audout; called by the OpenSL output thread for every buffer.
// no-op while no movie is playing.
void movie_mix_audio(int16_t *dst, int frames);

#endif
