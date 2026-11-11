// SNKHD - video CPS3: renderer di riferimento (FBNeo) e renderer HD
// Incluso da cps3.cpp.
//
// L'hardware compone sprite e righe di tilemap in un canvas grande 384x224 * zoom/64
// (registro "fullscreen zoom", 0x40 = 1:1, 0x80 = 2x indietro) e poi lo riduce a 384x224
// (FBNeo: campionamento al pixel piu' vicino, si perdono righe e colonne).
// Il renderer HD disegna direttamente alla risoluzione d'uscita W = 384*kx, H = 224*ky:
// la colonna canvas c occupa le colonne HD [fx(c), fx(c+1)) con fx(c) = ceil(c*kx*64/zoom);
// ogni sprite (tile 16x16 con zoom proprio) e' ricampionato sull'intera area HD che copre.
// A kx = ky = 1 e zoom 0x40 l'uscita e' identica pixel per pixel al riferimento.

static inline INT32 ceil_div(INT64 a, INT64 b) { return (INT32)(a >= 0 ? (a + b - 1) / b : -((-a) / b)); }
static inline INT32 floor_div(INT64 a, INT64 b) { return (INT32)(a >= 0 ? a / b : -((-a + b - 1) / b)); }

// ------------------------------------------------------------------ strato testo (SS RAM, tile 8x8 4bpp)
// disegna il pixel (x, y) nativo con colore col: callback del bersaglio
typedef void (*ss_put_fn)(int x, int y, UINT32 col, UINT32 pal, UINT32 tile);
static void draw_text_layer(ss_put_fn put) {
    INT32 bank = (ss_bank_base & 0x01000000) ? 0x0000 : 0x0800;
    INT32 prev_rowscroll = 0;
    for (INT32 line = 0; line < 224; line++) {
        INT32 y = line / 8;
        INT32 count = (y * 64) + bank;
        INT32 rowscroll = RamSS[((line - 1) & 0x1ff) + 0x4000 / 4] >> 16;
        if (rowscroll == prev_rowscroll && (y * 8 != line)) continue;
        prev_rowscroll = rowscroll;
        for (INT32 x = 0; x < 64; x++, count++) {
            UINT32 data = RamSS[count];
            UINT32 tile = (data >> 16) & 0x1ff;
            INT32 pal = (data & 0x003f) >> 1;
            INT32 flipx = data & 0x0080, flipy = data & 0x0040;
            pal += ss_pal_base << 5;
            if (tile == 0) continue;
            tile += 0x200;
            INT32 effx = (x * 8) - rowscroll;
            if (effx < -7 || effx > CPS3_W) continue;
            INT32 ty = y * 8;
            if (effx > CPS3_W - 8 || ty > CPS3_H - 8) continue;   // come FBNeo (drawgfxzoom_0)
            const UINT8 *src = (const UINT8 *)RamSS + tile * 64;
            const UINT32 *color = Pal32 + ((pal << 4) & 0x1ffff);
            for (INT32 r = 0; r < 8; ++r) {
                const UINT8 *s = src + r * 8;
                INT32 dy = ty + (flipy ? 7 - r : r);
                // ordine dei pixel come FBNeo LSB_FIRST: byte 2,0,6,4, nibble basso poi alto
                UINT8 px[8] = { (UINT8)(s[2] & 0xf), (UINT8)(s[2] >> 4), (UINT8)(s[0] & 0xf), (UINT8)(s[0] >> 4),
                                (UINT8)(s[6] & 0xf), (UINT8)(s[6] >> 4), (UINT8)(s[4] & 0xf), (UINT8)(s[4] >> 4) };
                for (INT32 k = 0; k < 8; ++k) {
                    if (!px[k]) continue;
                    INT32 dx = effx + (flipx ? 7 - k : k);
                    if (dx < 0 || dx >= CPS3_W) continue;   // SNKHD: niente scrittura sulla riga precedente
                    put(dx, dy, color[px[k]], pal, tile - 0x200);
                }
            }
        }
    }
}

