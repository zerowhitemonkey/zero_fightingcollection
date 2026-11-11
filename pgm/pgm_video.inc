// SNKHD - video PGM (incluso da pgm_core.cpp)
// Semantica = pgm_draw.cpp di FBNeo (OldCodeMode 0): fondale = palette 0x3ff, sprite a priorita' 1 sotto lo
// sfondo, sfondo 32x32 (5 bpp) con rowscroll, sprite a priorita' 0, testo 8x8 (4 bpp).
// Lo sprite disegnato dopo copre quello prima (qualunque priorita'), come FBNeo.
// HD: scala intera kx x ky; strati di sfondo e testo (mai zoomati dall'hardware) campionati dal nativo,
// sprite ricampionati in modo uniforme sull'area HD che coprono (negli zoom niente righe/colonne perse).
// Ombre: sprite della palette ombra -> trasparenza opzionale (una sola miscela per pixel).

static uint32_t pal32[0x1001];
static uint16_t *nat_bg, *nat_tx;           // indici colore nativi (0xffff = trasparente)
static uint16_t *hspr;                      // sprite HD: colore | 0x8000 priorita' 1 (0xffff = vuoto)
static uint16_t *hsh;                       // ombra HD: colore | 0x8000 priorita' 1 (0xffff = nessuna)
static size_t hspr_cap;
static uint16_t *sdec; static size_t sdec_cap;
static int *colmap, *rowmap; static size_t map_cap;
static UINT8 *tiletrans_t;
static int ntilemask;
static int shadow_mode = 0, shadow_high = -1;
static UINT32 shadow_lo = 1, shadow_hi = 0;
static pgm_sprite_cb spr_cb;
static UINT32 hide_lo = 1, hide_hi = 0;
extern "C" void pgm_debug_hide(uint32_t lo, uint32_t hi) { hide_lo = lo; hide_hi = hi; }

extern "C" void pgm_set_shadows(int m) { shadow_mode = m < 0 ? 0 : m > 2 ? 2 : m; }
extern "C" void pgm_set_shadow_sprites(uint32_t lo, uint32_t hi, int high) { shadow_lo = lo; shadow_hi = hi; shadow_high = high; }
extern "C" void pgm_set_sprite_cb(pgm_sprite_cb cb) { spr_cb = cb; }

static void video_alloc() {
    nat_bg = (uint16_t *)malloc(PGM_W * PGM_H * 2);
    nat_tx = (uint16_t *)malloc(PGM_W * PGM_H * 2);
    ntilemask = ((nPGMTileROMLen / 5) * 8) / 0x400;
    tiletrans_t = (UINT8 *)calloc(ntilemask ? ntilemask : 1, 1);
    for (int t = 0; t < ntilemask; ++t) {
        const UINT8 *g = PGMTileROMExp + (size_t)t * 0x400;
        for (int j = 0; j < 0x400; ++j) if (g[j] != 0x1f) { tiletrans_t[t] = 1; break; }
    }
}
static void video_free() {
    free(nat_bg); free(nat_tx); free(hspr); free(hsh); free(sdec); free(colmap); free(rowmap); free(tiletrans_t);
    nat_bg = nat_tx = hspr = hsh = sdec = NULL; colmap = rowmap = NULL; tiletrans_t = NULL;
    hspr_cap = sdec_cap = map_cap = 0;
}

static inline uint32_t calc_col(uint16_t c) {
    int r = (c & 0x7c00) >> 7; r |= r >> 5;
    int g = (c & 0x03e0) >> 2; g |= g >> 5;
    int b = (c & 0x001f) << 3; b |= b >> 5;
    return 0xff000000u | (r << 16) | (g << 8) | b;
}
static void build_palette() {
    for (int i = 0; i < 0x1000; ++i) pal32[i] = calc_col(PGMPalRAM[i]);
    pal32[0x1000] = pal32[0x3ff];
}

