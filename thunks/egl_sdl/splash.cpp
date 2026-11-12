// Static splash screen shown while the game loads (libraries + Unity startup can take several seconds).
// Enabled by setting  splash = "splash.bmp"  in the [device] section of the config (path relative to the
// folder the loader was started from). Off by default; if anything fails it silently falls back to black.
#include "egl_sdl.h"
#include "SDL2/SDL.h"
#include "glad.h"
#include "gles2.h"
#include "logging.h"

extern SDL_Window* sdl_win;
extern SDL_GLContext sdl_ctx;

static GLuint compile_shader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(s);
        return 0;
    }
    return s;
}

void splash_show(const char* bmpPath)
{
    SDL_Surface* bmp = SDL_LoadBMP(bmpPath);
    if (!bmp) {
        warning("Splash: could not load %s: %s\n", bmpPath, SDL_GetError());
        return;
    }
    // ABGR8888 == bytes R,G,B,A in memory on little-endian, which is what GL_RGBA/GL_UNSIGNED_BYTE expects.
    SDL_Surface* img = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(bmp);
    if (!img)
        return;

    if (!sdl_ensure_window()) {
        SDL_FreeSurface(img);
        return;
    }
    SDL_GL_MakeCurrent(sdl_win, sdl_ctx);
    load_gles2_funcs();

    const char* vs = "attribute vec2 pos; attribute vec2 uv; varying vec2 v_uv;"
                     "void main() { v_uv = uv; gl_Position = vec4(pos, 0.0, 1.0); }";
    const char* fs = "precision mediump float; varying vec2 v_uv; uniform sampler2D tex;"
                     "void main() { gl_FragColor = texture2D(tex, v_uv); }";
    GLuint v = compile_shader(GL_VERTEX_SHADER, vs);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs);
    GLuint prog = (v && f) ? glCreateProgram() : 0;
    if (prog) {
        glAttachShader(prog, v);
        glAttachShader(prog, f);
        glLinkProgram(prog);
        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            glDeleteProgram(prog);
            prog = 0;
        }
    }
    if (v) glDeleteShader(v);
    if (f) glDeleteShader(f);

    if (prog) {
        int dw = 0, dh = 0;
        SDL_GL_GetDrawableSize(sdl_win, &dw, &dh);
        if (dw <= 0 || dh <= 0)
            SDL_GetWindowSize(sdl_win, &dw, &dh);

        // Fit the image inside the window, keeping its aspect ratio (black bars if it doesn't match).
        float imgAspect = (float)img->w / (float)img->h;
        float winAspect = (float)dw / (float)dh;
        float sx = 1.0f, sy = 1.0f;
        if (imgAspect > winAspect)
            sy = winAspect / imgAspect;
        else
            sx = imgAspect / winAspect;
        const GLfloat verts[] = {
            -sx, -sy, 0.0f, 1.0f,
             sx, -sy, 1.0f, 1.0f,
            -sx,  sy, 0.0f, 0.0f,
             sx,  sy, 1.0f, 0.0f,
        };

        GLuint tex = 0, vbo = 0;
        glGenTextures(1, &tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->w, img->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

        glUseProgram(prog);
        GLint posLoc = glGetAttribLocation(prog, "pos");
        GLint uvLoc = glGetAttribLocation(prog, "uv");
        glUniform1i(glGetUniformLocation(prog, "tex"), 0);
        glEnableVertexAttribArray(posLoc);
        glEnableVertexAttribArray(uvLoc);
        glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
        glVertexAttribPointer(uvLoc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

        glViewport(0, 0, dw, dh);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        // Two swaps so both buffers of a double-buffered window contain the splash.
        for (int i = 0; i < 2; i++) {
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            SDL_GL_SwapWindow(sdl_win);
        }

        // Leave the context in a clean state for Unity.
        glDisableVertexAttribArray(posLoc);
        glDisableVertexAttribArray(uvLoc);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        glDeleteBuffers(1, &vbo);
        glDeleteTextures(1, &tex);
        glDeleteProgram(prog);
        g_splash_shown = true;
    }

    SDL_FreeSurface(img);
    // Release the context from this thread so Unity's render thread can make it current.
    SDL_GL_MakeCurrent(sdl_win, NULL);
}