// ------------------------------------------------------------------ riga di tilemap (canvas)
// scrive i pixel della riga canvas "drawline" del tilemap in line[] (indice = x canvas + TL_OFF)
#define TL_OFF 32
#define TL_W (1024 + 2 * TL_OFF)
static INT32 tl_minx, tl_maxx;   // intervallo scritto
static int tilemap_line(UINT32 *line, INT32 drawline, const UINT32 *regs, INT32 max_x, INT32 max_y) {
    INT32 scrolly = ((regs[0] & 0x0000ffff) >> 0) + 4;
    INT32 l = (drawline + scrolly) & 0x3ff;
    if (!(regs[1] & 0x00008000)) return 0;
    UINT32 mapbase = ((regs[2] & 0x007f0000) >> 16) << 10;
    UINT32 linebase = ((regs[2] & 0x7f000000) >> 24) << 10;
    INT32 scrollx = (regs[0] & 0xffff0000) >> 16;
    if (regs[1] & 0x00004000) scrollx += (RamSpr[(linebase + ((l + 16) & 0x3ff)) & 0x1ffff] >> 16) & 0x3ff;
    if (drawline > max_y + 4) return 0;
    INT32 tileline = (l / 16) + 1, tilesubline = l % 16;
    tl_minx = 1 << 30; tl_maxx = -(1 << 30);
    const UINT8 *cram = (const UINT8 *)RamCRam;
    for (INT32 x = 0; x < (max_x / 16) + 2; x++) {
        UINT32 dat = RamSpr[(mapbase + ((tileline & 63) * 64) + ((x + scrollx / 16) & 63)) & 0x1ffff];
        UINT32 tileno = (dat & 0xfffe0000) >> 17;
        UINT32 colour = (dat & 0x000001ff);
        INT32 bpp = (dat & 0x00000200) >> 9;
        INT32 yflip = (dat & 0x00000800) >> 11, xflip = (dat & 0x00001000) >> 12;
        colour <<= bpp ? 6 : 8;
        INT32 sx = (x * 16) - scrollx % 16;
        INT32 sub = yflip ? 15 - tilesubline : tilesubline;
        const UINT8 *src = cram + ((tileno * 256 + 16 * sub) & 0x7fffff);
        UINT32 *dst = line + TL_OFF + sx;
        if (sx + TL_OFF < 0 || sx + 16 + TL_OFF > TL_W) continue;
        if (sx < tl_minx) tl_minx = sx;
        if (sx + 15 > tl_maxx) tl_maxx = sx + 15;
        // ordine dei byte LSB_FIRST: 3,2,1,0, 7,6,5,4, ...
        static const UINT8 ord[16] = { 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12 };
        for (INT32 k = 0; k < 16; ++k) {
            UINT8 c = src[ord[k]];
            if (c) dst[xflip ? 15 - k : k] = c | colour;
        }
    }
    return tl_maxx >= tl_minx;
}

// ------------------------------------------------------------------ bersaglio di disegno
struct vtarget {
    UINT32 *pen; INT32 W, H;      // buffer di pen (indice palette + bit di fusione)
    INT64 Ax, Ay, B;              // fx(c) = ceil(c*Ax/B)
    INT32 max_x, max_y;           // limiti del canvas come FBNeo
    const INT32 *gx;              // colonna canvas per colonna HD
    int ref;                      // 1 = canvas FBNeo (1:1, clip ai limiti del canvas)
    int dyn;                      // SNKHD HUD PRIORITY: marca i pixel degli sprite delle voci con gscroll 0
    UINT32 mark;                  // marca della voce corrente (PEN_MARK o 0)
};
#define PEN_MARK 0x80000000u      // pixel di sprite "del mondo" (personaggi/effetti) sopra l'HUD dinamico

static inline INT32 vt_fx(const vtarget *t, INT32 c) { return t->ref ? c : ceil_div((INT64)c * t->Ax, t->B); }
static inline INT32 vt_fy(const vtarget *t, INT32 c) { return t->ref ? c : ceil_div((INT64)c * t->Ay, t->B); }

enum { TR_PEN = 0, TR_BLEND = 1 };