// ---- strati nativi: sfondo (con rowscroll, come MAME) e testo
static void render_bg_native() {
    const UINT16 *vram = (const UINT16 *)PGMBgRAM;
    int xs = pgm_bg_scrollx, ys = pgm_bg_scrolly;
    for (int y = 0; y < PGM_H; ++y) {
        uint16_t *d = nat_bg + y * PGM_W;
        int sy = (ys + y) & 0x1ff;
        int sx0 = (xs + PGMRowRAM[y]) & 0x7ff;
        const UINT16 *row = vram + ((sy >> 5) & 15) * 128;
        int x = 0;
        while (x < PGM_W) {
            int px = (sx0 + x) & 0x7ff;
            int run = 32 - (px & 31); if (run > PGM_W - x) run = PGM_W - x;
            int code = row[(px >> 5) * 2], attr = row[(px >> 5) * 2 + 1];
            if (code >= ntilemask || !tiletrans_t[code]) { for (int k = 0; k < run; ++k) d[x + k] = 0xffff; x += run; continue; }
            int fx = (attr & 0x40) ? 31 : 0, fy = (attr & 0x80) ? 31 : 0;
            int color = (((attr & 0x3e) >> 1) | 0x20) << 5;
            const UINT8 *g = PGMTileROMExp + (size_t)code * 0x400 + (((sy & 31) ^ fy) << 5);
            for (int k = 0; k < run; ++k) {
                int p = g[((px + k) & 31) ^ fx];
                d[x + k] = p == 0x1f ? 0xffff : (uint16_t)(color | p);
            }
            x += run;
        }
    }
}
static void render_tx_native() {
    const UINT16 *vram = (const UINT16 *)PGMTxtRAM;
    int xs = pgm_fg_scrollx & 0x1ff, ys = pgm_fg_scrolly & 0xff;
    for (int y = 0; y < PGM_H; ++y) {
        uint16_t *d = nat_tx + y * PGM_W;
        int py = (y + ys) & 0xff;
        const UINT16 *row = vram + (py >> 3) * 128;
        for (int x = 0; x < PGM_W; ++x) {
            int px = (x + xs) & 0x1ff;
            int code = row[(px >> 3) * 2], attr = row[(px >> 3) * 2 + 1];
            int fx = (attr & 0x40) ? 7 : 0, fy = (attr & 0x80) ? 7 : 0;
            int p = PGMTileROM[(size_t)code * 0x40 + (((py & 7) ^ fy) << 3) + ((px & 7) ^ fx)];
            d[x] = p == 0xf ? 0xffff : (uint16_t)(((((attr & 0x3e) >> 1) | 0x80) << 4) | p);
        }
    }
}

// ---- sprite
struct SprInfo { int xpos, ypos, wide, high, palt, flip, prio; UINT32 boff, xzoom, yzoom; int xgrow, ygrow; int shadow; };

static int sprite_list(SprInfo *out, int max) {
    const UINT16 *s = PGMSprBuf, *end = PGMSprBuf + 0x1000 / 2;
    const UINT16 *zt = PGMZoomRAM;
    int n = 0;
    for (; s < end && n < max; s += 8) {
        if ((s[4] & 0x7fff) == 0) break;
        SprInfo &o = out[n];
        o.xpos = s[0] & 0x07ff; o.ypos = s[1] & 0x03ff;
        int xzom = (s[0] & 0x7800) >> 11, yzom = (s[1] & 0x7800) >> 11;
        o.xgrow = (s[0] & 0x8000) >> 15; o.ygrow = (s[1] & 0x8000) >> 15;
        o.palt = (s[2] & 0x1f00) >> 8; o.flip = (s[2] & 0x6000) >> 13;
        o.boff = ((s[2] & 0x007f) << 16) | s[3];
        o.wide = (s[4] & 0x7e00) >> 9; o.prio = (s[2] & 0x0080) >> 7; o.high = s[4] & 0x01ff;
        if (o.xgrow) xzom = 0x10 - xzom;
        if (o.ygrow) yzom = 0x10 - yzom;
        o.xzoom = (xzom & 0x10) ? 0 : ((UINT32)zt[xzom * 2] << 16) | zt[xzom * 2 + 1];
        o.yzoom = (yzom & 0x10) ? 0 : ((UINT32)zt[yzom * 2] << 16) | zt[yzom * 2 + 1];
        if (o.xpos > 0x3ff) o.xpos -= 0x800;
        if (o.ypos > 0x1ff) o.ypos -= 0x400;
        o.shadow = (o.boff >= shadow_lo && o.boff <= shadow_hi && (shadow_high < 0 || o.high == shadow_high));
        if (spr_cb) spr_cb(n, s);
        if (o.wide == 0) continue;
        if (o.boff >= hide_lo && o.boff <= hide_hi) continue;
        ++n;
    }
    return n;
}

// mappa in ordine di uscita -> colonna sorgente (gia' specchiata); restituisce la dimensione in uscita
static int zoom_map(int *map, int src, UINT32 zoom, int grow, int flip) {
    int n = 0;
    for (int c = 0; c < src; ++c) {
        int s = flip ? src - 1 - c : c;
        int bit = zoom ? (int)((zoom >> (c & 0x1f)) & 1) : 0;
        if (bit && grow) { map[n++] = s; map[n++] = s; }
        else if (bit) continue;
        else map[n++] = s;
    }
    return n;
}

