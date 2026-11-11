// SNKHD - video Psikyo PS3-V1 (PS6406B): renderer nativo e HD. Incluso da psikyo.cpp.
//
// Semantica di psikyosh_v.cpp di MAME (BSD-3): composizione per 8 livelli di priorita'; in ogni livello
// prima gli sprite di quel livello (lista catturata al vblank, buffer z = indice nella lista), poi i
// tilemap (registri per riga se il bit 0x80 del tipo e' attivo: scroll, zoom orizzontale, alpha,
// banco e priorita' per scanline), poi il post-lineblend se e' il suo livello. Prima di tutto il
// pre-lineblend riempie ogni riga. Fusioni: pieno, alpha costante, alpha per pen (pen 0xC0-0xFF).
//
// HD: si disegna direttamente a W = 320*kx, H = 224*ky (kx, ky interi). Uno sprite di dimensione
// nativa sw x sh (dopo lo zoom) occupa sw*kx x sh*ky pixel HD e ogni pixel HD campiona la bitmap
// dello sprite alla posizione esatta floor(r*zoom/k) (stessa formula del nativo con r nativo):
// negli zoom out tutti i pixel sorgente restano visibili invece di saltarne. Sfondi: riga nativa
// s = Y/ky (niente zoom verticale), colonne campionate con lo zoom di riga alla stessa maniera.
// A kx = ky = 1 il risultato coincide pixel per pixel con il renderer nativo (stesso codice).

double psk_prof[8]; int psk_prof_on;   // banco di prova (vedi compose)
static UINT8  alphatab[256];
static UINT16 bg_zoom[256];
static inline UINT32 pal6bit(UINT32 x) { x &= 0x3f; return (x << 2) | (x >> 4); }

static void video_init(void) {
    for (int i = 0; i < 0xc0; ++i) alphatab[i] = 0xff;
    for (int i = 0; i < 0x40; ++i) alphatab[i + 0xc0] = (UINT8)pal6bit(0x3f - i);
    for (int i = 0; i < 0x100; ++i) bg_zoom[i] = (UINT16)((64 * 0x400) / (i + 64));
}

// alpha_blend_r32 di MAME: s pesato 'a', d pesato 256-a, per canale ((s*a + d*(256-a)) >> 8).
// R e B nella stessa moltiplicazione: ogni somma sta in 16 bit (<= 255*256), nessun riporto tra i canali.
static inline UINT32 ablend(UINT32 d, UINT32 s, UINT32 a) {
    const UINT32 na = 256 - a;
    return ((((s & 0xff00ff) * a + (d & 0xff00ff) * na) >> 8) & 0xff00ff) |
           ((((s & 0x00ff00) * a + (d & 0x00ff00) * na) >> 8) & 0x00ff00) | 0xff000000u;
}

// SNKHD ombre (SHADOWS): 0 SOLID (come il gioco), 1 = 75%, 2 = 50%. Le ombre di Daraku sono sprite
// in palette 0xFE (nera): personaggi (ellissi a tile monocromatiche) e oggetti (sagome <= 2x2 tile:
// lattine "Kan_Shadow", schizzi "hane_kage", mosche). Escluso il lampo nero di 1 frame "pl_flash"
// (pezzi del personaggio, grandi e multicolore). Una sola miscela per pixel anche se due ombre si
// sovrappongono (buffer sh, azzerato a ogni frame).
static int  shadow_mode;
static UINT32 shadow_a;           // peso dell'ombra (0xC0 = 75%, 0x80 = 50%)
static UINT8 *mono4;              // bit per tile 4bpp: tutti i pixel non nulli dello stesso colore
extern "C" void psikyo_set_shadows(int mode) {
    shadow_mode = mode < 0 ? 0 : mode > 2 ? 2 : mode;
    shadow_a = shadow_mode == 1 ? 0xC0 : 0x80;
}
static void calc_mono(void) {
    free(mono4);
    UINT32 n = gfx_size >> 7;
    mono4 = (UINT8 *)calloc((n + 7) / 8, 1);
    if (!mono4) return;
    for (UINT32 t = 0; t < n; ++t) {
        const UINT8 *p = RomTiles + (size_t)t * 128; int c0 = 0, ok = 1;
        for (int i = 0; i < 128 && ok; ++i) {
            int c[2] = { p[i] >> 4, p[i] & 15 };
            for (int k = 0; k < 2; ++k) if (c[k]) { if (!c0) c0 = c[k]; else if (c[k] != c0) ok = 0; }
        }
        if (ok) mono4[t >> 3] |= 1 << (t & 7);
    }
}

