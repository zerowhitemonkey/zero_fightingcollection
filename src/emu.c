// SNKHD - wrapper del core Geolith
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emu.h"
#include "geo.h"
#include "geo_lspc.h"
#include "geo_m68k.h"
#include "geo_mixer.h"
#include "geo_neo.h"
#include "miniz.h"
#include "geo_vfs.h"
#include "../cps3/cps3.h"
#include "../snes/snes_core.h"
#include "../psikyo/psikyo.h"
#include "../pgm/pgmcore.h"
#include "stvcore.h"

static uint32_t *vbuf = NULL;
static uint32_t *hdbuf = NULL;
static unsigned hd_w = 320, hd_h = 224;
static int16_t abuf[4096];
static size_t asamps = 0;
static void *romfile = NULL;
static unsigned in_joy[2], in_sys[2];
static int loaded = 0;      // Neo Geo caricato
static int c3loaded = 0;    // CPS3 caricato
static int snloaded = 0;    // SNES caricato
static int pkloaded = 0;    // Psikyo caricato
static int pgloaded = 0;    // PGM caricato
static int stloaded = 0;    // ST-V caricato
static int cursys = SYS_NEOGEO;
static int blend_rebuild(void);

#include <stdarg.h>
int emu_verbose = 0;
static void cb_log(int level, const char *fmt, ...) {
    if (!emu_verbose && level < GEO_LOG_WRN) return;
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}
static void cb_audio(size_t samps) { asamps = samps >> 1; }

static unsigned in_js(unsigned port) {
    unsigned j = in_joy[port & 1], b = 0xff;
    if (j & IN_UP) b &= ~0x01;
    if (j & IN_DOWN) b &= ~0x02;
    if (j & IN_LEFT) b &= ~0x04;
    if (j & IN_RIGHT) b &= ~0x08;
    if ((b & 0x03) == 0) b |= 0x03;
    if ((b & 0x0c) == 0) b |= 0x0c;
    if (j & IN_A) b &= ~0x10;
    if (j & IN_B) b &= ~0x20;
    if (j & IN_C) b &= ~0x40;
    if (j & IN_D) b &= ~0x80;
    return b;
}
static unsigned in_stat_a(void) {
    unsigned c = 0x07 | 0x18; // coin 3/4 assenti (MVS)
    if (in_sys[0] & SYS_COIN) c &= ~0x01;
    if (in_sys[1] & SYS_COIN) c &= ~0x02;
    return c;
}
static unsigned in_stat_b(void) {
    unsigned s = 0x0f | 0x30; // nessuna memory card
    if (in_sys[0] & SYS_START) s &= ~0x01;
    if (in_sys[0] & SYS_SELECT) s &= ~0x02;
    if (in_sys[1] & SYS_START) s &= ~0x04;
    if (in_sys[1] & SYS_SELECT) s &= ~0x08;
    return s;
}
static unsigned in_systype(void) { return 0xc0; }
static unsigned in_dipsw(void) { return 0xff & ~0x40; } // Free Play

static void *readparts(const char *path, size_t *sz);
static void *readfile(const char *path, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return readparts(path, sz);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *d = malloc(n);
    if (d && fread(d, 1, n, f) != (size_t)n) { free(d); d = NULL; }
    fclose(f);
    if (sz) *sz = n;
    return d;
}

// file diviso in parti <path>.001, .002, ... (file > 30 MB trasferiti a pezzi): concatenate in memoria
static void *readparts(const char *path, size_t *sz) {
    char pp[1100]; size_t tot = 0; unsigned char *d = NULL;
    for (int i = 1; i < 1000; ++i) {
        snprintf(pp, sizeof pp, "%s.%03d", path, i);
        FILE *f = fopen(pp, "rb");
        if (!f) break;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        unsigned char *nd = (unsigned char*)realloc(d, tot + n);
        if (!nd || fread(nd + tot, 1, n, f) != (size_t)n) { fclose(f); free(nd ? nd : d); return NULL; }
        d = nd; tot += n; fclose(f);
    }
    if (sz) *sz = tot;
    return d;
}

