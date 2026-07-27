/* main.c -- Professor Layton: Curious Village HD .so loader (Nintendo Switch)
 *
 * Loads the Android arm64 libll1.so, resolves its imports against native
 * implementations, patches its file I/O onto the SD card and drives it through
 * the same minimal entry-point sequence the Vita port used for this binary:
 * JNI_OnLoad -> setViewSize -> render() every frame. The game owns its GL
 * rendering; the loader owns the EGL context, input and the swap.
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <switch.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "config.h"
#include "util.h"
#include "error.h"
#include "so_util.h"
#include "imports.h"
#include "libc_shim.h"
#include "jni.h"
#include "opensl.h"
#include "movie.h"

static void *heap_so_base = NULL;
static size_t heap_so_limit = 0;

so_module game_mod; // libll1.so

// separate the newlib heap from the .so load arena
void __libnx_initheap(void) {
  void *addr;
  size_t size = 0, fake_heap_size = 0;
  size_t mem_available = 0, mem_used = 0;

  if (envHasHeapOverride()) {
    addr = envGetHeapOverrideAddr();
    size = envGetHeapOverrideSize();
  } else {
    svcGetInfo(&mem_available, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&mem_used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    if (mem_available > mem_used + 0x200000)
      size = (mem_available - mem_used - 0x200000) & ~0x1FFFFF;
    if (size == 0)
      size = 0x2000000 * 16;
    Result rc = svcSetHeapSize(&addr, size);
    if (R_FAILED(rc))
      diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed));
  }

  extern char *fake_heap_start;
  extern char *fake_heap_end;
  fake_heap_size  = umin(size, MEMORY_MB * 1024 * 1024);
  fake_heap_start = (char *)addr;
  fake_heap_end   = (char *)addr + fake_heap_size;

  heap_so_base = (char *)addr + fake_heap_size;
  heap_so_base = (void *)ALIGN_MEM((uintptr_t)heap_so_base, 0x1000);
  heap_so_limit = (char *)addr + size - (char *)heap_so_base;
}

static void check_syscalls(void) {
  if (!envIsSyscallHinted(0x77))
    fatal_error("svcMapProcessCodeMemory is unavailable.");
  if (!envIsSyscallHinted(0x78))
    fatal_error("svcUnmapProcessCodeMemory is unavailable.");
  if (!envIsSyscallHinted(0x73))
    fatal_error("svcSetProcessMemoryPermission is unavailable.");
  if (envGetOwnProcessHandle() == INVALID_HANDLE)
    fatal_error("Own process handle is unavailable.");
}

static void check_data(void) {
  struct stat st;
  if (stat(SO_NAME, &st) < 0)
    fatal_error("Could not find\n%s.\nExtract it from the APK's lib/arm64-v8a folder.", SO_NAME);
  if (stat(ASSETS_DIR, &st) < 0)
    fatal_error("Could not find the '%s' folder.\nExtract the APK's assets here.", ASSETS_DIR);
}

static void set_screen_size(int w, int h) {
  if (w <= 0 || h <= 0 || w > 1920 || h > 1080) {
    if (appletGetOperationMode() == AppletOperationMode_Console) {
      screen_width = 1920;
      screen_height = 1080;
    } else {
      screen_width = 1280;
      screen_height = 720;
    }
  } else {
    screen_width = w;
    screen_height = h;
  }
}

// ---------------------------------------------------------------------------
// EGL
// ---------------------------------------------------------------------------

static EGLDisplay s_display;
static EGLContext s_context;
static EGLSurface s_surface;

static int egl_init(void) {
  s_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!s_display)
    return -1;
  eglInitialize(s_display, NULL, NULL);
  if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
    return -2;

  EGLConfig config;
  EGLint num_configs = 0;
  const EGLint attribs[] = {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 24,
    EGL_STENCIL_SIZE, 8,
    EGL_NONE
  };
  eglChooseConfig(s_display, attribs, &config, 1, &num_configs);
  if (num_configs == 0)
    return -3;

  NWindow *win = nwindowGetDefault();
  nwindowSetDimensions(win, screen_width, screen_height);
  s_surface = eglCreateWindowSurface(s_display, config, (EGLNativeWindowType)win, NULL);
  if (!s_surface)
    return -4;

  const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
  s_context = eglCreateContext(s_display, config, EGL_NO_CONTEXT, ctx_attribs);
  if (!s_context)
    return -5;

  eglMakeCurrent(s_display, s_surface, s_surface, s_context);
  eglSwapInterval(s_display, 1);
  return 0;
}

// ---------------------------------------------------------------------------
// game patches
// ---------------------------------------------------------------------------

// the engine's file loaders, redirected onto the assets folder

static uint8_t FS_LoadFile(char *buf, const char *fname, int pos, int size) {
  FILE *f = fopen(asset_path(fname), "rb");
  if (!f)
    return 0;
  fseek(f, pos, SEEK_SET);
  fread(buf, 1, size, f);
  fclose(f);
  return 1;
}

static int FS_GetLength(const char *fname) {
  struct stat st;
  if (stat(asset_path(fname), &st) >= 0)
    return (int)st.st_size;
  return 0;
}

static void criErr_Notify(int unk, const char *err) {
  (void)unk;
  debugPrintf("criErr: %s\n", err ? err : "(null)");
}

// In landscape the engine letterboxes movies into the DS top-screen area of
// its layout, which is tiny on a 16:9 display. Replace GL_DrawMovie with a
// fullscreen aspect-fit draw. Portrait keeps the engine's own renderer (and
// its rotate button), so this hook is only installed in landscape.
static struct {
  GLuint prog;
  GLint loc_pos, loc_uv, loc_tex;
  int ready;
} movgl;

static GLuint link_program(const char *vs_src, const char *fs_src);

static void GL_DrawMovie_hook(unsigned int target, unsigned int tex,
                              float x, float y, float w, float h, float *mat) {
  (void)target; (void)x; (void)y; (void)w; (void)h; (void)mat;
  if (!movgl.ready) {
    movgl.ready = 1;
    movgl.prog = link_program(
        "attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV;"
        "void main() { vUV = aUV; gl_Position = vec4(aPos, 0.0, 1.0); }",
        "precision mediump float; uniform sampler2D tex; varying vec2 vUV;"
        "void main() { gl_FragColor = texture2D(tex, vUV); }");
    if (movgl.prog) {
      movgl.loc_pos = glGetAttribLocation(movgl.prog, "aPos");
      movgl.loc_uv = glGetAttribLocation(movgl.prog, "aUV");
      movgl.loc_tex = glGetUniformLocation(movgl.prog, "tex");
    }
  }
  int mw, mh;
  movie_dims(&mw, &mh);
  if (!movgl.prog || mw <= 0 || mh <= 0)
    return;

  // save the engine state this draw touches
  GLint prev_prog, prev_tex, prev_vp[4];
  GLint en_pos = 0, en_uv = 0;
  const GLboolean p_blend = glIsEnabled(GL_BLEND);
  const GLboolean p_depth = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean p_scissor = glIsEnabled(GL_SCISSOR_TEST);
  const GLboolean p_cull = glIsEnabled(GL_CULL_FACE);
  glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
  glGetIntegerv(GL_VIEWPORT, prev_vp);
  glGetVertexAttribiv(movgl.loc_pos, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en_pos);
  glGetVertexAttribiv(movgl.loc_uv, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en_uv);

  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glViewport(0, 0, screen_width, screen_height);

  float sx = 1.0f, sy = 1.0f;
  const float vid_aspect = (float)mw / (float)mh;
  const float scr_aspect = (float)screen_width / (float)screen_height;
  if (vid_aspect < scr_aspect)
    sx = vid_aspect / scr_aspect;
  else
    sy = scr_aspect / vid_aspect;

  const GLfloat pos[8] = { -sx,-sy,  sx,-sy,  -sx,sy,  sx,sy };
  const GLfloat uv[8]  = { 0,1,  1,1,  0,0,  1,0 };

  glUseProgram(movgl.prog);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(movgl.loc_tex, 0);
  glEnableVertexAttribArray(movgl.loc_pos);
  glEnableVertexAttribArray(movgl.loc_uv);
  glVertexAttribPointer(movgl.loc_pos, 2, GL_FLOAT, GL_FALSE, 0, pos);
  glVertexAttribPointer(movgl.loc_uv, 2, GL_FLOAT, GL_FALSE, 0, uv);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  if (!en_pos) glDisableVertexAttribArray(movgl.loc_pos);
  if (!en_uv) glDisableVertexAttribArray(movgl.loc_uv);
  glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
  glUseProgram(prev_prog);
  glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
  if (p_blend) glEnable(GL_BLEND);
  if (p_depth) glEnable(GL_DEPTH_TEST);
  if (p_scissor) glEnable(GL_SCISSOR_TEST);
  if (p_cull) glEnable(GL_CULL_FACE);
}

// The engine paces its render thread (OS_Run) against its NitroMain logic
// thread with a two-mutex turnstile: each side locks one mutex that the OTHER
// side unlocks. libnx mutexes refuse cross-thread unlocks, so these two
// addresses are routed through semaphores by the pthread shims (imports.c).
#define TURNSTILE_A_OFF 0x356b38
#define TURNSTILE_B_OFF 0x356b60

// patch an exported engine function; must run before so_finalize (the
// trampoline is written into the still-writable staging copy)
static void hook(const char *sym, void *fn) {
  uintptr_t addr = so_try_find_addr_rx(&game_mod, sym);
  if (!addr) {
    debugPrintf("patch: symbol %s not found\n", sym);
    return;
  }
  addr = addr - (uintptr_t)game_mod.load_virtbase + (uintptr_t)game_mod.load_base;
  hook_arm64(addr, (uintptr_t)fn);
}

static void patch_game(void) {
  hook("_Z11FS_LoadFilePcPKcii", FS_LoadFile);
  hook("_Z12FS_GetLengthPKc", FS_GetLength);
  hook("criErr_Notify", criErr_Notify);
  // landscape: draw movies fullscreen instead of in the layout's screen area;
  // portrait keeps the engine's movie renderer and its rotate button
  if (!config.portrait)
    hook("_Z12GL_DrawMoviejjffffPf", GL_DrawMovie_hook);
  // make the render<->logic vsync turnstile work across threads (see above)
  register_turnstile_mutexes(
      (void *)((uintptr_t)game_mod.load_virtbase + TURNSTILE_A_OFF),
      (void *)((uintptr_t)game_mod.load_virtbase + TURNSTILE_B_OFF));
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

static int  (* JNI_OnLoad)(void *vm);
static void (* setViewSize)(void *env, void *obj, int w, int h);
static void (* game_resume)(void *env, void *obj);
static void (* game_render)(void *env, void *obj, int frame, int button, int touch_num,
                            float x1, float y1, float x2, float y2);
static int  (* mo_create_texture)(void *env, void *obj, int w, int h);

// ---------------------------------------------------------------------------
// view: the game renders into a view_w x view_h viewport -- the framebuffer
// itself in landscape, or an offscreen FBO blitted rotated 90 degrees in
// portrait (hold the console sideways; the DS layout fills the screen)
// ---------------------------------------------------------------------------

static int view_w, view_h;

static struct {
  GLuint fbo, tex, depth;
  GLuint prog;
  GLint loc_pos, loc_uv, loc_tex;
} rot;

static GLuint compile_shader(GLenum type, const char *src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  return s;
}

static GLuint link_program(const char *vs_src, const char *fs_src) {
  const GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
  const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    glDeleteProgram(prog);
    return 0;
  }
  return prog;
}

static void rot_init(void) {
  glGenTextures(1, &rot.tex);
  glBindTexture(GL_TEXTURE_2D, rot.tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, view_w, view_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  glGenRenderbuffers(1, &rot.depth);
  glBindRenderbuffer(GL_RENDERBUFFER, rot.depth);
  glRenderbufferStorage(GL_RENDERBUFFER, 0x88F0 /* GL_DEPTH24_STENCIL8_OES */, view_w, view_h);

  glGenFramebuffers(1, &rot.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, rot.fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rot.tex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rot.depth);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rot.depth);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    // retry with a plain 16-bit depth buffer
    glBindRenderbuffer(GL_RENDERBUFFER, rot.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, view_w, view_h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      fatal_error("Could not create the portrait framebuffer.");
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  rot.prog = link_program(
      "attribute vec2 aPos; attribute vec2 aUV; varying vec2 vUV;"
      "void main() { vUV = aUV; gl_Position = vec4(aPos, 0.0, 1.0); }",
      "precision mediump float; uniform sampler2D tex; varying vec2 vUV;"
      "void main() { gl_FragColor = texture2D(tex, vUV); }");
  if (!rot.prog)
    fatal_error("Could not build the portrait blit shader.");
  rot.loc_pos = glGetAttribLocation(rot.prog, "aPos");
  rot.loc_uv = glGetAttribLocation(rot.prog, "aUV");
  rot.loc_tex = glGetUniformLocation(rot.prog, "tex");
}