struct vtarget {
    UINT32 *fb; UINT16 *zb;       // colore 0xFFRRGGBB e buffer z
    INT32 W, H, kx, ky;
    UINT8 *sh;                    // pixel gia' scuriti da un'ombra semitrasparente in questo frame
    INT32 s0, s1;                 // fascia di righe native [s0, s1) disegnata da questo thread
    UINT8 *rowbuf; UINT32 *lc; UINT8 *lw;   // buffer di riga del thread (W elementi)
};

// passo in virgola fissa 32 bit per floor(r*z/k): con passo arrotondato per eccesso e r < 2^20 il
// risultato e' esatto (la parte frazionaria di r*z/k e' un multiplo di 1/k <= 1 - 1/16)
static inline UINT64 fstep(UINT32 z, INT32 k) { return (((UINT64)z << 32) + (UINT64)k - 1) / (UINT64)k; }

// ------------------------------------------------------------------ sprite
// tnum: prima tile; le tile sono numerate per righe (wide per riga), la bitmap intera (wide*16 x high*16)
// si ribalta e si scala come un blocco (MAME drawgfxzoom; il caso non zoomato e' identico)
static void draw_sprite(const vtarget *t, int dpth, UINT32 tnum, UINT32 colr, int flipx, int flipy,
                        INT32 offsx, INT32 offsy, INT32 alpha, UINT32 zoomx, UINT32 zoomy, int wide, int high, UINT16 z, int shadow) {
    if (!zoomx || !zoomy) return;
    INT32 sw = (INT32)((((INT64)wide * 16 * 0x100000) / zoomx + 0x200) >> 10);
    INT32 sh = (INT32)((((INT64)high * 16 * 0x100000) / zoomy + 0x200) >> 10);
    if (sw <= 0 || sh <= 0) return;
    const INT32 kx = t->kx, ky = t->ky;
    INT32 X0 = offsx * kx, Y0 = offsy * ky, X1 = (offsx + sw) * kx, Y1 = (offsy + sh) * ky;
    INT32 xa = X0 < 0 ? 0 : X0, xb = X1 > t->W ? t->W : X1;
    const INT32 ylo = t->s0 * ky, yhi = t->s1 * ky;   // fascia del thread
    INT32 ya = Y0 < ylo ? ylo : Y0, yb = Y1 > yhi ? yhi : Y1;
    if (xa >= xb || ya >= yb) return;

    // tile della bitmap dello sprite (MAME: gfx->get_data(code % elements))
    const UINT32 elems = dpth ? (gfx_size >> 8) : (gfx_size >> 7);
    const int rowb = dpth ? 16 : 8;
    const UINT8 *tiles[256];
    for (int i = 0; i < wide * high; ++i) {
        UINT32 c = (tnum + i) % elems;
        int empty = dpth ? (TransTab[(c >> 3) + 0x10000] >> (c & 7)) & 1 : (TransTab[c >> 3] >> (c & 7)) & 1;
        tiles[i] = empty ? NULL : RomTiles + (size_t)c * (dpth ? 256 : 128);
    }
    const UINT32 *pal = Pal32 + colr * 16;
    const INT32 bw = wide * 16, bh = high * 16;
    const UINT64 sx = fstep(zoomx, kx), sy = fstep(zoomy, ky);
    const int at = alpha;   // 0xff pieno, >=0 costante, <0 per pen

    // la sequenza dei pixel sorgente di una riga HD dipende solo dalla riga della bitmap (py): si decodifica
    // una volta in rowbuf (indice di colore, 0 = trasparente) e si applica a tutte le righe HD con lo stesso py
    UINT8 *rowbuf = t->rowbuf;
    INT32 last_py = -1, any = 0, i0 = 0, i1 = 0;   // [i0, i1) = parte non trasparente della riga decodificata
    const INT64 c0 = xa - X0;
    const INT64 pos0 = flipx ? (INT64)((UINT64)((INT64)sw * kx - 1 - c0) * sx) : (INT64)((UINT64)c0 * sx);
    const INT64 step = flipx ? -(INT64)sx : (INT64)sx;
    const INT32 n = xb - xa;
    for (INT32 Y = ya; Y < yb; ++Y) {
        INT32 r = Y - Y0;
        INT64 rr = flipy ? (INT64)sh * ky - 1 - r : r;
        INT32 py = (INT32)(((UINT64)rr * sy) >> 32) >> 10;
        if (py >= bh) continue;
        if (py != last_py) {
            last_py = py; any = 0;
            const UINT8 *rowp[16];
            for (int tx = 0; tx < wide; ++tx) rowp[tx] = tiles[(py >> 4) * wide + tx] ? tiles[(py >> 4) * wide + tx] + (py & 15) * rowb : NULL;
            INT64 pos = pos0;
            i0 = n; i1 = 0;
            for (INT32 i = 0; i < n; ++i, pos += step) {
                INT32 px = (INT32)((UINT64)pos >> 32) >> 10;
                UINT32 c = 0;
                if (px < bw) {
                    const UINT8 *s = rowp[px >> 4];
                    if (s) c = dpth ? s[px & 15] : ((s[(px & 15) >> 1] >> ((px & 1) ? 0 : 4)) & 15);
                }
                rowbuf[i] = (UINT8)c;
                if (c) { if (i < i0) i0 = i; i1 = i + 1; }
            }
            any = i0 < i1;
        }
        if (!any) continue;
        if (psk_prof_on) psk_prof[5] += i1 - i0;
        UINT32 *d = t->fb + (size_t)Y * t->W + xa;
        UINT16 *zp = t->zb + (size_t)Y * t->W + xa;
        if (shadow) {   // SNKHD: ombra semitrasparente, una sola miscela per pixel
            UINT8 *shp = t->sh + (size_t)Y * t->W + xa;
            for (INT32 i = i0; i < i1; ++i) {
                UINT32 c = rowbuf[i];
                if (!c) continue;
                if (z > 0) { if (z < zp[i]) continue; zp[i] = z; }
                if (!shp[i]) { d[i] = ablend(d[i], pal[c], shadow_a); shp[i] = 1; }
            }
        } else if (at == 0xff) {
            if (z > 0) {
                for (INT32 i = i0; i < i1; ++i) { UINT32 c = rowbuf[i]; if (c && z >= zp[i]) { zp[i] = z; d[i] = pal[c]; } }
            } else {
                for (INT32 i = i0; i < i1; ++i) { UINT32 c = rowbuf[i]; if (c) d[i] = pal[c]; }
            }
        } else {
            for (INT32 i = i0; i < i1; ++i) {
                UINT32 c = rowbuf[i];
                if (!c) continue;
                if (z > 0) { if (z < zp[i]) continue; zp[i] = z; }
                UINT32 col = pal[c];
                if (at >= 0) d[i] = ablend(d[i], col, (UINT32)at);
                else { UINT32 a = alphatab[c]; d[i] = a == 0xff ? col : ablend(d[i], col, a); }
            }
        }
    }
}

