// SNKHD - grafica (indipendente dal backend)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gfx.h"
#include "gfxb.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "../ext/stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../ext/stb_truetype.h"

static int win_w = 1, win_h = 1;
static float cv_scale = 0, cv_x = 0, cv_y = 0;
static float galpha = 1.0f;

#define NFONTCACHE 16
typedef struct { int font, px; unsigned tex; int aw, ah; stbtt_packedchar pc[96]; unsigned used; } fcache_t;
static fcache_t fc[NFONTCACHE];
static unsigned fc_tick;
static unsigned char *fontdata[2];

static fcache_t *font_get(int font, float canvas_size) {
    int px = (int)(canvas_size * cv_scale + 0.5f);
    if (px < 6) px = 6;
    fcache_t *lru = &fc[0];
    for (int i = 0; i < NFONTCACHE; ++i) {
        if (fc[i].tex && fc[i].font == font && fc[i].px == px) { fc[i].used = ++fc_tick; return &fc[i]; }
        if (fc[i].used < lru->used) lru = &fc[i];
    }
    if (lru->tex) gfxb_tex_free(lru->tex);
    int a = px > 90 ? 2048 : (px > 40 ? 1024 : 512);
    unsigned char *bmp = calloc(a, a);
    stbtt_pack_context pk;
    stbtt_PackBegin(&pk, bmp, a, a, 0, 1, NULL);
    stbtt_PackSetOversampling(&pk, px < 40 ? 2 : 1, 1);
    stbtt_PackFontRange(&pk, fontdata[font], 0, (float)px, 32, 96, lru->pc);
    stbtt_PackEnd(&pk);
    lru->tex = gfxb_tex_create(a, a, TEXF_R8, bmp, 0);
    free(bmp);
    lru->font = font; lru->px = px; lru->aw = a; lru->ah = a; lru->used = ++fc_tick;
    return lru;
}

static void color4(uint32_t rgba, float out[4], int apply_alpha) {
    out[0] = ((rgba >> 24) & 0xff) / 255.0f;
    out[1] = ((rgba >> 16) & 0xff) / 255.0f;
    out[2] = ((rgba >> 8) & 0xff) / 255.0f;
    out[3] = (rgba & 0xff) / 255.0f * (apply_alpha ? galpha : 1.0f);
}

static float vtx[6 * 4 * 512];
static int nvtx;
static void quad_px(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1) {
    float *v = vtx + nvtx * 4;
    float q[6][4] = { {x0,y0,u0,v0},{x1,y0,u1,v0},{x1,y1,u1,v1},{x0,y0,u0,v0},{x1,y1,u1,v1},{x0,y1,u0,v1} };
    memcpy(v, q, sizeof q);
    nvtx += 6;
}
static void flush(unsigned tex, int mode, const float col[4]) {
    if (!nvtx) return;
    gfxb_draw(tex, mode, col, vtx, nvtx);
    nvtx = 0;
}
#define PX(x) (cv_x + (x) * cv_scale)
#define PY(y) (cv_y + (y) * cv_scale)