static void vt_sprite(const vtarget *t, UINT32 code, UINT32 pal, INT32 flipx, INT32 flipy, INT32 sx, INT32 sy,
                      INT32 scalex, INT32 scaley, INT32 alpha, INT32 transparency) {
    if (!scalex || !scaley) return;
    INT32 w = (scalex * 16 + 0x8000) >> 16, h = (scaley * 16 + 0x8000) >> 16;
    if (!w || !h) return;
    INT32 X0 = vt_fx(t, sx), X1 = vt_fx(t, sx + w), Y0 = vt_fy(t, sy), Y1 = vt_fy(t, sy + h);
    INT32 Wh = X1 - X0, Hh = Y1 - Y0;
    if (Wh <= 0 || Hh <= 0) return;
    INT32 dx = (16 << 16) / Wh, dy = (16 << 16) / Hh;
    INT32 xb = 0, yi = 0;
    if (flipx) { xb = (Wh - 1) * dx; dx = -dx; }
    if (flipy) { yi = (Hh - 1) * dy; dy = -dy; }
    INT32 cx1 = t->ref ? t->max_x + 1 : t->W, cy1 = t->ref ? t->max_y + 1 : t->H;
    if (X0 < 0) { xb += (0 - X0) * dx; X0 = 0; }
    if (Y0 < 0) { yi += (0 - Y0) * dy; Y0 = 0; }
    if (X1 > cx1) X1 = cx1;
    if (Y1 > cy1) Y1 = cy1;
    if (X1 <= X0) return;
    const UINT8 *base = (const UINT8 *)RamCRam + ((code * 256) & 0x7fffff);
    INT32 pitch = t->ref ? 1024 : t->W;
    for (INT32 y = Y0; y < Y1; y++, yi += dy) {
        const UINT8 *source = base + (yi >> 16) * 16;
        UINT32 *dest = t->pen + (size_t)y * pitch;
        INT32 xi = xb;
        if (transparency == TR_PEN) {
            UINT32 pv = (pal << alpha) | t->mark;
            for (INT32 x = X0; x < X1; x++, xi += dx) {
                UINT8 c = source[(xi >> 16) ^ 3];
                if (c) dest[x] = pv | c;
            }
        } else {
            for (INT32 x = X0; x < X1; x++, xi += dx) {
                UINT8 c = source[(xi >> 16) ^ 3];
                if (c) {
                    if (alpha == 6) dest[x] |= (c & 0xf) << 13;
                    else dest[x] |= ((c & 1) << 15) | ((pal & 0x1) << 16);
                }
            }
        }
    }
}

static UINT32 tl_buf[TL_W];
static void vt_tileline(const vtarget *t, INT32 drawline, const UINT32 *regs) {
    if (t->ref) {
        if (!tilemap_line(tl_buf, drawline, regs, t->max_x, t->max_y)) return;
        // FBNeo scrive direttamente nella riga del canvas (larghezza 1024, margine 16 a sinistra)
        UINT32 *row = t->pen + (size_t)drawline * 1024;
        for (INT32 x = tl_minx; x <= tl_maxx; ++x) {
            UINT32 v = tl_buf[x + TL_OFF];
            if (v && x >= -16 && x < 1024 - 16) row[x] = v;
            tl_buf[x + TL_OFF] = 0;
        }
        return;
    }
    INT32 Y0 = vt_fy(t, drawline), Y1 = vt_fy(t, drawline + 1);
    if (Y0 < 0) Y0 = 0;
    if (Y1 > t->H) Y1 = t->H;
    if (Y1 <= Y0) return;
    if (!tilemap_line(tl_buf, drawline, regs, t->max_x, t->max_y)) return;
    INT32 xa = vt_fx(t, tl_minx), xb2 = vt_fx(t, tl_maxx + 1);
    if (xa < 0) xa = 0;
    if (xb2 > t->W) xb2 = t->W;
    for (INT32 y = Y0; y < Y1; ++y) {
        UINT32 *row = t->pen + (size_t)y * t->W;
        for (INT32 x = xa; x < xb2; ++x) {
            UINT32 v = tl_buf[t->gx[x] + TL_OFF];
            if (v) row[x] = v;
        }
    }
    for (INT32 x = tl_minx; x <= tl_maxx; ++x) tl_buf[x + TL_OFF] = 0;
}