static int sprite_is_shadow(UINT32 w1, UINT32 w2) {
    if ((w2 >> 24) != 0xFE || ((w2 >> 23) & 1)) return 0;
    int wide = ((w1 >> 8) & 0xf) + 1, high = ((w1 >> 24) & 0xf) + 1;
    if (wide * high <= 4) return 1;
    if (!mono4) return 0;
    UINT32 n = gfx_size >> 7;
    for (int i = 0; i < wide * high; ++i) {
        UINT32 c = ((w2 & 0x7ffff) + i) % n;
        if (!((TransTab[c >> 3] >> (c & 7)) & 1) && !((mono4[c >> 3] >> (c & 7)) & 1)) return 0;
    }
    return 1;
}

// lista degli sprite del frame decodificata una volta (sprites_prepare), poi disegnata da ogni fascia
struct sprent { UINT32 tnum, colr, zx, zy; INT32 x, y, alpha; UINT16 z; UINT8 dpth, flipx, flipy, wide, high, shadow, pri; };
static sprent spr_list[0x400];
static int spr_n;
static void sprites_prepare(void) {
    const UINT32 *spr = RamSprBuf;
    const UINT16 *list = (const UINT16 *)(spr + 0x3800 / 4);
    const UINT16 *zoom = (const UINT16 *)RamZoom;
    const UINT8 *atab = (const UINT8 *)VidRegs;
    spr_n = 0;
    for (int i = 0; i < 0x800 / 2; ++i) {
        UINT32 listdat = list[i ^ 1];
        UINT32 n = (listdat & 0x3ff) << 2;
        UINT32 w0 = spr[n], w1 = spr[n + 1], w2 = spr[n + 2];
        INT32 pri = (INT32)(((VidRegs[2] << (((w1 & 0x3000) >> 12) << 2)) >> 28) & 0xf);
        UINT32 zy = zoom[((w1 >> 16) & 0xff) ^ 1], zx = zoom[(w1 & 0xff) ^ 1];
        if (pri <= 7 && zy && zx) {
            sprent *e = &spr_list[spr_n++];
            INT32 ypos = (w0 >> 16) & 0x3ff, xpos = w0 & 0x3ff;
            if (ypos & 0x200) ypos -= 0x400;
            if (xpos & 0x200) xpos -= 0x400;
            UINT8 av = atab[((w2 >> 20) & 7) ^ 3];
            e->alpha = (av & 0x80) ? -1 : (INT32)pal6bit(0x3f - (av & 0x3f));
            e->shadow = shadow_mode && e->alpha == 0xff && sprite_is_shadow(w1, w2);
            e->tnum = w2 & 0x7ffff; e->colr = w2 >> 24; e->zx = zx; e->zy = zy; e->x = xpos; e->y = ypos;
            e->z = (UINT16)i; e->dpth = (w2 >> 23) & 1; e->flipx = (w1 >> 15) & 1; e->flipy = (w1 >> 31) & 1;
            e->wide = ((w1 >> 8) & 0xf) + 1; e->high = ((w1 >> 24) & 0xf) + 1; e->pri = (UINT8)pri;
        }
        if (listdat & 0x4000) break;
    }
}