int emu_system(void) { return cursys; }
void emu_prepare(int sys) { if (!loaded && !c3loaded && !snloaded && !pkloaded && !pgloaded && !stloaded) cursys = sys; }   // sistema del prossimo gioco (prima di emu_set_hd)
unsigned emu_native_w(void) { return cursys == SYS_SNES ? 256 : cursys == SYS_CPS3 ? CPS3_W : cursys == SYS_PSIKYO ? PSIKYO_W : cursys == SYS_PGM ? PGM_W : cursys == SYS_STV ? STV_W : LSPC_WIDTH; }
unsigned emu_native_h(void) { return cursys == SYS_CPS3 ? CPS3_H : cursys == SYS_SNES ? 224 : cursys == SYS_PSIKYO ? PSIKYO_H : cursys == SYS_PGM ? PGM_H : cursys == SYS_STV ? STV_H : LSPC_HEIGHT_VISIBLE; }
double emu_fps(void) { return cursys == SYS_SNES ? (snloaded ? snescore_fps() : SNES_FPS_NTSC) : cursys == SYS_CPS3 ? CPS3_FPS : cursys == SYS_PSIKYO ? PSIKYO_FPS : cursys == SYS_PGM ? PGM_FPS : cursys == SYS_STV ? STV_FPS : EMU_FPS; }

void emu_set_hd(unsigned w, unsigned h) {
    if (cursys == SYS_SNES) {   // SNES: immagine fissa 512x448 (il core non ha un renderer HD), adattata dallo shader
        if (hdbuf && hd_w == SNES_FB_W && hd_h == SNES_FB_H) return;
        free(hdbuf);
        hd_w = SNES_FB_W; hd_h = SNES_FB_H;
        hdbuf = (uint32_t*)calloc((size_t)hd_w * hd_h, sizeof(uint32_t));
        return;
    }
    unsigned nw = emu_native_w(), nh = emu_native_h();
    if (w < nw) w = nw;
    if (h < nh) h = nh;
    if (w > nw * 16) w = nw * 16;
    if (h > nh * 16) h = nh * 16;
    if (cursys == SYS_STV) { if (w > nw * 6) w = nw * 6; if (h > nh * 6) h = nh * 6; }   // ST-V: renderer HD fino a 6x per asse
    if (cursys == SYS_CPS3 || cursys == SYS_PSIKYO || cursys == SYS_PGM || cursys == SYS_STV) { w -= w % nw; h -= h % nh; }   // CPS3 / Psikyo: multiplo intero per asse
    if (hdbuf && w == hd_w && h == hd_h) return;
    hd_w = w; hd_h = h;
    free(hdbuf);
    hdbuf = (uint32_t*)calloc((size_t)w * h, sizeof(uint32_t));
    if (loaded) geo_lspc_set_hd(hdbuf, hd_w, hd_h);
    if (c3loaded) cps3_set_hd(hdbuf, hd_w, hd_h);
    if (pkloaded) psikyo_set_hd(hdbuf, hd_w, hd_h);
    if (pgloaded) pgm_set_hd(hdbuf, hd_w, hd_h);
    if (stloaded) stv_set_hd(hdbuf, hd_w, hd_h);
}
unsigned emu_hd_w(void) { return hd_w; }
unsigned emu_hd_h(void) { return hd_h; }


int emu_load_cps3(const char *zip_path, const char *game, int region) {
    emu_unload();
    cursys = SYS_CPS3;
    if (!cps3_load_zip(zip_path, game, region)) return 0;
    c3loaded = 1;
    { unsigned w = hd_w, h = hd_h; hd_w = hd_h = 0; emu_set_hd(w, h); }   // dimensioni ricontrollate per il CPS3
    cps3_set_hd(hdbuf, hd_w, hd_h);
    return 1;
}

int emu_load_psikyo(const char *zip_path, const char *game, int region) {
    emu_unload();
    cursys = SYS_PSIKYO;
    if (!psikyo_load_zip(zip_path, game, region)) return 0;
    pkloaded = 1;
    { unsigned w = hd_w, h = hd_h; hd_w = hd_h = 0; emu_set_hd(w, h); }   // dimensioni ricontrollate (multiplo intero)
    psikyo_set_hd(hdbuf, hd_w, hd_h);
    blend_rebuild();
    return 1;
}