// decodifica le prime 'rows' righe dello sprite in sdec (wide*16 per riga, 0xffff = trasparente)
static void sprite_decode(const SprInfo &o, int rows) {
    int w = o.wide * 16;
    size_t need = (size_t)w * (rows > 0 ? rows : 1);
    if (need > sdec_cap) { free(sdec); sdec = (uint16_t *)malloc(need * 2); sdec_cap = need; }
    const UINT8 *b = PGMSPRMaskROM, *a = PGMSPRColROM;
    UINT32 bm = nPGMSPRMaskMaskLen, am = nPGMSPRColMaskLen;
    UINT32 bo = o.boff * 2;
    UINT32 ao = ((UINT32)b[(bo + 3) & bm] << 24) | (b[(bo + 2) & bm] << 16) | (b[(bo + 1) & bm] << 8) | b[bo & bm];
    ao = (ao >> 2) * 3;
    bo += 4;
    int palt = o.palt << 5;
    uint16_t *d = sdec;
    for (int y = 0; y < rows; ++y)
        for (int xg = 0; xg < w; xg += 8) {
            int m = b[bo++ & bm] ^ 0xff;
            for (int x = 0; x < 8; ++x, ++d) {
                if (m & (1 << x)) *d = (uint16_t)(a[ao++ & am] | palt);
                else *d = 0xffff;
            }
        }
}

static void ensure_maps(size_t n) {
    if (n > map_cap) { free(colmap); free(rowmap); colmap = (int *)malloc(n * sizeof(int)); rowmap = (int *)malloc(n * sizeof(int)); map_cap = n; }
}

// disegno di uno sprite in un buffer (dst w x h, scala kx x ky). k = 1: semantica FBNeo esatta.
static void draw_sprite_scaled(const SprInfo &o, uint16_t *dst, uint16_t *dsh, int W, int H, int kx, int ky) {
    int sw = o.wide * 16, sh = o.high;
    if (sh <= 0) return;
    ensure_maps((size_t)(sw > sh ? sw : sh) * 2 * (kx > ky ? kx : ky) + 64);
    static int ncol[2048], nrow[1024];
    int wout = zoom_map(ncol, sw, o.xzoom, o.xgrow, o.flip & 1);
    int hout = zoom_map(nrow, sh, o.yzoom, o.ygrow, (o.flip & 2) ? 1 : 0);
    if (!wout || !hout) return;
    // fuori schermo?
    if (o.xpos >= PGM_W || o.xpos + wout <= 0 || o.ypos >= PGM_H || o.ypos + hout <= 0) return;
    int hw = wout * kx, hh = hout * ky;
    int x0 = o.xpos * kx, y0 = o.ypos * ky;
    // colonne/righe HD -> sorgente
    if (kx == 1) for (int i = 0; i < hw; ++i) colmap[i] = ncol[i];
    else for (int i = 0; i < hw; ++i) {
        int u = (int)(((long long)(2 * i + 1) * sw) / (2LL * hw));        // ordine di uscita
        colmap[i] = (o.flip & 1) ? sw - 1 - u : u;
    }
    if (ky == 1) for (int j = 0; j < hh; ++j) rowmap[j] = nrow[j];
    else for (int j = 0; j < hh; ++j) {
        int v = (int)(((long long)(2 * j + 1) * sh) / (2LL * hh));
        rowmap[j] = (o.flip & 2) ? sh - 1 - v : v;
    }
    // righe sorgente necessarie (solo quelle visibili)
    int jmin = y0 < 0 ? -y0 : 0, jmax = hh; if (y0 + jmax > H) jmax = H - y0;
    int imin = x0 < 0 ? -x0 : 0, imax = hw; if (x0 + imax > W) imax = W - x0;
    if (jmin >= jmax || imin >= imax) return;
    int maxrow = 0;
    for (int j = jmin; j < jmax; ++j) if (rowmap[j] > maxrow) maxrow = rowmap[j];
    sprite_decode(o, maxrow + 1);
    uint16_t pr = o.prio ? 0x8000 : 0;
    for (int j = jmin; j < jmax; ++j) {
        const uint16_t *srow = sdec + (size_t)rowmap[j] * sw;
        uint16_t *d = dst + (size_t)(y0 + j) * W + x0;
        uint16_t *ds = dsh ? dsh + (size_t)(y0 + j) * W + x0 : NULL;
        if (o.shadow && ds) {
            for (int i = imin; i < imax; ++i) { uint16_t p = srow[colmap[i]]; if (p != 0xffff) ds[i] = p | pr; }
        } else if (ds) {
            for (int i = imin; i < imax; ++i) { uint16_t p = srow[colmap[i]]; if (p != 0xffff) { d[i] = p | pr; ds[i] = 0xffff; } }
        } else {
            for (int i = imin; i < imax; ++i) { uint16_t p = srow[colmap[i]]; if (p != 0xffff) d[i] = p | pr; }
        }
    }
}