static void draw_sprites(const vtarget *t, int req_pri) {
    for (int i = 0; i < spr_n; ++i) {
        const sprent *e = &spr_list[i];
        if (e->pri != req_pri) continue;
        draw_sprite(t, e->dpth, e->tnum, e->colr, e->flipx, e->flipy, e->x, e->y, e->alpha, e->zx, e->zy, e->wide, e->high, e->z, e->shadow);
    }
}

// ------------------------------------------------------------------ tilemap (draw_single_tilemap di MAME)
static void draw_tilemap(const vtarget *t, int layer, int req_pri) {
    UINT32 v6 = VidRegs[6] << (8 * layer), v7 = VidRegs[7] << (4 * layer);
    UINT32 linebank = (v6 & 0x7f000000) >> 24;
    if (linebank > 0x1f) return;
    int line_fx = (v6 & 0x80000000) != 0;
    int depth8 = (v7 & 0x4000) != 0;
    INT32 height = (v7 & 0x1000) ? 512 : 256;
    const UINT32 *scroll_reg = RamSpr + linebank * 0x200;
    const UINT32 *pzab_reg = scroll_reg + 0x100;
    const UINT32 elems = depth8 ? (gfx_size >> 8) : (gfx_size >> 7);
    const INT32 kx = t->kx, ky = t->ky;
    for (INT32 s = t->s0; s < t->s1; ++s) {
        UINT32 regaddr = (line_fx ? (UINT32)s : 0xfcu + layer) & 0xff;
        UINT32 pz = pzab_reg[regaddr];
        if ((INT32)(pz >> 24) != req_pri) continue;
        UINT32 tilebank = pz & 0xff;
        if (tilebank < 0x0a || tilebank > 0x1f) continue;
        UINT32 sc = scroll_reg[regaddr];
        INT32 scrollx = sc & 0x1ff, scrolly = (sc & 0x01ff0000) >> 16;
        UINT32 zm = bg_zoom[(pz >> 16) & 0xff];
        int alphamap = (pz >> 15) & 1;
        INT32 alpha = (pz & 0x3f00) >> 8;
        if (!alphamap) alpha = (INT32)pal6bit(0x3f - alpha);
        if (!alphamap && alpha == 0) continue;
        INT32 srcy = (s - scrolly) & (height - 1);
        const UINT32 *map = RamSpr + ((tilebank + (srcy >> 8)) & 0x1f) * 0x200 + ((srcy & 0xff) >> 4) * 32;
        INT32 ty = srcy & 15;
        const UINT64 st = fstep(zm, kx);
        // la riga di sorgente e' la stessa per le ky righe HD (nessuno zoom verticale): colore e peso per
        // colonna calcolati una volta (lw: 0 = trasparente, 0xff = pieno, altrimenti peso della miscela)
        UINT32 *lc = t->lc; UINT8 *lw = t->lw;
        INT32 x0 = t->W, x1 = 0;
        {
            UINT64 pos = 0;
            const UINT8 *lastp = NULL; INT32 lasttx = -1; UINT32 lastcol = 0;
            for (INT32 X = 0; X < t->W; ++X, pos += st) {
                lw[X] = 0;
                INT32 srcx = (INT32)(((pos >> 32) >> 10) - scrollx) & 0x1ff;
                INT32 tx = srcx >> 4;
                if (tx != lasttx) {
                    UINT32 data = map[tx];
                    UINT32 c = (data & 0x7ffff) % elems;
                    int empty = depth8 ? (TransTab[(c >> 3) + 0x10000] >> (c & 7)) & 1 : (TransTab[c >> 3] >> (c & 7)) & 1;
                    lastp = empty ? NULL : RomTiles + (size_t)c * (depth8 ? 256 : 128) + ty * (depth8 ? 16 : 8);
                    lastcol = (data >> 24) << 4;
                    lasttx = tx;
                }
                if (!lastp) continue;
                INT32 xx = srcx & 15;
                UINT32 pix = depth8 ? lastp[xx] : ((lastp[xx >> 1] >> ((xx & 1) ? 0 : 4)) & 15);
                if (!pix) continue;
                UINT32 a = alphamap ? alphatab[pix] : (UINT32)alpha;
                if (!a) continue;
                lc[X] = Pal32[(pix + lastcol) & 0xfff]; lw[X] = (UINT8)a;
                if (X < x0) x0 = X;
                x1 = X + 1;
            }
        }
        for (INT32 Y = s * ky; Y < (s + 1) * ky && x0 < x1; ++Y) {
            UINT32 *d = t->fb + (size_t)Y * t->W;
            for (INT32 X = x0; X < x1; ++X) {
                UINT32 a = lw[X];
                if (a == 0xff) d[X] = lc[X]; else if (a) d[X] = ablend(d[X], lc[X], a);
            }
        }
    }
}