int emu_load_pgm(const char *zip_path, const char *bios_zip, const char *game, int region) {
    emu_unload();
    cursys = SYS_PGM;
    if (!pgm_load_zip(zip_path, bios_zip, game, region)) return 0;
    pgloaded = 1;
    { unsigned w = hd_w, h = hd_h; hd_w = hd_h = 0; emu_set_hd(w, h); }   // dimensioni ricontrollate (multiplo intero)
    pgm_set_hd(hdbuf, hd_w, hd_h);
    blend_rebuild();
    return 1;
}

int emu_load_stv(const char *zip_path, const char *bios_zip, int region, const char *basedir) {
    emu_unload();
    cursys = SYS_STV;
    if (!stv_load_zip(zip_path, bios_zip, region, basedir)) { fprintf(stderr, "ST-V: %s\n", stv_last_error()); return 0; }
    stloaded = 1;
    { unsigned w = hd_w, h = hd_h; hd_w = hd_h = 0; emu_set_hd(w, h); }   // dimensioni ricontrollate (multiplo intero)
    stv_set_hd(hdbuf, hd_w, hd_h);
    blend_rebuild();
    return 1;
}
const char *emu_stv_error(void) { return stv_last_error(); }
void emu_stv_fx(int mode) { stv_set_fx(mode); }   // SPECIAL EFFECTS ST-V (renderer VDP1/VDP2)

int emu_load_snes(const char *rom_zip) {
    emu_unload();
    cursys = SYS_SNES;
    { unsigned w = hd_w, h = hd_h; hd_w = hd_h = 0; emu_set_hd(w, h); }
    if (!snescore_load_zip(rom_zip)) return 0;
    snloaded = 1;
    return 1;
}

static int load_neo(void *img, size_t sz, const char *bios_zip, int region_jp, const char *nvram_path);
int emu_load(const char *neo_path, const char *bios_zip, int region_jp, const char *nvram_path) {
    size_t sz = 0;
    void *img = readfile(neo_path, &sz);
    if (!img) return 0;
    return load_neo(img, sz, bios_zip, region_jp, nvram_path);
}
// immagine .neo gia' in memoria (costruita da neorom dai romset MAME); l'emulatore ne diventa proprietario
int emu_load_image(void *img, size_t sz, const char *bios_zip, int region_jp, const char *nvram_path) {
    return load_neo(img, sz, bios_zip, region_jp, nvram_path);
}
// nome del BIOS USA nello zip: usa_2slt.bin (zip della collection) o sp-u2.sp1 (neogeo.zip di MAME, stesso CRC)
static const char *bios_us_name(const char *zip) {
    mz_zip_archive z; memset(&z, 0, sizeof z);
    const char *n = "usa_2slt.bin";
    if (mz_zip_reader_init_file(&z, zip, 0)) {
        if (mz_zip_reader_locate_file(&z, "usa_2slt.bin", NULL, 0) < 0 && mz_zip_reader_locate_file(&z, "sp-u2.sp1", NULL, 0) >= 0) n = "sp-u2.sp1";
        mz_zip_reader_end(&z);
    }
    return n;
}
static int load_neo(void *img, size_t sz, const char *bios_zip, int region_jp, const char *nvram_path) {
    emu_unload();
    cursys = SYS_NEOGEO;
    memset(snkhd_dip_mask, 0, sizeof(snkhd_dip_mask));
    geo_log_set_callback(cb_log);
    geo_set_system(SYSTEM_MVS);
    geo_set_region(region_jp ? REGION_JP : REGION_US);
    geo_bios_set_override(region_jp ? "vs-bios.rom" : bios_us_name(bios_zip));
    geo_init();
    if (!vbuf) vbuf = (uint32_t*)calloc(LSPC_WIDTH * LSPC_SCANLINES, sizeof(uint32_t));
    geo_lspc_set_buffer(vbuf);
    if (!hdbuf) emu_set_hd(hd_w, hd_h);
    geo_mixer_set_buffer(abuf);
    geo_mixer_set_callback(cb_audio);
    geo_mixer_init();
    geo_mixer_set_raw(1);
    if (!geo_bios_load_file(bios_zip)) { free(img); return 0; }
    romfile = img;
    if (!romfile || !geo_neo_load(romfile, sz)) return 0;
    if (nvram_path) geo_savedata_load(GEO_SAVEDATA_NVRAM, nvram_path);
    geo_input_set_callback(0, in_js);
    geo_input_set_callback(1, in_js);
    geo_input_sys_set_callback(0, in_stat_a);
    geo_input_sys_set_callback(1, in_stat_b);
    geo_input_sys_set_callback(2, in_systype);
    geo_input_sys_set_callback(3, in_dipsw);
    geo_lspc_set_hd(hdbuf, hd_w, hd_h);
    geo_lspc_set_palette(0);
    geo_lspc_set_sprlimit(96);
    geo_reset(1);
    loaded = 1;
    return 1;
}