// draw the portrait FBO rotated onto the screen (strip order: BL BR TL TR);
// portrait==1 matches the handedness the game's own movie-rotate button assumes
// (Android landscape); portrait==2 is the same view rotated 180 degrees, for
// holding the console the other way up
static void rot_blit(void) {
  static const GLfloat pos[8]  = { -1,-1,  1,-1,  -1,1,  1,1 };
  static const GLfloat uv_1[8] = { 0,1,  0,0,  1,1,  1,0 }; // portrait 1
  static const GLfloat uv_2[8] = { 1,0,  1,1,  0,0,  0,1 }; // portrait 2 (180)
  const GLfloat *uv = (config.portrait == 2) ? uv_2 : uv_1;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glViewport(0, 0, screen_width, screen_height);
  glUseProgram(rot.prog);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, rot.tex);
  glUniform1i(rot.loc_tex, 0);
  glEnableVertexAttribArray(rot.loc_pos);
  glEnableVertexAttribArray(rot.loc_uv);
  glVertexAttribPointer(rot.loc_pos, 2, GL_FLOAT, GL_FALSE, 0, pos);
  glVertexAttribPointer(rot.loc_uv, 2, GL_FLOAT, GL_FALSE, 0, uv);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDisableVertexAttribArray(rot.loc_pos);
  glDisableVertexAttribArray(rot.loc_uv);
}