static void draw_background(const vtarget *t, int req_pri) {
    for (int i = 0; i <= 3; ++i)
        if ((VidRegs[7] << (4 * i)) & 0x8000) draw_tilemap(t, i, req_pri);
}

// ------------------------------------------------------------------ pre/post lineblend (banco da vidregs[7] >> 24)
static void prelineblend(const vtarget *t) {
    UINT32 bank = (VidRegs[7] & 0xff000000) >> 24;
    for (INT32 s = t->s0; s < t->s1; ++s) {
        UINT32 c = 0xff000000u | (bank > 0x1f ? 0 : (RamSpr[bank * 0x200 + s] >> 8));
        for (INT32 Y = s * t->ky; Y < (s + 1) * t->ky; ++Y) {
            UINT32 *d = t->fb + (size_t)Y * t->W;
            for (INT32 X = 0; X < t->W; ++X) d[X] = c;
        }
    }
}
static void postlineblend(const vtarget *t, int req_pri) {
    if ((INT32)(VidRegs[2] & 0xf) != req_pri) return;
    UINT32 bank = (VidRegs[7] & 0xff000000) >> 24;
    if (bank > 0x1f) return;
    const UINT32 *lb = RamSpr + bank * 0x200 + 0x100;
    for (INT32 s = t->s0; s < t->s1; ++s) {
        UINT32 v = lb[s], c = 0xff000000u | (v >> 8);
        if (!(v & 0xff)) continue;
        for (INT32 Y = s * t->ky; Y < (s + 1) * t->ky; ++Y) {
            UINT32 *d = t->fb + (size_t)Y * t->W;
            if (v & 0x80) for (INT32 X = 0; X < t->W; ++X) d[X] = c;
            else { UINT32 a = (v & 0x7f) << 1; for (INT32 X = 0; X < t->W; ++X) d[X] = ablend(d[X], c, a); }
        }
    }
}