/* SNKHD: trasparenze per categoria (ombre, effetti, interfaccia barre, interfaccia scritte, tabella esterna).
   Testo nel formato .bld di FBNeo esteso:
     "min-max [tipo]" / "tile [tipo]"   tile della C ROM (esadecimale)
     "P xx[-yy] [tipo]"                 tutte le sprite con quella palette
     "FP x[-y] [tipo]"                  fix layer: palette fix 0-F
     "FT min-max [tipo]"                fix layer: tile fix
   righe Game/Name/; ignorate. Per le categorie il peso e' quello della categoria; per la tabella esterna
   (BLD del banco di prova) tipo 1 = w1 (FBNeo 75%), tipo 2/3 = w2 (50%). Le categorie "gated" valgono solo
   quando emu_blend_gate(1) (incontro in corso). Se due righe marcano la stessa tile vince la prima categoria. */
typedef struct { char *txt; int w1, w2, gated; } bcat_t;
static bcat_t bcat[EMU_BLEND_NCAT];
static int bgate = 1, bdirty = 0;
static uint8_t *blendtab, *fixtab;
static uint8_t bpal[256], fpal[16];
static size_t blendn, fixn;

static int blend_parse(const char *txt, int w1, int w2) {
    uint8_t wt[4] = { 0, (uint8_t)w1, (uint8_t)w2, (uint8_t)w2 };
    char l[256]; int cnt = 0;
    for (const char *q = txt; *q; ) {
        size_t ll = strcspn(q, "\n");
        size_t cl = ll < sizeof l - 1 ? ll : sizeof l - 1;
        memcpy(l, q, cl); l[cl] = 0;
        q += ll; if (*q) ++q;
        if (!strncmp(l, "Game", 4) || !strncmp(l, "Name", 4) || l[0] == ';' || l[0] == 0) continue;
        int kind = 0; const char *b = l;            // 0 tile, 1 palette sprite, 2 palette fix, 3 tile fix
        if ((l[0] == 'F' || l[0] == 'f') && (l[1] == 'P' || l[1] == 'p')) { kind = 2; b = l + 2; }
        else if ((l[0] == 'F' || l[0] == 'f') && (l[1] == 'T' || l[1] == 't')) { kind = 3; b = l + 2; }
        else if (l[0] == 'P' || l[0] == 'p') { kind = 1; b = l + 1; }
        unsigned lo, hi; int type = 2; const char *d = strchr(b, '-');
        if (d) { if (sscanf(b, "%x", &lo) != 1 || sscanf(d + 1, "%x %d", &hi, &type) < 1) continue; }
        else { if (sscanf(b, "%x %d", &lo, &type) < 1) continue; hi = lo; }
        uint8_t w = wt[type & 3];
        if (!w) continue;
        for (unsigned k = lo; k <= hi; ++k) {
            uint8_t *t = kind == 0 ? (k < blendn ? &blendtab[k] : NULL) : kind == 1 ? (k < 256 ? &bpal[k] : NULL) :
                         kind == 2 ? (k < 16 ? &fpal[k] : NULL) : (k < fixn ? &fixtab[k] : NULL);
            if (!t) break;
            if (!*t) { *t = w; cnt++; }
        }
    }
    return cnt;
}