static float cursor_x, cursor_y; // stick-driven touch point (view space)
static int   cursor_shown;       // draw the pointer this frame? (stick in use, not touching)

// ---------------------------------------------------------------------------
// input: touchscreen in handheld, a stick-driven cursor with a controller.
// everything is produced in game-view coordinates (portrait-aware).
// ---------------------------------------------------------------------------

static PadState pad;

// map a physical touch-panel point (1280x720) into game-view coordinates
// (the portrait mapping matches rot_blit's rotation)
static void panel_to_view(float px, float py, float *gx, float *gy) {
  if (config.portrait == 1) {
    *gx = (720.0f - py) * ((float)view_w / 720.0f);
    *gy = px * ((float)view_h / 1280.0f);
  } else if (config.portrait == 2) {
    // 180 degrees from portrait 1
    *gx = py * ((float)view_w / 720.0f);
    *gy = (1280.0f - px) * ((float)view_h / 1280.0f);
  } else {
    *gx = px * ((float)view_w / 1280.0f);
    *gy = py * ((float)view_h / 720.0f);
  }
}

static void collect_input(int *touch_num, float *x1, float *y1, float *x2, float *y2) {
  *touch_num = 0;
  *x1 = *y1 = *x2 = *y2 = 0.0f;

  // real touches (the panel is physically 1280x720, in landscape)
  HidTouchScreenState ts = { 0 };
  if (hidGetTouchScreenStates(&ts, 1) && ts.count > 0) {
    *touch_num = ts.count > 2 ? 2 : ts.count;
    panel_to_view(ts.touches[0].x, ts.touches[0].y, x1, y1);
    if (*touch_num > 1)
      panel_to_view(ts.touches[1].x, ts.touches[1].y, x2, y2);
    cursor_shown = 0; // direct touch: no on-screen pointer needed
    return;
  }

  // controller: left stick moves a hidden touch point, ZR / R / A press it
  padUpdate(&pad);
  const u64 held = padGetButtons(&pad);
  const HidAnalogStickState ls = padGetStickPos(&pad, 0);
  const float dead = 0.18f;
  const float speed = 14.0f * ((float)(view_w > view_h ? view_w : view_h) / 1280.0f);
  float nx = ls.x / 32767.0f, ny = ls.y / 32767.0f;
  if (nx > -dead && nx < dead) nx = 0.0f;
  if (ny > -dead && ny < dead) ny = 0.0f;

  // show the pointer once the controller is actually being used; a real touch
  // (handled above) hides it again
  if (nx != 0.0f || ny != 0.0f ||
      (held & (HidNpadButton_ZR | HidNpadButton_R | HidNpadButton_A)))
    cursor_shown = 1;

  // rotate the stick into view space so "up" follows the held console
  float dx, dy;
  if (config.portrait == 1) {
    dx = ny;
    dy = nx;
  } else {
    dx = nx;
    dy = -ny;
  }
  cursor_x += dx * speed;
  cursor_y += dy * speed;
  if (cursor_x < 0) cursor_x = 0;
  if (cursor_y < 0) cursor_y = 0;
  if (cursor_x > view_w)  cursor_x = view_w;
  if (cursor_y > view_h)  cursor_y = view_h;

  if (held & (HidNpadButton_ZR | HidNpadButton_R | HidNpadButton_A)) {
    *touch_num = 1;
    *x1 = cursor_x;
    *y1 = cursor_y;
  }
}