#include <time.h>
// banco di prova: tempi per fase (psk_prof_on = 1): 0 azzeramenti, 1 pre-lineblend, 2 sprite, 3 tilemap,
// 4 post-lineblend (ms accumulati), 5 pixel HD delle righe di sprite applicate
static inline double nowms(void) {
#ifdef _WIN32
    return 0;   // solo banco di prova (Linux)
#else
    if (!psk_prof_on) return 0;
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
#endif
}
// una fascia di righe (thread): azzeramenti e disegno completo delle righe [s0, s1)
static void compose_band(const vtarget *t) {
    double t0 = nowms();
    size_t o = (size_t)t->s0 * t->ky * t->W, n = (size_t)(t->s1 - t->s0) * t->ky * t->W;
    memset(t->zb + o, 0, n * sizeof(UINT16));
    if (shadow_mode) memset(t->sh + o, 0, n);
    double t1 = nowms(); psk_prof[0] += t1 - t0;
    prelineblend(t);
    double t2 = nowms(); psk_prof[1] += t2 - t1;
    for (int i = 0; i <= 7; ++i) {
        double a = nowms();
        draw_sprites(t, i);
        double b = nowms(); psk_prof[2] += b - a;
        draw_background(t, i);
        double c = nowms(); psk_prof[3] += c - b;
        postlineblend(t, i);
        psk_prof[4] += nowms() - c;
    }
}

// ------------------------------------------------------------------ thread di composizione
// L'immagine HD e' divisa in fasce orizzontali (righe native), una per thread: ogni pixel riceve le stesse
// operazioni nello stesso ordine, quindi il risultato e' identico a quello con un solo thread.
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif
#define PSK_MAXBANDS 8
struct band_scratch { UINT8 rowbuf[PSIKYO_W * 16]; UINT32 lc[PSIKYO_W * 16]; UINT8 lw[PSIKYO_W * 16]; };
static band_scratch bscr[PSK_MAXBANDS];
static vtarget band_t[PSK_MAXBANDS];
static int nbands = 0, nworkers = 0, want_bands = 0;
static volatile int job_gen = 0, job_left = 0;
#ifdef _WIN32
static CRITICAL_SECTION pk_mx; static CONDITION_VARIABLE pk_go, pk_done;
static DWORD WINAPI band_worker(LPVOID arg) {
    int id = (int)(intptr_t)arg, seen = 0;
    for (;;) {
        EnterCriticalSection(&pk_mx);
        while (job_gen == seen) SleepConditionVariableCS(&pk_go, &pk_mx, INFINITE);
        seen = job_gen;
        LeaveCriticalSection(&pk_mx);
        compose_band(&band_t[id]);
        EnterCriticalSection(&pk_mx);
        if (--job_left == 0) WakeConditionVariable(&pk_done);
        LeaveCriticalSection(&pk_mx);
    }
    return 0;
}
#else
static pthread_mutex_t pk_mx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pk_go = PTHREAD_COND_INITIALIZER, pk_done = PTHREAD_COND_INITIALIZER;
static void *band_worker(void *arg) {
    int id = (int)(intptr_t)arg, seen = 0;
    for (;;) {
        pthread_mutex_lock(&pk_mx);
        while (job_gen == seen) pthread_cond_wait(&pk_go, &pk_mx);
        seen = job_gen;
        pthread_mutex_unlock(&pk_mx);
        compose_band(&band_t[id]);
        pthread_mutex_lock(&pk_mx);
        if (--job_left == 0) pthread_cond_signal(&pk_done);
        pthread_mutex_unlock(&pk_mx);
    }
    return NULL;
}
#endif
static int cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si; GetSystemInfo(&si); return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN); return n > 0 ? (int)n : 1;
#endif
}
// numero di fasce: 0 = automatico (processori, al massimo 4)
extern "C" void psikyo_set_threads(int n) { want_bands = n < 0 ? 0 : n > PSK_MAXBANDS ? PSK_MAXBANDS : n; }
static int bands_init(void) {
    int want = want_bands ? want_bands : cpu_count();
    if (want > 4 && !want_bands) want = 4;
    if (want < 1) want = 1;
#ifdef _WIN32
    if (!nworkers && want > 1) { InitializeCriticalSection(&pk_mx); InitializeConditionVariable(&pk_go); InitializeConditionVariable(&pk_done); }
#endif
    while (nworkers < want - 1) {   // i thread restano in attesa per tutta la vita del processo
        int id = nworkers + 1;
#ifdef _WIN32
        HANDLE h = CreateThread(NULL, 0, band_worker, (LPVOID)(intptr_t)id, 0, NULL);
        if (!h) break;
        CloseHandle(h);
#else
        pthread_t th;
        if (pthread_create(&th, NULL, band_worker, (void *)(intptr_t)id)) break;
        pthread_detach(th);
#endif
        nworkers++;
    }
    nbands = want < nworkers + 1 ? want : nworkers + 1;
    return nbands;
}