// CPS3: solo righe "FP a[-b]" (palette dello strato testo, 0-FF) e "FT a[-b]" (tile dello strato testo, 0-1FF)
static uint8_t c3_palw[256], c3_tilew[0x200];
static int c3_parse(const char *txt, int w) {
    char l[256]; int cnt = 0;
    for (const char *q = txt; *q; ) {
        size_t ll = strcspn(q, "\n");
        size_t cl = ll < sizeof l - 1 ? ll : sizeof l - 1;
        memcpy(l, q, cl); l[cl] = 0;
        q += ll; if (*q) ++q;
        int kind = -1;
        if ((l[0] == 'F' || l[0] == 'f') && (l[1] == 'P' || l[1] == 'p')) kind = 0;
        else if ((l[0] == 'F' || l[0] == 'f') && (l[1] == 'T' || l[1] == 't')) kind = 1;
        if (kind < 0) continue;
        unsigned lo, hi; const char *b = l + 2, *d = strchr(b, '-');
        if (sscanf(b, "%x", &lo) != 1) continue;
        hi = lo;
        if (d && sscanf(d + 1, "%x", &hi) != 1) continue;
        for (unsigned k = lo; k <= hi; ++k) {
            uint8_t *t = kind == 0 ? (k < 256 ? &c3_palw[k] : NULL) : (k < 0x200 ? &c3_tilew[k] : NULL);
            if (!t) break;
            if (!*t) { *t = (uint8_t)w; cnt++; }
        }
    }
    return cnt;
}

static int blend_rebuild(void) {
    bdirty = 0;
    if (cursys == SYS_STV) return 0;   // ST-V: trasparenze (SPECIAL EFFECTS) nello Step 3
    if (cursys == SYS_PGM) {      // PGM: solo le ombre (criterio in pgmcfg), opacita' nel renderer
        const bcat_t *b = &bcat[EMU_BLEND_SHADOWS];
        int mode = (!b->txt || !b->w1) ? 0 : b->w1 >= 0xc0 ? 1 : 2;
        if (pgloaded) pgm_set_shadows(mode);
        return mode;
    }
    if (cursys == SYS_PSIKYO) {   // Psikyo: solo le ombre (palette 0xFE), opacita' nel renderer: 0 piene, 1 = 75%, 2 = 50%
        const bcat_t *b = &bcat[EMU_BLEND_SHADOWS];
        int mode = (!b->txt || !b->w1) ? 0 : b->w1 >= 0xc0 ? 1 : 2;
        if (pkloaded) psikyo_set_shadows(mode);
        return mode;
    }
    if (cursys == SYS_CPS3) {
        memset(c3_palw, 0, sizeof c3_palw); memset(c3_tilew, 0, sizeof c3_tilew);
        int cnt = 0;
        for (int c = 0; c < EMU_BLEND_NCAT; ++c) {
            if (!bcat[c].txt || !bcat[c].w1 || (bcat[c].gated && !bgate)) continue;
            cnt += c3_parse(bcat[c].txt, bcat[c].w1);
        }
        if (c3loaded) cps3_set_text_blend(cnt ? c3_palw : NULL, cnt ? c3_tilew : NULL);
        return cnt;
    }
    geo_lspc_set_blend(NULL, 0);
    geo_lspc_set_blend_pal(NULL);
    geo_lspc_set_blend_fix(NULL, NULL, 0);
    if (!loaded) return -1;
    romdata_t *r = geo_romdata_ptr();
    size_t n = r->csz >> 7, fn = r->ssz >> 5;
    if (n != blendn || !blendtab) { free(blendtab); blendtab = (uint8_t*)calloc(n ? n : 1, 1); blendn = n; }
    if (fn != fixn || !fixtab) { free(fixtab); fixtab = (uint8_t*)calloc(fn ? fn : 1, 1); fixn = fn; }
    memset(blendtab, 0, blendn); memset(fixtab, 0, fixn ? fixn : 1);
    memset(bpal, 0, sizeof bpal); memset(fpal, 0, sizeof fpal);
    int cnt = 0, anyfix = 0;
    for (int c = 0; c < EMU_BLEND_NCAT; ++c) {
        if (!bcat[c].txt || (!bcat[c].w1 && !bcat[c].w2) || (bcat[c].gated && !bgate)) continue;
        cnt += blend_parse(bcat[c].txt, bcat[c].w1, bcat[c].w2);
    }
    for (size_t k = 0; k < fixn && !anyfix; ++k) if (fixtab[k]) anyfix = 1;
    geo_lspc_set_blend(blendtab, blendn);
    geo_lspc_set_blend_pal(bpal);
    geo_lspc_set_blend_fix(fpal, anyfix ? fixtab : NULL, anyfix ? fixn : 0);
    return cnt;
}

int emu_blend_set(int cat, const char *txt, int weight, int gated) {
    if (cat < 0 || cat >= EMU_BLEND_NCAT) return -1;
    free(bcat[cat].txt);
    bcat[cat].txt = txt ? strdup(txt) : NULL;
    bcat[cat].w1 = bcat[cat].w2 = txt ? weight : 0;
    bcat[cat].gated = gated;
    return blend_rebuild();
}

