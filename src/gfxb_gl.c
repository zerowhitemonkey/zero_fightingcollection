// SNKHD - backend OpenGL 3.3 (Linux / test)
#include <stdio.h>
#include <string.h>
#include "glfn.h"
#include "gfxb.h"

static SDL_Window *gwin;
static SDL_GLContext ctx;
static GLuint prog_basic, prog_crt, vao, vbo;
static GLint u_screen, u_mode, u_color, u_tex, u_tsz, u_scl;
static float sharp_p[4] = { 1, 1, 1, 1 };
void gfxb_set_sharp(float tw, float th, float sx, float sy) { sharp_p[0] = tw; sharp_p[1] = th; sharp_p[2] = sx; sharp_p[3] = sy; }
static GLint c_tex, c_screen, c_blur, c_scan;
static int bw, bh;                  // dimensione dello schermo
static GLuint fbo, fbo_tex; static int fbo_w, fbo_h;

static const char *vs_basic =
    "#version 330 core\n"
    "layout(location=0) in vec2 aPos; layout(location=1) in vec2 aUV;\n"
    "uniform vec2 uScreen; out vec2 vUV;\n"
    "void main(){ vUV=aUV; gl_Position=vec4(aPos.x/uScreen.x*2.0-1.0, 1.0-aPos.y/uScreen.y*2.0, 0.0, 1.0); }\n";
static const char *fs_basic =
    "#version 330 core\n"
    "in vec2 vUV; out vec4 o; uniform sampler2D uTex; uniform int uMode; uniform vec4 uColor; uniform vec2 uTsz, uScl;\n"
    "void main(){\n"
    " if(uMode==0) o=uColor;\n"
    " else if(uMode==3){ vec2 texel=vUV*uTsz; vec2 sc=max(uScl,vec2(1.0)); vec2 tf=floor(texel);\n"
    "  vec2 rr=0.5-0.5/sc; vec2 cd=texel-tf-0.5; vec2 f=(cd-clamp(cd,-rr,rr))*sc+0.5;\n"
    "  o=texture(uTex,(tf+f)/uTsz)*uColor; }\n"
    " else if(uMode==1) o=texture(uTex,vUV)*uColor;\n"
    " else o=vec4(uColor.rgb, uColor.a*texture(uTex,vUV).r);\n"
    "}\n";
// Port di CRT-ZERO.fx (CRT-Frutbunn semplificato, 320x240). vUV con v=0 in alto.
static const char *fs_crt =
    "#version 330 core\n"
    "in vec2 vUV; out vec4 o; uniform sampler2D uTex; uniform vec2 uScreen; uniform float uBlur; uniform int uScan;\n"
    "const vec2 tsize = vec2(320.0, 240.0);\n"
    "vec3 S(vec2 uv){ return texture(uTex, vec2(uv.x, 1.0-uv.y)).rgb; }\n"
    "vec3 gaussian(vec2 uv, vec2 r){\n"
    " float b = uBlur / (r.x / r.y); float dx=b/r.x, dy=b/r.y;\n"
    " vec3 c = S(uv+vec2(-dx,-dy))*0.077847 + S(uv+vec2(-dx,0.0))*0.123317 + S(uv+vec2(-dx,dy))*0.077847;\n"
    " c += S(uv+vec2(0.0,-dy))*0.123317 + S(uv)*0.195346 + S(uv+vec2(0.0,dy))*0.123317;\n"
    " c += S(uv+vec2(dx,-dy))*0.077847 + S(uv+vec2(dx,0.0))*0.123317 + S(uv+vec2(dx,dy))*0.077847;\n"
    " return c; }\n"
    "void main(){\n"
    " vec2 st = vUV - vec2(0.5);\n"
    " vec3 color = gaussian(vUV, tsize*2.0);\n"
    " if(uScan!=0){ float s = 2.5 + uScreen.y*(1.0/tsize.y); float j = cos(st.y*tsize.y*s)*0.25*(uScan==2 ? 0.5 : 1.0); color -= j*color; }\n"
    " o = vec4(color,1.0);\n"
    "}\n";

static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s, 1024, NULL, log); fprintf(stderr, "shader: %s\n", log); }
    return s;
}
static GLuint program(const char *vs, const char *fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
    glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
    glLinkProgram(p);
    return p;
}