// lista sprite (come DrvCps3Draw di FBNeo)
extern "C" { int cps3_dbg_only = -1; unsigned long long cps3_dbg_skip = 0; }   // debug: disegna solo / salta voci della lista principale
static void draw_sprites(const vtarget *t) {
    const UINT32 *SprSrc = SprList;
    for (INT32 i = 0x00000 / 4; i < 0x2000 / 4; i += 4) {
        if (SprSrc[i + 0] & 0x80000000) break;
        if (cps3_dbg_only >= 0 && i / 4 != cps3_dbg_only) continue;
        if (i / 4 < 64 && ((cps3_dbg_skip >> (i / 4)) & 1)) continue;
        INT32 gscroll = (SprSrc[i + 0] & 0x70000000) >> 28;
        INT32 length = (SprSrc[i + 0] & 0x01ff0000) >> 14;
        UINT32 start = (SprSrc[i + 0] & 0x00007ff0) >> 4;
        INT32 xpos = (SprSrc[i + 1] & 0x03ff0000) >> 16;
        INT32 ypos = (SprSrc[i + 1] & 0x000003ff) >> 0;
        INT32 whichbpp = (SprSrc[i + 2] & 0x40000000) >> 30;
        INT32 whichpal = (SprSrc[i + 2] & 0x20000000) >> 29;
        INT32 global_xflip = (SprSrc[i + 2] & 0x10000000) >> 28;
        INT32 global_yflip = (SprSrc[i + 2] & 0x08000000) >> 27;
        INT32 global_alpha = (SprSrc[i + 2] & 0x04000000) >> 26;
        INT32 global_bpp = (SprSrc[i + 2] & 0x02000000) >> 25;
        INT32 global_pal = (SprSrc[i + 2] & 0x01ff0000) >> 16;
        INT32 gscrollx = (RamVRegBuf[gscroll] & 0x03ff0000) >> 16;
        INT32 gscrolly = (RamVRegBuf[gscroll] & 0x000003ff) >> 0;
        // JoJo: le voci con gscroll 0 sono personaggi, Stand, ombre ed effetti (gli sfondi sono tilemap o voci
        // con gscroll 1/2/4: oggetti dello scenario, sfondi degli scontri, schermate fisse)
        int dyn_entry = t->dyn && gscroll == 0;
        start = (start * 0x100) >> 2;
        for (INT32 j = 0; j < length; j += 4) {
            if (start + j + 2 >= 0x20000) break;
            UINT32 value1 = SprSrc[start + j + 0], value2 = SprSrc[start + j + 1], value3 = SprSrc[start + j + 2];
            static const INT32 tilestable[4] = { 8, 1, 2, 4 };
            UINT32 tileno = (value1 & 0xfffe0000) >> 17;
            INT32 flipx = (value1 & 0x00001000) >> 12;
            INT32 flipy = (value1 & 0x00000800) >> 11;
            INT32 alpha = (value1 & 0x00000400) >> 10;
            INT32 bpp = (value1 & 0x00000200) >> 9;
            INT32 pal = (value1 & 0x000001ff);
            INT32 xpos2 = (value2 & 0x03ff0000) >> 16;
            INT32 ypos2 = (value2 & 0x000003ff) >> 0;
            INT32 ysizedraw2 = ((value3 & 0x7f000000) >> 24) + 1;
            INT32 xsizedraw2 = ((value3 & 0x007f0000) >> 16) + 1;
            INT32 ysize2 = ((value3 & 0x0000000c) >> 2);
            INT32 xsize2 = ((value3 & 0x00000003) >> 0);
            if (ysize2 == 0) continue;
            if (xsize2 == 0) {
                INT32 tilemapnum = ((value3 & 0x00000030) >> 4);
                const UINT32 *regs = RamVReg + 8 + tilemapnum * 4;
                for (INT32 yy = 0; yy <= ysizedraw2; yy++) {
                    INT32 cury_pos = ypos2 + gscrolly - yy;
                    cury_pos = ~cury_pos;
                    cury_pos -= 18;
                    cury_pos &= 0x3ff;
                    if (cury_pos >= 0 && cury_pos <= t->max_y + 1) vt_tileline(t, cury_pos, regs);
                }
                continue;
            }
            ysize2 = tilestable[ysize2];
            xsize2 = tilestable[xsize2];
            UINT32 xinc = ((xsizedraw2) << 16) / ((xsize2));
            UINT32 yinc = ((ysizedraw2) << 16) / ((ysize2));
            UINT32 xscale = xinc / 16, yscale = yinc / 16;
            if (xscale & 0xffff) xscale += (1 << 16) / 16;
            if (yscale & 0xffff) yscale += (1 << 16) / 16;
            xsize2 -= 1; ysize2 -= 1;
            flipx ^= global_xflip;
            flipy ^= global_yflip;
            if (!flipx) xpos2 += (xsizedraw2 / 2); else xpos2 -= (xsizedraw2 / 2);
            ypos2 += (ysizedraw2 / 2);
            if (!flipx) xpos2 -= ((xsize2 + 1) * xinc) >> 16; else xpos2 += (xsize2 * xinc) >> 16;
            if (flipy) ypos2 -= (ysize2 * yinc) >> 16;
            INT32 actualpal = whichpal ? global_pal : pal;
            // ombre dei personaggi (palette 0x43) escluse: restano sotto l'HUD
            ((vtarget *)t)->mark = (dyn_entry && actualpal != 0x43) ? PEN_MARK : 0;
            INT32 color_granularity = ((whichbpp ? global_bpp : bpp) ? 6 : 8);
            INT32 trans = (global_alpha || alpha) ? TR_BLEND : TR_PEN;
            INT32 count = 0;
            for (INT32 xx = 0; xx < xsize2 + 1; xx++) {
                INT32 cx;
                if (!flipx) cx = (xpos + xpos2 + ((xx * xinc) >> 16));
                else cx = (xpos + xpos2 - ((xx * xinc) >> 16));
                cx += gscrollx; cx += 1; cx &= 0x3ff;
                if (cx & 0x200) cx -= 0x400;
                for (INT32 yy = 0; yy < ysize2 + 1; yy++) {
                    INT32 cy;
                    if (flipy) cy = (ypos + ypos2 + ((yy * yinc) >> 16));
                    else cy = (ypos + ypos2 - ((yy * yinc) >> 16));
                    cy += gscrolly;
                    cy = 0x3ff - cy;
                    cy -= 17;
                    cy &= 0x3ff;
                    if (cy & 0x200) cy -= 0x400;
                    vt_sprite(t, tileno + count, actualpal, flipx, flipy, cx, cy, xscale, yscale, color_granularity, trans);
                    count++;
                }
            }
        }
    }
}

