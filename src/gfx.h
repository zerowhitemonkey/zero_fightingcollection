// SNKHD - grafica (OpenGL 3.3): canvas virtuale 1920x1080, testo, immagini, gioco + filtro CRT
#ifndef SNKHD_GFX_H
#define SNKHD_GFX_H
#include <stdint.h>

#define CANVAS_W 1920.0f
#define CANVAS_H 1080.0f

typedef struct { unsigned id; int w, h; } tex_t;

enum { FONT_REG = 0, FONT_BOLD = 1 };
enum { AL_LEFT = 0, AL_CENTER = 1, AL_RIGHT = 2 };

int  gfx_init(const char *assetdir);
void gfx_frame_begin(int winw, int winh);   // viewport + clear
void gfx_set_alpha(float a);
void gfx_view(float *x0, float *y0, float *x1, float *y1); // area della finestra in coordinate del canvas                // moltiplicatore globale (dissolvenze)
void gfx_rect(float x, float y, float w, float h, uint32_t rgba);
void gfx_rect_outline(float x, float y, float w, float h, float t, uint32_t rgba);
void gfx_image(const tex_t *t, float x, float y, float w, float h, uint32_t rgba);
void gfx_image_fit(const tex_t *t, float x, float y, float w, float h, uint32_t rgba); // mantiene le proporzioni, centrata
void gfx_image_cover(const tex_t *t, float x, float y, float w, float h, uint32_t rgba); // riempie ritagliando
float gfx_text(int font, float size, float x, float y, uint32_t rgba, int align, const char *s);
float gfx_text_w(int font, float size, const char *s);
int  gfx_tex_load(tex_t *t, const char *path);
void gfx_tex_free(tex_t *t);

// Gioco: carica il fotogramma HD e lo disegna (4:3 o 16:9) con eventuale overlay e filtro CRT
void gfx_game_upload(const uint32_t *px, int w, int h, int pitch);
void gfx_game_set_tex(unsigned tex, int w, int h);   // texture esterna (cattura), non posseduta da gfx
void gfx_game_rect(int screen, int ww, int wh, int *x, int *y, int *w, int *h);   // screen = SCR_*
void gfx_game_visible(int screen, int ww, int wh, double *vw, double *vh);       // pixel nativi mostrati
void gfx_set_monitor(float aspect);        // proporzione fisica dello schermo (0 = pixel quadrati)
void gfx_set_crop(int l, int t, int r, int b);   // bordi neri da togliere (pixel nativi)
void gfx_get_crop(int *l, int *t, int *r, int *b);
void gfx_set_native(int w, int h);         // risoluzione nativa del sistema (320x224 Neo Geo, 384x224 CPS3)
void gfx_game_begin(int screen);           // disegna il gioco nel buffer intermedio
void gfx_game_end(int scanlines, int blur); // applica il filtro CRT e mostra
void gfx_fade(float a);                     // velo nero sull'intero schermo (0..1)
void gfx_present(void);
#endif