static inline uint32_t blend_px(uint32_t d, uint32_t s, int w) {   // w = peso dell'ombra su 256
    uint32_t rb = ((((s & 0xff00ff) * w) + ((d & 0xff00ff) * (256 - w))) >> 8) & 0xff00ff;
    uint32_t g = ((((s & 0x00ff00) * w) + ((d & 0x00ff00) * (256 - w))) >> 8) & 0x00ff00;
    return 0xff000000u | rb | g;
}

static SprInfo sprs[512];

// composizione: out W x H (scala kx x ky)
static void compose(uint32_t *out, int W, int H, int kx, int ky, uint16_t *sp, uint16_t *sh) {
    int show_hi = !(pgm_video_control & 0x2000), show_tx = !(pgm_video_control & 0x0800);
    int sw = shadow_mode == 1 ? 0xc0 : 0x80;
    uint32_t back = pal32[0x1000];
    static int xmap[PGM_W * 16];
    for (int x = 0; x < W; ++x) xmap[x] = x / kx;
    for (int y = 0; y < H; ++y) {
        int ny = y / ky;
        const uint16_t *bg = nat_bg + ny * PGM_W, *tx = nat_tx + ny * PGM_W;
        const uint16_t *s = sp + (size_t)y * W;
        const uint16_t *h = sh ? sh + (size_t)y * W : NULL;
        uint32_t *o = out + (size_t)y * W;
        for (int x = 0; x < W; ++x) {
            int nx = xmap[x];
            uint16_t t = tx[nx];
            if (show_tx && t != 0xffff) { o[x] = pal32[t]; continue; }
            uint16_t p = s[x], b = bg[nx];
            uint32_t c;
            int layer;   // 2 = sprite alto, 1 = sfondo, 0 = sprite basso/fondale
            if (p != 0xffff && !(p & 0x8000) && show_hi) { c = pal32[p & 0x3ff]; layer = 2; }
            else if (b != 0xffff) { c = pal32[b]; layer = 1; }
            else if (p != 0xffff && (p & 0x8000)) { c = pal32[p & 0x3ff]; layer = 0; }
            else { c = back; layer = 0; }
            if (h) {
                uint16_t q = h[x];
                if (q != 0xffff) {
                    int ql = (q & 0x8000) ? 0 : 2;      // l'ombra e' sopra solo agli strati sotto la sua priorita'
                    if (ql == 2 ? show_hi : (layer == 0))
                        c = blend_px(c, pal32[q & 0x3ff], sw);
                }
            }
            o[x] = c;
        }
    }
}

static void render_scaled(uint32_t *out, int W, int H, int kx, int ky) {
    build_palette();
    render_bg_native();
    render_tx_native();
    size_t n = (size_t)W * H;
    if (n > hspr_cap) { free(hspr); free(hsh); hspr = (uint16_t *)malloc(n * 2); hsh = (uint16_t *)malloc(n * 2); hspr_cap = n; }
    memset(hspr, 0xff, n * 2);
    int use_sh = shadow_mode != 0 && shadow_lo <= shadow_hi;
    if (use_sh) memset(hsh, 0xff, n * 2);
    int ns = sprite_list(sprs, 512);
    for (int i = 0; i < ns; ++i) {
        SprInfo o = sprs[i];
        if (!use_sh) o.shadow = 0;
        draw_sprite_scaled(o, hspr, use_sh ? hsh : NULL, W, H, kx, ky);
    }
    compose(out, W, H, kx, ky, hspr, use_sh ? hsh : NULL);
}

static void pgm_render_hd() {
    int kx = hd_w / PGM_W, ky = hd_h / PGM_H;
    if (kx < 1 || ky < 1 || kx > 16 || ky > 16) return;
    render_scaled(hd_buf, kx * PGM_W, ky * PGM_H, kx, ky);
}

// riferimento: stessa semantica di pgm_draw.cpp a 448x224 (ombre sempre piene)
extern "C" void pgm_render_reference(uint32_t *out) {
    if (!loaded) return;
    int sm = shadow_mode; shadow_mode = 0;
    render_scaled(out, PGM_W, PGM_H, 1, 1);
    shadow_mode = sm;
}