extern "C" unsigned cps3_fszoom(void);
static UINT32 fs_zoom(void) {
    UINT32 z = RamVReg[6 * 4 + 3] & 0xff;   // come FBNeo: registro non bufferizzato
    return z > 0x80 ? 0x80 : z;
}

// ------------------------------------------------------------------ riferimento (FBNeo)
static uint32_t *ref_out;
static void ref_put(int x, int y, UINT32 c, UINT32, UINT32) { ref_out[y * CPS3_W + x] = c; }

extern "C" void cps3_render_reference(uint32_t *out) {
    if (!loaded) return;
    UINT32 zoom = fs_zoom();
    UINT32 fsz = zoom << (16 - 6);
    vtarget t; memset(&t, 0, sizeof t);
    t.ref = 1; t.pen = RamScreen;
    t.max_x = ((CPS3_W * fsz) >> 16) - 1;
    t.max_y = ((CPS3_H * fsz) >> 16) - 1;
    {
        UINT32 *p = RamScreen;
        INT32 clrsz = (t.max_x + 1) * sizeof(INT32);
        for (INT32 yy = 0; yy <= t.max_y; yy++, p += 1024) if (clrsz > 0) memset(p, 0, clrsz);
    }
    draw_sprites(&t);
    UINT32 srcy = 0;
    for (INT32 ry = 0; ry < CPS3_H; ry++) {
        const UINT32 *srcb = RamScreen + (srcy >> 16) * 1024;
        UINT32 srcx = 0;
        for (INT32 rx = 0; rx < CPS3_W; rx++) { out[ry * CPS3_W + rx] = Pal32[srcb[srcx >> 16] & 0x1ffff]; srcx += fsz; }
        srcy += fsz;
    }
    ref_out = out;
    draw_text_layer(ref_put);
}

// ------------------------------------------------------------------ HD
static UINT32 *hd_pen; static size_t hd_pen_n;
static INT32 *hd_gx; static size_t hd_gx_n;
static unsigned hd_kx = 1, hd_ky = 1;
static UINT32 *hd_hud; static size_t hd_hud_n;   // HUD dinamico: colore | opacita' nel byte alto (0 = assente)
static int hud_dyn;                               // 0 ORIGINAL, 1 DYNAMIC (HUD alto), 2 DYNAMIC+ (anche in basso)
static int hud_dyn_on;                            // attivo in questo frame (HUD dell'incontro presente)
extern "C" void cps3_set_hud_dynamic(int mode) { hud_dyn = mode < 0 ? 0 : mode > 2 ? 2 : mode; }

