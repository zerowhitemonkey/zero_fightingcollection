// SNKHD - backend grafico (OpenGL 3.3 per Linux/test, Direct3D 11 per Windows)
#ifndef SNKHD_GFXB_H
#define SNKHD_GFXB_H
#include <SDL.h>
enum { TEXF_RGBA8 = 0, TEXF_R8 = 1, TEXF_BGRA8_DYN = 2 };
enum { GDM_COLOR = 0, GDM_TEX = 1, GDM_ALPHA = 2, GDM_SHARP = 3 };
Uint32 gfxb_window_flags(void);
int  gfxb_init(SDL_Window *w, int vsync, char *info, int infolen);
void gfxb_quit(void);
void gfxb_set_vsync(int on);
void gfxb_drawable(int *w, int *h);
void gfxb_begin(int w, int h);              // destinazione = schermo, pulito a nero
unsigned gfxb_tex_create(int w, int h, int fmt, const void *data, int mips);
void gfxb_tex_update(unsigned t, const void *data, int w, int h, int pitch_px);
void gfxb_tex_free(unsigned t);
// v: nv vertici (x, y, u, v) in pixel della destinazione corrente; triangoli
// parametri di GDM_SHARP: dimensione della texture e fattore di scala texel -> schermo
void gfxb_set_sharp(float tw, float th, float sx, float sy);
void gfxb_draw(unsigned tex, int mode, const float color[4], const float *v, int nv);
void gfxb_rt_begin(void);                   // destinazione = buffer intermedio (dimensione schermo)
void gfxb_rt_crt(int scanlines, int blur);  // buffer intermedio -> schermo con filtro CRT
void gfxb_set_blur_scale(float s);          // fattore sul raggio del BLUR (1 = Neo Geo 320 px)
void gfxb_present(void);
#ifdef _WIN32
// cattura dello schermo (giochi esterni): device/contesto D3D11 e texture GPU copiabili
void *gfxb_d3d_device(void);
void *gfxb_d3d_context(void);
unsigned gfxb_tex_create_gpu(int w, int h);   // BGRA, destinazione di CopySubresourceRegion
void *gfxb_tex_resource(unsigned t);          // ID3D11Texture2D*
#endif
#endif