static void compose(const vtarget *t0) {
    for (int i = 0; i < 0x1400; ++i) Pal32[i] = 0xff000000u | (RamPal[i] >> 8);
    sprites_prepare();
    int nb = bands_init();
    for (int b = 0; b < nb; ++b) {
        vtarget *t = &band_t[b];
        *t = *t0;
        t->s0 = PSIKYO_H * b / nb; t->s1 = PSIKYO_H * (b + 1) / nb;
        t->rowbuf = bscr[b].rowbuf; t->lc = bscr[b].lc; t->lw = bscr[b].lw;
    }
    if (nb > 1 && !psk_prof_on) {
#ifdef _WIN32
        EnterCriticalSection(&pk_mx); job_left = nb - 1; job_gen++; WakeAllConditionVariable(&pk_go); LeaveCriticalSection(&pk_mx);
        compose_band(&band_t[0]);
        EnterCriticalSection(&pk_mx); while (job_left) SleepConditionVariableCS(&pk_done, &pk_mx, INFINITE); LeaveCriticalSection(&pk_mx);
#else
        pthread_mutex_lock(&pk_mx); job_left = nb - 1; job_gen++; pthread_cond_broadcast(&pk_go); pthread_mutex_unlock(&pk_mx);
        compose_band(&band_t[0]);
        pthread_mutex_lock(&pk_mx); while (job_left) pthread_cond_wait(&pk_done, &pk_mx); pthread_mutex_unlock(&pk_mx);
#endif
    } else for (int b = 0; b < nb; ++b) compose_band(&band_t[b]);   // un thread (o banco di prova con i tempi per fase)
}

// ------------------------------------------------------------------ uscite
static UINT32 *hd_fb; static UINT16 *hd_zb; static UINT8 *hd_sh; static size_t hd_n;
static unsigned hd_w, hd_h, hd_kx = 1, hd_ky = 1;

static int ensure_buf(size_t n) {
    if (n <= hd_n) return 1;
    free(hd_fb); free(hd_zb); free(hd_sh);
    hd_fb = (UINT32 *)malloc(n * sizeof(UINT32)); hd_zb = (UINT16 *)malloc(n * sizeof(UINT16)); hd_sh = (UINT8 *)malloc(n);
    hd_n = (hd_fb && hd_zb && hd_sh) ? n : 0;
    return hd_n != 0;
}

extern "C" void psikyo_render_reference(uint32_t *out) {
    if (!loaded || !out || !ensure_buf(PSIKYO_W * PSIKYO_H)) return;
    vtarget t = { hd_fb, hd_zb, PSIKYO_W, PSIKYO_H, 1, 1, hd_sh, 0, PSIKYO_H, NULL, NULL, NULL };
    compose(&t);
    memcpy(out, hd_fb, PSIKYO_W * PSIKYO_H * sizeof(UINT32));
}

extern "C" void psikyo_set_hd(uint32_t *buf, unsigned w, unsigned h) {
    hd_out = buf; hd_w = w; hd_h = h;
    hd_kx = w / PSIKYO_W; hd_ky = h / PSIKYO_H;
    if (!hd_kx) hd_kx = 1;
    if (!hd_ky) hd_ky = 1;
}

static void hd_render(void) {
    INT32 W = (INT32)(PSIKYO_W * hd_kx), H = (INT32)(PSIKYO_H * hd_ky);
    if (!ensure_buf((size_t)W * H)) return;
    // buffer dell'app della stessa larghezza: si compone direttamente li' (colori gia' 0xFFRRGGBB)
    const int direct = hd_w == (unsigned)W && hd_h >= (unsigned)H;
    vtarget t = { direct ? hd_out : hd_fb, hd_zb, W, H, (INT32)hd_kx, (INT32)hd_ky, hd_sh, 0, PSIKYO_H, NULL, NULL, NULL };
    compose(&t);
    if (!direct)
        for (INT32 y = 0; y < H; ++y) memcpy(hd_out + (size_t)y * hd_w, hd_fb + (size_t)y * W, (size_t)W * sizeof(UINT32));
}

extern "C" void psikyo_debug_render_hd(void) { if (loaded && hd_out) hd_render(); }