// On-screen pointer for the stick-driven cursor. Drawn in view space straight
// after the game's frame, into the same target the game used (the portrait FBO
// or the framebuffer), so it sits exactly on the touch point collect_input
// feeds the engine -- and is rotated together with the game in portrait. The
// shape is a radially symmetric dot, so it reads correctly at any rotation.
static struct {
  GLuint prog;
  GLint loc_pos, loc_local, loc_feather;
  int ready;
} cur;

static void cursor_draw(void) {
  if (!cur.ready) {
    cur.ready = 1;
    cur.prog = link_program(
        "attribute vec2 aPos; attribute vec2 aLocal; varying vec2 vLocal;"
        "void main() { vLocal = aLocal; gl_Position = vec4(aPos, 0.0, 1.0); }",
        "precision mediump float; varying vec2 vLocal; uniform float uFeather;"
        "void main() {"
        "  float d = length(vLocal);"
        "  float a = 1.0 - smoothstep(1.0 - uFeather, 1.0, d);"
        "  float core = 1.0 - smoothstep(0.74 - uFeather, 0.74 + uFeather, d);"
        "  vec3 col = mix(vec3(0.04), vec3(0.98), core);" // dark ring, white centre
        "  gl_FragColor = vec4(col, a * 0.85);"
        "}");
    if (cur.prog) {
      cur.loc_pos = glGetAttribLocation(cur.prog, "aPos");
      cur.loc_local = glGetAttribLocation(cur.prog, "aLocal");
      cur.loc_feather = glGetUniformLocation(cur.prog, "uFeather");
    }
  }
  if (!cur.prog)
    return;

  // a constant on-screen size regardless of render resolution
  const float r = 18.0f * ((float)(view_w > view_h ? view_w : view_h) / 1280.0f);
  const float cx = (cursor_x / (float)view_w) * 2.0f - 1.0f;
  const float cy = 1.0f - (cursor_y / (float)view_h) * 2.0f; // view y is top-down
  const float rx = r / (float)view_w * 2.0f;
  const float ry = r / (float)view_h * 2.0f;
  const GLfloat pos[8] = {
    cx - rx, cy - ry,  cx + rx, cy - ry,  cx - rx, cy + ry,  cx + rx, cy + ry,
  };
  static const GLfloat local[8] = { -1,-1,  1,-1,  -1,1,  1,1 };

  // save the engine state this draw touches
  GLint prev_prog, prev_buf, prev_vp[4];
  GLint en_pos = 0, en_local = 0;
  GLint bsrc_rgb, bdst_rgb, bsrc_a, bdst_a, beq_rgb, beq_a;
  const GLboolean p_blend = glIsEnabled(GL_BLEND);
  const GLboolean p_depth = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean p_scissor = glIsEnabled(GL_SCISSOR_TEST);
  const GLboolean p_cull = glIsEnabled(GL_CULL_FACE);
  glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_buf);
  glGetIntegerv(GL_VIEWPORT, prev_vp);
  glGetIntegerv(GL_BLEND_SRC_RGB, &bsrc_rgb);
  glGetIntegerv(GL_BLEND_DST_RGB, &bdst_rgb);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &bsrc_a);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &bdst_a);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &beq_rgb);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &beq_a);
  glGetVertexAttribiv(cur.loc_pos, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en_pos);
  glGetVertexAttribiv(cur.loc_local, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en_local);

  glViewport(0, 0, view_w, view_h);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(cur.prog);
  glUniform1f(cur.loc_feather, 2.5f / r);
  glEnableVertexAttribArray(cur.loc_pos);
  glEnableVertexAttribArray(cur.loc_local);
  glVertexAttribPointer(cur.loc_pos, 2, GL_FLOAT, GL_FALSE, 0, pos);
  glVertexAttribPointer(cur.loc_local, 2, GL_FLOAT, GL_FALSE, 0, local);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // restore exactly what we changed
  if (!en_pos) glDisableVertexAttribArray(cur.loc_pos);
  if (!en_local) glDisableVertexAttribArray(cur.loc_local);
  glBindBuffer(GL_ARRAY_BUFFER, (GLuint)prev_buf);
  glUseProgram(prev_prog);
  glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
  glBlendEquationSeparate(beq_rgb, beq_a);
  glBlendFuncSeparate(bsrc_rgb, bdst_rgb, bsrc_a, bdst_a);
  if (!p_blend) glDisable(GL_BLEND);
  if (p_depth) glEnable(GL_DEPTH_TEST);
  if (p_scissor) glEnable(GL_SCISSOR_TEST);
  if (p_cull) glEnable(GL_CULL_FACE);
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;

  cpu_boost(1);

  if (read_config(CONFIG_NAME) < 0)
    write_config(CONFIG_NAME);
  if (config.portrait < 0 || config.portrait > 2)
    config.portrait = 1; // 0 = landscape; 1 / 2 = portrait (two directions)

  check_syscalls();
  check_data();

  set_screen_size(config.screen_width, config.screen_height);

  // game view: the framebuffer in landscape, a rotated FBO in portrait
  if (config.portrait) {
    view_w = screen_height;
    view_h = screen_width;
  } else {
    view_w = screen_width;
    view_h = screen_height;
  }
  cursor_x = view_w / 2.0f;
  cursor_y = view_h / 2.0f;

  // mesa fast path: the engine never reads glGetError for correctness
  setenv("MESA_NO_ERROR", "1", 1);

  if (egl_init() != 0)
    fatal_error("Could not initialize EGL/GLES2.");
  if (config.portrait)
    rot_init();

  if (so_load(&game_mod, SO_NAME, heap_so_base, heap_so_limit) < 0)
    fatal_error("Could not load\n%s.", SO_NAME);
  so_relocate(&game_mod);
  so_resolve(&game_mod, dynlib_functions, dynlib_numfunctions, 1);
  patch_game();

  // entry points must be resolved BEFORE so_finalize: the lookup reads the
  // dynsym tables in load_base, which finalize remaps as no-access
  JNI_OnLoad   = (void *)so_find_addr_rx(&game_mod, "JNI_OnLoad");
  setViewSize  = (void *)so_find_addr_rx(&game_mod, "Java_com_Level5_LT1R_MainActivity_setViewSize");
  game_resume  = (void *)so_find_addr_rx(&game_mod, "Java_com_Level5_LT1R_MainActivity_resume");
  game_render  = (void *)so_find_addr_rx(&game_mod, "Java_com_Level5_LT1R_MainActivity_render");
  mo_create_texture = (void *)so_find_addr_rx(&game_mod, "Java_com_Level5_LT1R_MainActivity_MO_1CreateTexture");

  so_finalize(&game_mod);
  so_flush_caches(&game_mod);

  // the game's stack-protector code reads its canary from TPIDR_EL0+0x28;
  // every thread running game code needs a bionic TLS there (worker threads
  // get theirs via pthread_create_fake)
  static uint8_t main_tls[BIONIC_TLS_SIZE];
  install_bionic_tls(main_tls);

  so_execute_init_array(&game_mod);
  so_free_temp(&game_mod);

  jni_init();
  opensl_init();

  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  padInitializeDefault(&pad);
  hidInitializeTouchScreen();

  // same minimal lifecycle the Vita port drives for this binary; resume()
  // starts CRI audio
  JNI_OnLoad(fake_vm);
  setViewSize(fake_env, NULL, view_w, view_h);
  game_resume(fake_env, NULL);

  const u64 tick_freq = armGetSystemTickFreq();
  u64 last_tick = armGetSystemTick();
  u64 frame_no = 0;

  while (appletMainLoop()) {
    const u64 now = armGetSystemTick();
    const u64 delta_us = (now - last_tick) * 1000000ull / tick_freq;
    last_tick = now;
    frame_no++;

    int touch_num;
    float x1, y1, x2, y2;
    collect_input(&touch_num, &x1, &y1, &x2, &y2);

    // create the game-side movie texture on this (GL) thread when a movie
    // just started, then keep it fed with the frame that is due
    int mo_w, mo_h;
    if (movie_pending_texture(&mo_w, &mo_h))
      movie_set_texture((unsigned)mo_create_texture(fake_env, NULL, mo_w, mo_h));
    movie_gl_tick();

    if (config.portrait)
      glBindFramebuffer(GL_FRAMEBUFFER, rot.fbo);
    glViewport(0, 0, view_w, view_h);

    int frame_step = (int)((delta_us + 8333) / 16667);
    if (frame_step < 1) frame_step = 1;
    if (frame_step > 6) frame_step = 6;
    game_render(fake_env, NULL, frame_step, 0, touch_num, x1, y1, x2, y2);

    // overlay the stick pointer into the same view-space target the game drew
    // to, so it lands on the touch point (and rotates with it in portrait)
    if (cursor_shown) {
      glBindFramebuffer(GL_FRAMEBUFFER, config.portrait ? rot.fbo : 0);
      cursor_draw();
    }

    if (config.portrait)
      rot_blit();

    eglSwapBuffers(s_display, s_surface);

    if (frame_no == 30)
      cpu_boost(0); // boot loading is done; back to normal clocks
  }

  movie_close();
  opensl_exit();
  eglMakeCurrent(s_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(s_display, s_context);
  eglDestroySurface(s_display, s_surface);
  eglTerminate(s_display);

  return 0;
}