void emu_blend_gate(int on) {
    on = on ? 1 : 0;
    if (on != bgate) { bgate = on; bdirty = 1; }
    if (bdirty) blend_rebuild();
}

int emu_blend_tile_present(unsigned lo, unsigned hi) {
    if (!loaded) return 0;   // solo Neo Geo
    extern const void* geo_lspc_vram_ptr(void);
    const uint16_t *v = (const uint16_t*)geo_lspc_vram_ptr();
    unsigned sz = 0;
    for (unsigned i = 1; i < 382; ++i) {
        uint16_t s3 = v[0x8200 + i];
        if (!(s3 & 0x40)) sz = s3 & 0x3f;
        if (!sz || sz > 32) continue;
        for (unsigned r = 0; r < sz; ++r) {
            unsigned t = v[(i << 6) + (r << 1)] | ((v[(i << 6) + (r << 1) + 1] & 0xf0) << 12);
            if (t >= lo && t <= hi) return 1;
        }
    }
    return 0;
}

// CPS3: celle visibili dello strato testo (64 x 28)
static int c3_text_scan(int bypal, unsigned lo, unsigned hi) {
    const uint32_t *ss = cps3_ssram(); unsigned bank = cps3_ss_bank(), pb = cps3_ss_palbase() << 5;
    for (unsigned k = 0; k < 64 * 28; ++k) {
        uint32_t d = ss[(bank + k) & 0x3fff];
        unsigned t = (d >> 16) & 0x1ff;
        if (!t) continue;
        unsigned v = bypal ? ((((d & 0x3f) >> 1) + pb) & 0xff) : t;
        if (v >= lo && v <= hi) return 1;
    }
    return 0;
}

int emu_blend_fixpal_present(unsigned pal) {
    if (c3loaded) return c3_text_scan(1, pal, pal);
    if (!loaded) return 0;
    extern const void* geo_lspc_vram_ptr(void);
    const uint16_t *v = (const uint16_t*)geo_lspc_vram_ptr();
    for (unsigned a = 0x7000; a < 0x7500; ++a) if ((v[a] >> 12) == pal && (v[a] & 0x0fff)) return 1;
    return 0;
}

int emu_blend_fix_present(unsigned lo, unsigned hi) {
    if (c3loaded) return c3_text_scan(0, lo, hi);
    if (!loaded) return 0;
    extern const void* geo_lspc_vram_ptr(void);
    const uint16_t *v = (const uint16_t*)geo_lspc_vram_ptr();
    for (unsigned a = 0x7000; a < 0x7500; ++a) {
        unsigned t = v[a] & 0x0fff;
        if (t >= lo && t <= hi) return 1;
    }
    return 0;
}

int emu_load_blend_text(const char *txt, int w1, int w2) {
    free(bcat[EMU_BLEND_EXT].txt);
    bcat[EMU_BLEND_EXT].txt = txt ? strdup(txt) : NULL;
    bcat[EMU_BLEND_EXT].w1 = w1 ? w1 : 0xff - 0x3f;
    bcat[EMU_BLEND_EXT].w2 = w2 ? w2 : 0xff - 0x7f;
    bcat[EMU_BLEND_EXT].gated = 0;
    return txt ? blend_rebuild() : (blend_rebuild(), -1);
}