Uint32 gfxb_window_flags(void) { return SDL_WINDOW_OPENGL; }

int gfxb_init(SDL_Window *w, int vsync, char *info, int infolen) {
    gwin = w;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    ctx = SDL_GL_CreateContext(w);
    if (!ctx || !glfn_load(SDL_GL_GetProcAddress)) return 0;
    snprintf(info, infolen, "OpenGL %s | %s", (const char*)glGetString(GL_VENDOR), (const char*)glGetString(GL_RENDERER));
    gfxb_set_vsync(vsync);
    prog_basic = program(vs_basic, fs_basic);
    u_screen = glGetUniformLocation(prog_basic, "uScreen");
    u_mode = glGetUniformLocation(prog_basic, "uMode");
    u_color = glGetUniformLocation(prog_basic, "uColor");
    u_tex = glGetUniformLocation(prog_basic, "uTex");
    u_tsz = glGetUniformLocation(prog_basic, "uTsz");
    u_scl = glGetUniformLocation(prog_basic, "uScl");
    prog_crt = program(vs_basic, fs_crt);
    c_tex = glGetUniformLocation(prog_crt, "uTex");
    c_screen = glGetUniformLocation(prog_crt, "uScreen");
    c_blur = glGetUniformLocation(prog_crt, "uBlur");
    c_scan = glGetUniformLocation(prog_crt, "uScan");
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 16, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, 0, 16, (void*)8);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return 1;
}
void gfxb_quit(void) { if (ctx) SDL_GL_DeleteContext(ctx); ctx = NULL; }
void gfxb_set_vsync(int on) { SDL_GL_SetSwapInterval(on ? 1 : 0); }
void gfxb_drawable(int *w, int *h) { SDL_GL_GetDrawableSize(gwin, w, h); }

void gfxb_begin(int w, int h) {
    bw = w; bh = h;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

unsigned gfxb_tex_create(int w, int h, int fmt, const void *data, int mips) {
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    if (fmt == TEXF_R8) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
    else if (fmt == TEXF_BGRA8_DYN)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    if (mips) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}
void gfxb_tex_update(unsigned t, const void *data, int w, int h, int pitch_px) {
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch_px);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}
void gfxb_tex_free(unsigned t) { GLuint g = t; glDeleteTextures(1, &g); }

void gfxb_draw(unsigned tex, int mode, const float c[4], const float *v, int nv) {
    glUseProgram(prog_basic);
    glUniform2f(u_screen, (float)bw, (float)bh);
    glUniform1i(u_tex, 0);
    glUniform1i(u_mode, mode);
    glUniform4f(u_color, c[0], c[1], c[2], c[3]);
    glUniform2f(u_tsz, sharp_p[0], sharp_p[1]);
    glUniform2f(u_scl, sharp_p[2], sharp_p[3]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, nv * 4 * sizeof(float), v, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, nv);
}

void gfxb_rt_begin(void) {
    if (!fbo || fbo_w != bw || fbo_h != bh) {
        if (fbo) { glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &fbo_tex); }
        fbo_tex = gfxb_tex_create(bw, bh, TEXF_RGBA8, NULL, 0);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_tex, 0);
        fbo_w = bw; fbo_h = bh;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, bw, bh);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

static float blur_scale = 1.0f;
void gfxb_set_blur_scale(float sc) { blur_scale = sc > 0 ? sc : 1.0f; }

void gfxb_rt_crt(int scan, int blur) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, bw, bh);
    glUseProgram(prog_crt);
    glUniform2f(glGetUniformLocation(prog_crt, "uScreen"), (float)bw, (float)bh);
    glUniform1i(c_tex, 0);
    glUniform2f(c_screen, (float)bw, (float)bh);
    glUniform1f(c_blur, (blur == 2 ? (320.0f / 480.0f) : blur ? 1.0f : 0.0f) * blur_scale);
    glUniform1i(c_scan, scan == 2 ? 2 : scan ? 1 : 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glDisable(GL_BLEND);
    float W = (float)bw, H = (float)bh;
    float q[6][4] = { {0,0,0,0},{W,0,1,0},{W,H,1,1},{0,0,0,0},{W,H,1,1},{0,H,0,1} };
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof q, q, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEnable(GL_BLEND);
}

void gfxb_present(void) { SDL_GL_SwapWindow(gwin); }