extern "C" void cps3_set_hd(uint32_t *buf, unsigned w, unsigned h) {
    hd_out = buf; hd_w = w; hd_h = h;
    hd_kx = w / CPS3_W; hd_ky = h / CPS3_H;
    if (!hd_kx) hd_kx = 1;
    if (!hd_ky) hd_ky = 1;
    size_t n = (size_t)w * h;
    if (n > hd_pen_n) { free(hd_pen); hd_pen = (UINT32 *)malloc(n * sizeof(UINT32)); hd_pen_n = n; }
    if (n > hd_hud_n) { free(hd_hud); hd_hud = (UINT32 *)malloc(n * sizeof(UINT32)); hd_hud_n = n; }
    if (w > hd_gx_n) { free(hd_gx); hd_gx = (INT32 *)malloc(w * sizeof(INT32)); hd_gx_n = w; }
}

// SNKHD: trasparenza dello strato testo (interfaccia) per palette e per tile; peso = opacita' 0..255, 0 = piena
static UINT8 txt_palw[256], txt_tilew[0x200];
static int txt_blend;
extern "C" void cps3_set_text_blend(const uint8_t *palw256, const uint8_t *tilew512) {
    memset(txt_palw, 0, sizeof txt_palw); memset(txt_tilew, 0, sizeof txt_tilew);
    txt_blend = 0;
    if (palw256) for (int i = 0; i < 256; ++i) { txt_palw[i] = palw256[i]; txt_blend |= palw256[i] != 0; }
    if (tilew512) for (int i = 0; i < 0x200; ++i) { txt_tilew[i] = tilew512[i]; txt_blend |= tilew512[i] != 0; }
}

// righe dello strato testo che con HUD PRIORITY passano sotto i personaggi (JoJo):
// alto = righe 0-5 (nomi, vitalita', Stand, timer, ritratti), basso = righe 24-27 (barre MAX)
static inline int hud_dyn_row(int y) { return hud_dyn_on && (y < 48 || (hud_dyn == 2 && y >= 192)); }
static inline UINT32 txt_weight(UINT32 pal, UINT32 tile) {
    UINT32 w = 0;
    if (txt_blend) { w = txt_palw[pal & 0xff]; if (!w) w = txt_tilew[tile & 0x1ff]; }
    return w;
}
static void hud_put(int x, int y, UINT32 c, UINT32 pal, UINT32 tile) {
    if (!hud_dyn_row(y)) return;
    UINT32 w = txt_weight(pal, tile), a = w ? w : 255;
    UINT32 v = (c & 0xffffff) | (a << 24);
    for (unsigned yy = 0; yy < hd_ky; ++yy) {
        UINT32 *row = hd_hud + ((size_t)y * hd_ky + yy) * hd_w + (size_t)x * hd_kx;
        for (unsigned xx = 0; xx < hd_kx; ++xx) row[xx] = v;
    }
}
// HUD dell'incontro presente: riga 1 con le cornici delle barre (palette locale 0x0f, almeno 20 celle)
static int hud_fight_present(void) {
    INT32 bank = (ss_bank_base & 0x01000000) ? 0x0000 : 0x0800, n = 0;
    for (INT32 x = 0; x < 48; ++x) {
        UINT32 d = RamSS[bank + 64 + x];
        if (((d >> 16) & 0x1ff) && ((d & 0x3f) >> 1) == 0x0f) ++n;
    }
    return n >= 20;
}

static void hd_put(int x, int y, UINT32 c, UINT32 pal, UINT32 tile) {
    if (hud_dyn_row(y)) return;   // gia' nel buffer dell'HUD dinamico
    UINT32 w = txt_weight(pal, tile);
    for (unsigned yy = 0; yy < hd_ky; ++yy) {
        uint32_t *row = hd_out + ((size_t)y * hd_ky + yy) * hd_w + (size_t)x * hd_kx;
        if (!w) { for (unsigned xx = 0; xx < hd_kx; ++xx) row[xx] = c; continue; }
        UINT32 iw = 256 - w, rb = (c & 0xff00ff) * w, g = (c & 0x00ff00) * w;
        for (unsigned xx = 0; xx < hd_kx; ++xx) {
            UINT32 d = row[xx];
            UINT32 orb = ((rb + (d & 0xff00ff) * iw) >> 8) & 0xff00ff, og = ((g + (d & 0x00ff00) * iw) >> 8) & 0x00ff00;
            row[xx] = 0xff000000u | orb | og;
        }
    }
}