// Come emu_load_blend_text, ma da file (banco di prova)
int emu_load_blend(const char *path, int w1, int w2) {
    if (!path) return emu_load_blend_text(NULL, 0, 0);
    FILE *f = fopen(path, "rb");
    if (!f) { emu_load_blend_text(NULL, 0, 0); return -1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc((size_t)(sz > 0 ? sz : 0) + 1);
    size_t rd = buf ? fread(buf, 1, (size_t)(sz > 0 ? sz : 0), f) : 0;
    fclose(f);
    if (!buf) return -1;
    buf[rd] = 0;
    int r = emu_load_blend_text(buf, w1, w2);
    free(buf);
    return r;
}

void emu_unload(void) {
    if (snloaded) { snescore_unload(); snloaded = 0; asamps = 0; }
    if (pkloaded) {
        for (int c = 0; c < EMU_BLEND_NCAT; ++c) { free(bcat[c].txt); bcat[c].txt = NULL; bcat[c].w1 = bcat[c].w2 = 0; }
        bgate = 1;
        psikyo_set_hd(NULL, 0, 0);
        psikyo_unload();
        pkloaded = 0;
        asamps = 0;
    }
    if (pgloaded) {
        for (int c = 0; c < EMU_BLEND_NCAT; ++c) { free(bcat[c].txt); bcat[c].txt = NULL; bcat[c].w1 = bcat[c].w2 = 0; }
        bgate = 1;
        pgm_set_hd(NULL, 0, 0);
        pgm_unload();
        pgloaded = 0;
        asamps = 0;
    }
    if (stloaded) {
        for (int c = 0; c < EMU_BLEND_NCAT; ++c) { free(bcat[c].txt); bcat[c].txt = NULL; bcat[c].w1 = bcat[c].w2 = 0; }
        bgate = 1;
        stv_set_hd(NULL, 0, 0);
        stv_unload();
        stloaded = 0;
        asamps = 0;
    }
    if (c3loaded) {
        for (int c = 0; c < EMU_BLEND_NCAT; ++c) { free(bcat[c].txt); bcat[c].txt = NULL; bcat[c].w1 = bcat[c].w2 = 0; }
        bgate = 1;
        cps3_set_text_blend(NULL, NULL);
        cps3_unload();
        c3loaded = 0;
        asamps = 0;
    }
    if (!loaded) return;
    geo_lspc_set_blend(NULL, 0);
    geo_lspc_set_blend_pal(NULL);
    geo_lspc_set_blend_fix(NULL, NULL, 0);
    for (int c = 0; c < EMU_BLEND_NCAT; ++c) { free(bcat[c].txt); bcat[c].txt = NULL; bcat[c].w1 = bcat[c].w2 = 0; }
    free(blendtab); blendtab = NULL; blendn = 0;
    free(fixtab); fixtab = NULL; fixn = 0;
    bgate = 1;
    geo_mixer_deinit();
    geo_deinit();
    geo_bios_unload();
    free(romfile);
    romfile = NULL;
    loaded = 0;
}

void emu_write8(uint32_t addr, uint8_t v) { if (snloaded) snescore_wram_write8(addr, v); else if (pkloaded) psikyo_write8(addr, v); else if (pgloaded) pgm_write8(addr, v); else if (stloaded) stv_write8(addr, v); }
uint8_t emu_read8(uint32_t addr) {
    if (snloaded) return snescore_wram8(addr);
    if (c3loaded) return cps3_read8(addr);
    if (pkloaded) return psikyo_read8(addr);
    if (pgloaded) return pgm_read8(addr);
    if (stloaded) return stv_read8(addr);
    if (loaded) return emu_ram()[addr & 0xffff];
    return 0;
}

int emu_save_nvram(const char *path) {
    return loaded ? geo_savedata_save(GEO_SAVEDATA_NVRAM, path) : 0;
}

void emu_set_softdip(int idx, int val) {
    if (idx < 0 || idx > 15) return;
    if (val < 0) snkhd_dip_mask[idx] = 0;
    else { snkhd_dip_val[idx] = (uint8_t)val; snkhd_dip_mask[idx] = 1; }
}

void emu_set_input(int port, unsigned joy, unsigned sys) {
    in_joy[port & 1] = joy;
    in_sys[port & 1] = sys;
    if (snloaded) {   // IN_A..F = Y X L B A R (ordine dei tasti SNES nel menu INPUT)
        unsigned b = 0;
        if (joy & IN_UP) b |= SN_UP;
        if (joy & IN_DOWN) b |= SN_DOWN;
        if (joy & IN_LEFT) b |= SN_LEFT;
        if (joy & IN_RIGHT) b |= SN_RIGHT;
        if (joy & IN_A) b |= SN_Y;
        if (joy & IN_B) b |= SN_X;
        if (joy & IN_C) b |= SN_L;
        if (joy & IN_D) b |= SN_B;
        if (joy & IN_E) b |= SN_A;
        if (joy & IN_F) b |= SN_R;
        if (sys & SYS_START) b |= SN_START;
        if (sys & SYS_SELECT) b |= SN_SELECT;
        snescore_set_input(port & 1, b);
        return;
    }
    if (pgloaded) {   // IN_A-D = PGM_B1-B4 (stessi bit), START/COIN rimappati
        pgm_set_input(port, joy & 0xff, ((sys & SYS_START) ? PGM_START : 0) | ((sys & SYS_COIN) ? PGM_COIN : 0));
        return;
    }
    if (stloaded) { stv_set_input(port, joy & 0x3ff, ((sys & SYS_START) ? STV_START : 0) | ((sys & SYS_COIN) ? STV_COIN : 0)); return; }   // IN_A-F = STV_B1-B6 (stessi bit)
    if (pkloaded) { psikyo_set_input(port, joy & 0xff, sys & (SYS_START | SYS_COIN)); return; }   // stessi bit (IN_A-D = PSK_B1-B4)
    if (c3loaded) cps3_set_input(port, joy, sys);   // stessi bit (IN_A-F = C3_B1-B6, SYS_START/COIN = C3_START/COIN)
}

void emu_set_overclock(int on) {
    // entrambi i sistemi (solo variabili): l'impostazione vale qualunque sia il prossimo gioco caricato
    cps3_set_overclock(on ? 200 : 100);   // SH-2 al 200%
    psikyo_set_overclock(on ? 200 : 100); // SH-2 al 200% (Psikyo)
    pgm_set_overclock(on ? 200 : 100);    // 68000 + ARM7 al 200% (PGM)
    stv_set_overclock(on ? 200 : 100);    // ST-V: non usato (Mednafen)
    geo_set_div68k(on ? 0 : 1);           // 68000 al 200%
}
// SNES: il core scrive gia' 0xAARRGGBB (come la texture dell'app); si forza solo l'alfa
static void sn_copy_video(void) {
    unsigned w, h, p; const uint32_t *v = snescore_video(&w, &h, &p);
    if (!v || !hdbuf) return;
    for (size_t k = 0, n = (size_t)w * h; k < n; ++k) { uint32_t c = v[k]; hdbuf[k] = c | 0xff000000u; }
}
void emu_frame(void) { if (snloaded) { snescore_frame(1); sn_copy_video(); } else if (c3loaded) cps3_frame(1); else if (pkloaded) psikyo_frame(1); else if (pgloaded) pgm_frame(1); else if (stloaded) stv_frame(1); else if (loaded) geo_exec(); }
void emu_frame_fast(void) {
    if (snloaded) { snescore_frame(0); return; }
    if (c3loaded) { cps3_frame(0); return; }
    if (pkloaded) { psikyo_frame(0); return; }
    if (pgloaded) { pgm_frame(0); return; }
    if (stloaded) { stv_frame(0); return; }
    if (!loaded) return;
    geo_lspc_set_skip_render(1);
    geo_exec();
    geo_lspc_set_skip_render(0);
}

const uint32_t *emu_video(unsigned *w, unsigned *h, unsigned *pitch) {
    *w = hd_w; *h = hd_h; *pitch = hd_w;
    return hdbuf;
}
const uint32_t *emu_video_native(unsigned *pitch) {
    *pitch = LSPC_WIDTH;
    return vbuf + LSPC_LINE_VISIBLE * LSPC_WIDTH;
}
const int16_t *emu_audio(size_t *frames) {
    if (snloaded) return snescore_audio(frames);
    if (c3loaded) return cps3_audio(frames);
    if (pkloaded) return psikyo_audio(frames);
    if (pgloaded) return pgm_audio(frames);
    if (stloaded) return stv_audio(frames);
    *frames = asamps; return abuf;
}
double emu_audio_rate(void) { return cursys == SYS_SNES ? SNES_AUDIO_PER_FRAME * emu_fps() : cursys == SYS_CPS3 ? CPS3_AUDIO_RATE : cursys == SYS_PSIKYO ? PSIKYO_AUDIO_RATE : cursys == SYS_PGM ? PGM_AUDIO_RATE : cursys == SYS_STV ? STV_AUDIO_RATE : 24000000.0 / 432.0; }
const uint8_t *emu_prom(size_t *sz) {
    if (!loaded) { *sz = 0; return NULL; }
    romdata_t *r = geo_romdata_ptr(); *sz = r->psz; return r->p;
}
uint8_t *emu_ram(void) { return (uint8_t*)geo_mem_ptr(GEO_MEMTYPE_MAINRAM, NULL); }