int gfx_init(const char *assetdir) {
    char p[1024];
    const char *names[2] = { "fonts/Dosis-SemiBold.ttf", "fonts/Dosis-ExtraBold.ttf" };
    for (int i = 0; i < 2; ++i) {
        snprintf(p, sizeof p, "%s/%s", assetdir, names[i]);
        FILE *f = fopen(p, "rb");
        if (!f) { fprintf(stderr, "missing font: %s\n", p); return 0; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        fontdata[i] = malloc(n);
        if (fread(fontdata[i], 1, n, f) != (size_t)n) { fclose(f); return 0; }
        fclose(f);
    }
    return 1;
}

void gfx_frame_begin(int winw, int winh) {
    if (winw < 1) winw = 1;
    if (winh < 1) winh = 1;
    float s = fminf(winw / CANVAS_W, winh / CANVAS_H);
    if (fabsf(s - cv_scale) > 1e-4f) {
        for (int i = 0; i < NFONTCACHE; ++i) if (fc[i].tex) { gfxb_tex_free(fc[i].tex); fc[i].tex = 0; }
    }
    win_w = winw; win_h = winh;
    cv_scale = s;
    cv_x = (winw - CANVAS_W * s) * 0.5f;
    cv_y = (winh - CANVAS_H * s) * 0.5f;
    gfxb_begin(winw, winh);
    galpha = 1.0f;
}

void gfx_set_alpha(float a) { galpha = a; }

// Il canvas 1920x1080 e' centrato nella finestra (scala intera dell'asse piu' corto): quello che tocca un bordo del
// canvas (sfondi, veli, barre) viene esteso fino al bordo dello schermo, cosi' i menu riempiono sempre tutta la
// finestra qualunque sia la proporzione del desktop (le illustrazioni vengono ritagliate, mai deformate).
static void bleed(float *x, float *y, float *w, float *h) {
    if (cv_scale <= 0) return;
    float x0 = *x, y0 = *y, x1 = *x + *w, y1 = *y + *h;
    if (fabsf(x0) <= 0.5f) x0 = -cv_x / cv_scale;              // solo cio' che sta esattamente sul bordo del canvas
    if (fabsf(y0) <= 0.5f) y0 = -cv_y / cv_scale;
    if (fabsf(x1 - CANVAS_W) <= 0.5f) x1 = (win_w - cv_x) / cv_scale;
    if (fabsf(y1 - CANVAS_H) <= 0.5f) y1 = (win_h - cv_y) / cv_scale;
    *x = x0; *y = y0; *w = x1 - x0; *h = y1 - y0;
}

// area visibile della finestra in coordinate del canvas (oltre 0..1920 / 0..1080 se la proporzione e' diversa)
void gfx_view(float *x0, float *y0, float *x1, float *y1) {
    float s = cv_scale > 0 ? cv_scale : 1;
    *x0 = -cv_x / s; *y0 = -cv_y / s; *x1 = (win_w - cv_x) / s; *y1 = (win_h - cv_y) / s;
}

void gfx_rect(float x, float y, float w, float h, uint32_t rgba) {
    float c[4]; color4(rgba, c, 1);
    bleed(&x, &y, &w, &h);
    quad_px(PX(x), PY(y), PX(x + w), PY(y + h), 0, 0, 0, 0);
    flush(0, GDM_COLOR, c);
}
void gfx_rect_outline(float x, float y, float w, float h, float t, uint32_t rgba) {
    gfx_rect(x, y, w, t, rgba);
    gfx_rect(x, y + h - t, w, t, rgba);
    gfx_rect(x, y + t, t, h - 2 * t, rgba);
    gfx_rect(x + w - t, y + t, t, h - 2 * t, rgba);
}
static void image_uv(const tex_t *t, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t rgba) {
    if (!t || !t->id) return;
    float c[4]; color4(rgba, c, 1);
    quad_px(PX(x), PY(y), PX(x + w), PY(y + h), u0, v0, u1, v1);
    flush(t->id, GDM_TEX, c);
}
void gfx_image(const tex_t *t, float x, float y, float w, float h, uint32_t rgba) {
    image_uv(t, x, y, w, h, 0, 0, 1, 1, rgba);
}
void gfx_image_fit(const tex_t *t, float x, float y, float w, float h, uint32_t rgba) {
    if (!t || !t->id) return;
    float s = fminf(w / t->w, h / t->h);
    float iw = t->w * s, ih = t->h * s;
    image_uv(t, x + (w - iw) * 0.5f, y + (h - ih) * 0.5f, iw, ih, 0, 0, 1, 1, rgba);
}
void gfx_image_cover(const tex_t *t, float x, float y, float w, float h, uint32_t rgba) {
    if (!t || !t->id) return;
    bleed(&x, &y, &w, &h);
    float ta = (float)t->w / t->h, ra = w / h;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    if (ta > ra) { float f = ra / ta; u0 = 0.0f; u1 = f; }
    else { float f = ta / ra; v0 = (1 - f) * 0.5f; v1 = v0 + f; }
    image_uv(t, x, y, w, h, u0, v0, u1, v1, rgba);
}

float gfx_text_w(int font, float size, const char *s) {
    fcache_t *f = font_get(font, size);
    float w = 0;
    for (; *s; ++s) {
        int c = (unsigned char)*s;
        if (c < 32 || c > 127) c = '?';
        w += f->pc[c - 32].xadvance;
    }
    return w / cv_scale;
}

float gfx_text(int font, float size, float x, float y, uint32_t rgba, int align, const char *s) {
    fcache_t *f = font_get(font, size);
    float w = gfx_text_w(font, size, s);
    if (align == AL_CENTER) x -= w * 0.5f;
    else if (align == AL_RIGHT) x -= w;
    float c[4]; color4(rgba, c, 1);
    float fx = roundf(PX(x)), fy = roundf(PY(y));
    for (; *s; ++s) {
        int ch = (unsigned char)*s;
        if (ch < 32 || ch > 127) ch = '?';
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(f->pc, f->aw, f->ah, ch - 32, &fx, &fy, &q, 0);
        quad_px(q.x0, q.y0, q.x1, q.y1, q.s0, q.t0, q.s1, q.t1);
        if (nvtx >= 6 * 500) flush(f->tex, GDM_ALPHA, c);
    }
    flush(f->tex, GDM_ALPHA, c);
    return w;
}

int gfx_tex_load(tex_t *t, const char *path) {
    int w, h, n;
    unsigned char *d = stbi_load(path, &w, &h, &n, 4);
    if (!d) { memset(t, 0, sizeof *t); return 0; }
    t->id = gfxb_tex_create(w, h, TEXF_RGBA8, d, 1);
    stbi_image_free(d);
    t->w = w; t->h = h;
    return t->id != 0;
}
// background images whose shape is far from 16:9 (e.g. 3:1 SteamGridDB heroes): build a 1920x1080
// picture with the whole image centred and a blurred, darkened copy filling the empty space
static unsigned char *compose_bg(const unsigned char *s, int w, int h) {
    enum { W = 1920, H = 1080, SW = 48, SH = 27 };
    static unsigned char sm[SH * SW * 4], sm2[SH * SW * 4];
    // 1) small cover-cropped copy (box average) -> blur
    float ra = (float)W / H, cw = (float)w, ch = (float)h, cx = 0, cy = 0;
    if ((float)w / h > ra) { cw = h * ra; cx = (w - cw) * 0.5f; } else { ch = w / ra; cy = (h - ch) * 0.5f; }
    for (int j = 0; j < SH; ++j) for (int i = 0; i < SW; ++i) {
        int x0 = (int)(cx + cw * i / SW), x1 = (int)(cx + cw * (i + 1) / SW), y0 = (int)(cy + ch * j / SH), y1 = (int)(cy + ch * (j + 1) / SH);
        if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
        int sx = (x1 - x0) / 12 + 1, sy = (y1 - y0) / 12 + 1;
        unsigned acc[3] = {0}, n = 0;
        for (int y = y0; y < y1 && y < h; y += sy) for (int x = x0; x < x1 && x < w; x += sx) {
            const unsigned char *p = s + ((size_t)y * w + x) * 4;
            acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2]; ++n;
        }
        for (int c = 0; c < 3; ++c) sm[(j * SW + i) * 4 + c] = (unsigned char)(n ? acc[c] / n : 0);
    }
    for (int pass = 0; pass < 5; ++pass) {
        for (int j = 0; j < SH; ++j) for (int i = 0; i < SW; ++i) for (int c = 0; c < 3; ++c) {
            unsigned a = 0, n = 0;
            for (int dj = -2; dj <= 2; ++dj) for (int di = -2; di <= 2; ++di) {
                int jj = j + dj, ii = i + di;
                if (jj < 0 || jj >= SH || ii < 0 || ii >= SW) continue;
                a += sm[(jj * SW + ii) * 4 + c]; ++n;
            }
            sm2[(j * SW + i) * 4 + c] = (unsigned char)(a / n);
        }
        memcpy(sm, sm2, sizeof sm);
    }
    // 2) whole image fitted and centred
    float f = fminf((float)W / w, (float)H / h);
    float fw = w * f, fh = h * f, ox = (W - fw) * 0.5f, oy = (H - fh) * 0.5f;
    float edge = 60.0f;   // soft blend at the borders of the fitted image
    unsigned char *o = (unsigned char*)malloc((size_t)W * H * 4);
    if (!o) return NULL;
    // blurred background: horizontal pass into SH rows of W pixels, then vertical lerp per output row
    static float hrow[SH][W][3];
    for (int j = 0; j < SH; ++j) for (int x = 0; x < W; ++x) {
        float sx = (x + 0.5f) * SW / W - 0.5f; if (sx < 0) sx = 0; if (sx > SW - 1) sx = SW - 1;
        int i0 = (int)sx, i1 = i0 + 1 < SW ? i0 + 1 : i0; float fx = sx - i0;
        for (int c = 0; c < 3; ++c)
            hrow[j][x][c] = (sm[(j * SW + i0) * 4 + c] * (1 - fx) + sm[(j * SW + i1) * 4 + c] * fx) * 0.45f;
    }
    // fitted image: precomputed horizontal mapping
    int fx0 = (int)ceilf(ox), fx1 = (int)floorf(ox + fw); if (fx0 < 0) fx0 = 0; if (fx1 > W) fx1 = W;
    int fy0 = (int)ceilf(oy), fy1 = (int)floorf(oy + fh); if (fy0 < 0) fy0 = 0; if (fy1 > H) fy1 = H;
    static int mx0[W], mx1[W]; static float mfx[W], max_[W];
    for (int x = fx0; x < fx1; ++x) {
        float lx = x + 0.5f - ox, sx = lx / f - 0.5f;
        if (sx < 0) sx = 0; if (sx > w - 1) sx = (float)(w - 1);
        mx0[x] = (int)sx; mx1[x] = mx0[x] + 1 < w ? mx0[x] + 1 : mx0[x]; mfx[x] = sx - mx0[x];
        float d = fminf(lx, fw - lx); max_[x] = (fw >= W - 1) ? 1.0f : (d >= edge ? 1.0f : d / edge);
    }
    for (int y = 0; y < H; ++y) {
        float sy = (y + 0.5f) * SH / H - 0.5f; if (sy < 0) sy = 0; if (sy > SH - 1) sy = SH - 1;
        int j0 = (int)sy, j1 = j0 + 1 < SH ? j0 + 1 : j0; float fy = sy - j0;
        unsigned char *q = o + (size_t)y * W * 4;
        int inrow = y >= fy0 && y < fy1;
        float ay = 1, ty = 0; int r0 = 0, r1 = 0;
        if (inrow) {
            float ly = y + 0.5f - oy, d = fminf(ly, fh - ly);
            ay = (fh >= H - 1) ? 1.0f : (d >= edge ? 1.0f : d / edge);
            float yy = ly / f - 0.5f; if (yy < 0) yy = 0; if (yy > h - 1) yy = (float)(h - 1);
            r0 = (int)yy; r1 = r0 + 1 < h ? r0 + 1 : r0; ty = yy - r0;
        }
        const unsigned char *s0 = s + (size_t)r0 * w * 4, *s1 = s + (size_t)r1 * w * 4;
        for (int x = 0; x < W; ++x, q += 4) {
            float a = (inrow && x >= fx0 && x < fx1) ? fminf(ay, max_[x]) : 0.0f;
            float bg[3] = {0, 0, 0};
            if (a < 1.0f) for (int c = 0; c < 3; ++c) bg[c] = hrow[j0][x][c] * (1 - fy) + hrow[j1][x][c] * fy;
            if (a > 0.0f) {
                int x0 = mx0[x] * 4, x1 = mx1[x] * 4; float tx = mfx[x];
                for (int c = 0; c < 3; ++c) {
                    float t0 = s0[x0 + c] + (s0[x1 + c] - s0[x0 + c]) * tx, t1 = s1[x0 + c] + (s1[x1 + c] - s1[x0 + c]) * tx;
                    float fg = t0 + (t1 - t0) * ty;
                    q[c] = (unsigned char)(bg[c] + (fg - bg[c]) * a + 0.5f);
                }
            } else for (int c = 0; c < 3; ++c) q[c] = (unsigned char)(bg[c] + 0.5f);
            q[3] = 255;
        }
    }
    return o;
}
int gfx_tex_load_bg(tex_t *t, const char *path) {
    int w, h, n;
    unsigned char *d = stbi_load(path, &w, &h, &n, 4);
    if (!d) { memset(t, 0, sizeof *t); return 0; }
    float r = ((float)w / h) / (16.0f / 9.0f);
    if (r > 1.15f || r < 0.87f) {
        unsigned char *c = compose_bg(d, w, h);
        if (c) { stbi_image_free(d); d = c; w = 1920; h = 1080; }
    }
    t->id = gfxb_tex_create(w, h, TEXF_RGBA8, d, 1);
    free(d);   // stbi_image_free is free() with the default allocator
    t->w = w; t->h = h;
    return t->id != 0;
}
void gfx_tex_free(tex_t *t) {
    if (t->id) gfxb_tex_free(t->id);
    memset(t, 0, sizeof *t);
}