static void hd_render(void) {
    if (!hd_pen || !hd_out) return;
    UINT32 zoom = fs_zoom();
    UINT32 fsz = zoom << (16 - 6);
    INT32 W = (INT32)(CPS3_W * hd_kx), H = (INT32)(CPS3_H * hd_ky);
    if (!zoom) {
        // zoom 0: canvas di dimensione nulla; FBNeo campiona sempre il pixel (0,0) del canvas,
        // che conserva il valore precedente salvo la riga 0 dei tilemap (ancora disegnata).
        // Titolo di Red Earth: sfondo rosso dopo lo zoom dentro il pianeta.
        vtarget t; memset(&t, 0, sizeof t);
        t.ref = 1; t.pen = RamScreen; t.max_x = -1; t.max_y = -1;
        draw_sprites(&t);
        UINT32 v = RamScreen[0];
        for (size_t i = 0; i < (size_t)W * H; ++i) hd_pen[i] = v;
    } else {
        memset(hd_pen, 0, (size_t)W * H * sizeof(UINT32));
        vtarget t; memset(&t, 0, sizeof t);
        t.pen = hd_pen; t.W = W; t.H = H;
        t.Ax = (INT64)hd_kx * 64; t.Ay = (INT64)hd_ky * 64; t.B = zoom;
        t.max_x = ((CPS3_W * fsz) >> 16) - 1;
        t.max_y = ((CPS3_H * fsz) >> 16) - 1;
        for (INT32 x = 0; x < W; ++x) {
            INT32 c = floor_div((INT64)x * t.B, t.Ax);
            if (c > 1024 - TL_OFF - 1) c = 1024 - TL_OFF - 1;
            hd_gx[x] = c;
        }
        t.gx = hd_gx;
        hud_dyn_on = hud_dyn && hd_hud && hud_fight_present();
        t.dyn = hud_dyn_on;
        draw_sprites(&t);
        RamScreen[0] = hd_pen[0] & ~PEN_MARK;   // pixel (0,0) del canvas, per lo zoom 0
    }
    if (!zoom) hud_dyn_on = 0;
    // pen -> colore
    const UINT32 *pal = Pal32;
    if (hud_dyn_on) {
        // righe dell'HUD dinamico: HUD sopra lo sfondo, sotto gli sprite marcati
        INT32 ya = 48 * (INT32)hd_ky, yb = 192 * (INT32)hd_ky;
        for (INT32 y = 0; y < H; ++y) if (y < ya || (hud_dyn == 2 && y >= yb)) memset(hd_hud + (size_t)y * hd_w, 0, W * sizeof(UINT32));
        draw_text_layer(hud_put);
        for (INT32 y = 0; y < H; ++y) {
            const UINT32 *s = hd_pen + (size_t)y * W;
            uint32_t *d = hd_out + (size_t)y * hd_w;
            if (!(y < ya || (hud_dyn == 2 && y >= yb))) { for (INT32 x = 0; x < W; ++x) d[x] = pal[s[x] & 0x1ffff]; continue; }
            const UINT32 *h = hd_hud + (size_t)y * hd_w;
            for (INT32 x = 0; x < W; ++x) {
                UINT32 p = s[x], c = pal[p & 0x1ffff], hv = h[x];
                if (!(p & PEN_MARK) && hv) {
                    UINT32 a = hv >> 24;
                    if (a == 255) c = 0xff000000u | hv;
                    else {
                        UINT32 ia = 256 - a;
                        UINT32 rb = (((hv & 0xff00ff) * a + (c & 0xff00ff) * ia) >> 8) & 0xff00ff;
                        UINT32 g = (((hv & 0x00ff00) * a + (c & 0x00ff00) * ia) >> 8) & 0x00ff00;
                        c = 0xff000000u | rb | g;
                    }
                }
                d[x] = c;
            }
        }
    } else {
        for (INT32 y = 0; y < H; ++y) {
            const UINT32 *s = hd_pen + (size_t)y * W;
            uint32_t *d = hd_out + (size_t)y * hd_w;
            for (INT32 x = 0; x < W; ++x) d[x] = pal[s[x] & 0x1ffff];
        }
    }
    draw_text_layer(hd_put);
}
extern "C" unsigned cps3_fszoom(void) { return loaded ? fs_zoom() : 0; }

extern "C" const uint32_t *cps3_ssram(void) { return RamSS; }
extern "C" unsigned cps3_ss_bank(void) { return (ss_bank_base & 0x01000000) ? 0x0000 : 0x0800; }
extern "C" unsigned cps3_ss_palbase(void) { return ss_pal_base; }