// ---------------------------------------------------------------- gioco
static unsigned game_tex; static int game_tw, game_th, game_w, game_h, game_ext;

// texture esterna (cattura di un gioco PC): non viene liberata da gfx
void gfx_game_set_tex(unsigned tex, int w, int h) {
    if (game_tex && !game_ext) gfxb_tex_free(game_tex);
    game_tex = tex; game_ext = tex != 0;
    game_tw = game_w = w; game_th = game_h = h;
}

void gfx_game_upload(const uint32_t *px, int w, int h, int pitch) {
    if (game_ext) { game_tex = 0; game_ext = 0; }
    if (!game_tex || w != game_tw || h != game_th) {
        if (game_tex) gfxb_tex_free(game_tex);
        game_tex = gfxb_tex_create(w, h, TEXF_BGRA8_DYN, NULL, 0);
        game_tw = w; game_th = h;
    }
    gfxb_tex_update(game_tex, px, w, h, pitch);
    game_w = w; game_h = h;
}

// riquadro del gioco in pixel interi: 4:3 alla massima altezza oppure tutto lo schermo
// SCR_SQUARE: pixel quadrati (320x224 -> 10:7, CPS3 384x224 -> 12:7); SCR_43: 4:3 come un CRT; SCR_169: tutto lo schermo;
// SCR_SQZOOM: pixel quadrati a schermo pieno con un po' di overscan
static int nat_w = 320, nat_h = 224;
void gfx_set_native(int w, int h) {
    nat_w = w > 0 ? w : 320; nat_h = h > 0 ? h : 224;
    gfxb_set_blur_scale(320.0f / (float)nat_w);   // BLUR: stesso raggio per pixel di gioco del Neo Geo
}
// Monitor: proporzione fisica dello schermo (0 = come i pixel, cioe' pixel quadrati). Su un CRT 4:3 a 1280x1024
// i pixel non sono quadrati: con MONITOR 4:3 il riquadro 4:3 occupa tutto lo schermo a qualsiasi risoluzione.
static float mon_aspect = 0;
void gfx_set_monitor(float aspect) { mon_aspect = aspect > 0 ? aspect : 0; }
// Bordi neri del gioco da togliere (pixel nativi): l'area ritagliata riempie il riquadro del gioco
static int crop_l, crop_t, crop_r, crop_b;
void gfx_set_crop(int l, int t, int r, int b) {
    crop_l = l < 0 ? 0 : l; crop_t = t < 0 ? 0 : t; crop_r = r < 0 ? 0 : r; crop_b = b < 0 ? 0 : b;
    if (crop_l + crop_r > nat_w / 4) crop_l = crop_r = 0;
    if (crop_t + crop_b > nat_h / 4) crop_t = crop_b = 0;
}
void gfx_get_crop(int *l, int *t, int *r, int *b) { *l = crop_l; *t = crop_t; *r = crop_r; *b = crop_b; }
// Riquadro del gioco e parte dell'immagine nativa mostrata (ex/ey = pixel nativi tolti in piu' per lato).
// SCREEN 3 (4:3 SQUARE ZOOM): pixel quadrati ingranditi fino a riempire lo schermo, l'eccedenza esce dai bordi
// (overscan); se andrebbero persi piu' di ZOOM_MAX_X / ZOOM_MAX_Y pixel per lato (CPS3, 384 pixel) resta come SQUARE.
#define ZOOM_MAX_X 16.0
#define ZOOM_MAX_Y 12.0
static void game_layout(int screen, int ww, int wh, int *x, int *y, int *w, int *h, double *ex, double *ey) {
    *ex = 0; *ey = 0;
    if (screen == 2) { *x = 0; *y = 0; *w = ww; *h = wh; return; }
    double vw = nat_w - crop_l - crop_r, vh = nat_h - crop_t - crop_b;
    // proporzione fisica voluta -> proporzione in pixel dello schermo
    double k = (mon_aspect > 0 && wh > 0) ? ((double)ww / wh) / mon_aspect : 1.0;
    double a = (screen == 1 ? 4.0 / 3.0 : vw / vh) * k;
    if (screen == 3 && ww > 0 && wh > 0) {
        double sa = (double)ww / wh, cx = 0, cy = 0;
        if (a > sa) cx = (vw - vw * sa / a) / 2;      // piu' larga dello schermo: lati fuori
        else cy = (vh - vh * a / sa) / 2;             // piu' alta: sopra e sotto fuori
        if (cx <= ZOOM_MAX_X && cy <= ZOOM_MAX_Y) {
            *ex = cx; *ey = cy;
            *x = 0; *y = 0; *w = ww; *h = wh;
            return;
        }
    }
    int gw = (int)(wh * a + 0.5), gh = wh;
    if (gw > ww) { gw = ww; gh = (int)(ww / a + 0.5); }
    if (gw >= ww - 2) gw = ww;          // arrotondamenti: niente bande di 1-2 pixel
    if (gh >= wh - 2) gh = wh;
    *w = gw; *h = gh;
    *x = (ww - gw) / 2; *y = (wh - gh) / 2;
}
void gfx_game_rect(int screen, int ww, int wh, int *x, int *y, int *w, int *h) {
    double ex, ey;
    game_layout(screen, ww, wh, x, y, w, h, &ex, &ey);
}
void gfx_game_visible(int screen, int ww, int wh, double *vw, double *vh) {
    int x, y, w, h; double ex, ey;
    game_layout(screen, ww, wh, &x, &y, &w, &h, &ex, &ey);
    *vw = nat_w - crop_l - crop_r - 2 * ex;
    *vh = nat_h - crop_t - crop_b - 2 * ey;
}

void gfx_game_begin(int screen) {
    gfxb_rt_begin();
    if (!game_tex) return;
    int x, y, w, h; double ex, ey;
    game_layout(screen, win_w, win_h, &x, &y, &w, &h, &ex, &ey);
    float c[4] = { 1, 1, 1, 1 };
    // immagine HD a scala intera -> rettangolo di gioco con "sharp bilinear":
    // blocchi regolari, solo il pixel di confine sfumato
    // area mostrata dell'immagine HD (bordi neri del gioco ed eventuale overscan esclusi)
    float kx = (float)game_w / nat_w, ky = (float)game_h / nat_h;
    float sx0 = (float)(crop_l + ex) * kx, sy0 = (float)(crop_t + ey) * ky;
    float sx1 = game_w - (float)(crop_r + ex) * kx, sy1 = game_h - (float)(crop_b + ey) * ky;
    gfxb_set_sharp((float)game_tw, (float)game_th, (float)w / (sx1 - sx0), (float)h / (sy1 - sy0));
    quad_px((float)x, (float)y, (float)(x + w), (float)(y + h), sx0 / game_tw, sy0 / game_th, sx1 / game_tw, sy1 / game_th);
    flush(game_tex, GDM_SHARP, c);
}

void gfx_game_end(int scanlines, int blur) {
    gfxb_rt_crt(scanlines, blur);
}

void gfx_fade(float a) {
    if (a <= 0.001f) return;
    if (a > 1) a = 1;
    float c[4] = { 0, 0, 0, a };
    quad_px(0, 0, (float)win_w, (float)win_h, 0, 0, 0, 0);
    flush(0, GDM_COLOR, c);
}

void gfx_present(void) { gfxb_present(); }
