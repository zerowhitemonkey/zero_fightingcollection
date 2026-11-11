// SNKHD - core IGS PGM: ambiente FBNeo minimo (burn), caricamento zip, API C, video HD.
// Driver, CPU (Musashi 68000, Z80, ARM7), ICS2115, V3021 e protezione ASIC27A tipo 2 = sorgenti FBNeo
// in pgm/fbn (licenza FBNeo, uso non commerciale). Tutti i simboli tranne pgm_* vengono resi locali
// al momento del link (ld -r + objcopy --keep-global-symbols), quindi non entrano in conflitto con
// Geolith (Musashi, Z80) ne' con il core CPS3.
#include "burnint.h"
#include "pgm.h"
#include "ics2115.h"
#include "timer.h"
#include "pgmcore.h"
#include <miniz.h>
#include <math.h>
#include <time.h>

// ------------------------------------------------------------------ ambiente burn
INT32 nBurnFPS = 6000;
INT32 nBurnSoundRate = PGM_AUDIO_RATE;
INT32 nBurnSoundLen = 800;
INT16 *pBurnSoundOut = NULL;
UINT8 *pBurnDraw = NULL;
INT32 nBurnBpp = 4;
UINT8 nBurnLayer = 0xff;
UINT8 nSpriteEnable = 0xff;
bool bDoIpsPatch = false;
bool bBurnUseBlend = false;
UINT32 nIpsDrvDefine = 0, nIpsMemExpLen[SND2_ROM + 1] = {0};
BurnRomInfo *pDataRomDesc = NULL;
UINT32 *pBurnDrvPalette = NULL;
INT32 nBurnCPUSpeedAdjust = 0x100;
INT32 nInterpolation = 3;
UINT8 HackCodeDip = 0;
INT32 nScreenWidth = PGM_W, nScreenHeight = PGM_H;
INT32 (*BurnAcb)(struct BurnArea *pba) = NULL;
UINT8 DebugCPU_ARM7Initted = 0, DebugCPU_SekInitted = 0, DebugCPU_ZetInitted = 0;
UINT8 DebugDev_V3021Initted = 0, DebugSnd_ICS2115Initted = 0;
extern "C" { INT16 Precalc[4096 * 4]; }

static INT32 __cdecl bprintf_null(INT32, TCHAR *, ...) { return 0; }
INT32 (__cdecl *bprintf)(INT32 nStatus, TCHAR *szFormat, ...) = bprintf_null;
static UINT32 __cdecl high_col(INT32 r, INT32 g, INT32 b, INT32) { return 0xff000000u | (r << 16) | (g << 8) | b; }
UINT32 (__cdecl *BurnHighCol)(INT32 r, INT32 g, INT32 b, INT32 i) = high_col;

INT32 BurnSetRefreshRate(double d) {
    nBurnFPS = (INT32)(d * 100.0 + 0.5);
    nBurnSoundLen = (nBurnSoundRate * 100 + nBurnFPS / 2) / nBurnFPS;
    return 0;
}
UINT8 *_BurnMalloc(INT32 size, char *, INT32) { UINT8 *p = (UINT8 *)malloc(size); if (p) memset(p, 0, size); return p; }
UINT8 *BurnRealloc(void *ptr, INT32 size) { return (UINT8 *)realloc(ptr, size); }
void _BurnFree(void *ptr) { free(ptr); }
void CpuCheatRegister(INT32, cpu_core_config *) {}
void HiscoreReset(INT32) {}
INT32 BurnDrvRedraw() { return 0; }
void BurnGetLocalTime(tm *t) {
    if (!t) return;
    time_t n = time(NULL);
    struct tm *l = localtime(&n);
    if (l) *t = *l; else memset(t, 0, sizeof *t);
}
// z80 daisy chain / ctc: non usati dal PGM
INT32 z80daisy_has_ctc = 0;
void z80ctc_timer_update(INT32) {}
#include "z80daisy.h"
void z80daisy_init(INT32, INT32) {}
void z80daisy_exit() {}
void z80daisy_reset(const struct z80_irq_daisy_chain *) {}
INT32 z80daisy_update_irq_state(const struct z80_irq_daisy_chain *) { return 0; }
INT32 z80daisy_call_ack_device(const struct z80_irq_daisy_chain *) { return 0; }
void z80daisy_call_reti_device(const struct z80_irq_daisy_chain *) {}
void z80daisy_scan(INT32) {}

// ------------------------------------------------------------------ ROM del gioco
#define HW_PGM_ARM (HARDWARE_IGS_PGM | HARDWARE_IGS_USE_ARM_CPU)
struct RomEnt { const char *name; UINT32 len, crc, type; };
static const RomEnt martmast_roms[] = {
    { "v104_32m.u9",                   0x0400000, 0xcfd9dff4, 1 | BRF_PRG | BRF_ESS },
    { "t1000.u3",                      0x0800000, 0xbbf879b5, 2 | BRF_GRA },
    { "a1000.u3",                      0x0800000, 0x43577ac8, 3 | BRF_GRA },
    { "a1001.u4",                      0x0800000, 0xfe7a476f, 3 | BRF_GRA },
    { "a1002.u6",                      0x0800000, 0x62e33d38, 3 | BRF_GRA },
    { "a1003.u8",                      0x0800000, 0xb2c4945a, 3 | BRF_GRA },
    { "a1004.u10",                     0x0400000, 0x9fd3f5fd, 3 | BRF_GRA },
    { "b1000.u9",                      0x0800000, 0xc5961f6f, 4 | BRF_GRA },
    { "b1001.u11",                     0x0800000, 0x0b7e1c06, 4 | BRF_GRA },
    { "m1000.u5",                      0x0800000, 0xed407ae8, 5 | BRF_SND },
    { "m1001.u7",                      0x0400000, 0x662d2d48, 5 | BRF_SND },
    { "martial_masters_v102_usa.asic", 0x0004000, 0xa6c0828c, 7 | BRF_PRG | BRF_ESS },
    { "v102_16m.u10",                  0x0200000, 0x18b745e6, 8 | BRF_PRG | BRF_ESS },
};
static const RomEnt bios_roms[] = {
    { "pgm_t01s.rom", 0x0200000, 0x1a7123a0, BRF_GRA | BRF_BIOS },   // 0x80 testo/tile
    { "pgm_m01s.rom", 0x0200000, 0x45ae7159, BRF_SND | BRF_BIOS },   // 0x81 campioni
    { "pgm_p01s.u20", 0x0020000, 0xe42b166e, BRF_PRG | BRF_BIOS },   // 0x82 BIOS 68K vecchia
    { "pgm_p02s.u20", 0x0020000, 0x78c15fa2, BRF_PRG | BRF_BIOS },   // 0x83 BIOS 68K nuova
};
static const RomEnt *game_roms = martmast_roms;
static int game_nroms = (int)(sizeof martmast_roms / sizeof martmast_roms[0]);
static const char *game_name = "martmast";

static const RomEnt *rom_ent(UINT32 i) {
    if (i >= 0x80 && i < 0x80 + 4) return &bios_roms[i - 0x80];
    if ((int)i < game_nroms) return &game_roms[i];
    return NULL;
}
char *BurnDrvGetTextA(UINT32) { return (char *)game_name; }
INT32 BurnDrvGetHardwareCode() { return HW_PGM_ARM; }
INT32 BurnDrvGetRomName(char **psz, UINT32 i, INT32) {
    const RomEnt *e = rom_ent(i);
    if (!e) return 1;
    if (psz) *psz = (char *)e->name;
    return 0;
}
INT32 BurnDrvGetRomInfo(struct BurnRomInfo *ri, UINT32 i) {
    const RomEnt *e = rom_ent(i);
    if (!e) { if (ri) memset(ri, 0, sizeof *ri); return 1; }
    if (ri) { memset(ri, 0, sizeof *ri); ri->nLen = e->len; ri->nCrc = e->crc; ri->nType = e->type; }
    return 0;
}

static mz_zip_archive zips[2];
static int zip_open[2];
static int zip_find(mz_zip_archive *z, const RomEnt *e) {
    int n = (int)mz_zip_reader_get_num_files(z);
    for (int k = 0; k < n; ++k) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(z, k, &st)) continue;
        if (st.m_crc32 == e->crc && st.m_uncomp_size == e->len) return k;
    }
    return mz_zip_reader_locate_file(z, e->name, NULL, 0);
}
static int load_err;
INT32 BurnLoadRom(UINT8 *dest, INT32 i, INT32 gap) {
    const RomEnt *e = rom_ent(i);
    if (!e) { load_err = 1; return 1; }
    for (int z = 0; z < 2; ++z) {
        if (!zip_open[z]) continue;
        int idx = zip_find(&zips[z], e);
        if (idx < 0) continue;
        size_t sz = 0;
        UINT8 *d = (UINT8 *)mz_zip_reader_extract_to_heap(&zips[z], idx, &sz, 0);
        if (!d) continue;
        if (gap <= 1) memcpy(dest, d, sz < e->len ? sz : e->len);
        else for (size_t k = 0; k < sz; ++k) dest[k * gap] = d[k];
        mz_free(d);
        return 0;
    }
    fprintf(stderr, "pgm: ROM mancante %s (crc %08x)\n", e->name, e->crc);
    load_err = 1;
    return 1;
}

// ------------------------------------------------------------------ stato del core
extern INT32 nPgmOverclock;
extern UINT8 *PGMSPRMaskROM;
extern INT32 nPGMSPRMaskMaskLen, nPGMSPRColMaskLen;
static int loaded;
static INT16 audio_buf[2048 * 2];
static size_t audio_frames;
static unsigned in_joy[2], in_sys[2];
static unsigned dip_a = 0x08;      // free play
static int region_val = 6;
static int martmast_init() {
    pPgmInitCallback = pgm_decrypt_martmast;
    pPgmProtCallback = install_protection_asic27a_martmast;
    nPgmAsicRegionHackAddress = 0x37c1;
    INT32 r = pgmInit();
    Arm7SetIdleLoopAddress(0x800039e);
    return r;
}

static void cmc_4p_precalc() {     // come FBNeo burn_sound.cpp
    for (INT32 a = 0; a < 4096; a++) {
        INT32 x = a * 4, x2 = x * x / 16384, x3 = x2 * x / 16384;
        Precalc[a * 4 + 0] = (INT16)(-x / 3 + x2 / 2 - x3 / 6);
        Precalc[a * 4 + 1] = (INT16)(-x / 2 - x2 + x3 / 2 + 16384);
        Precalc[a * 4 + 2] = (INT16)(x + x2 / 2 - x3 / 2);
        Precalc[a * 4 + 3] = (INT16)(-x / 6 + x3 / 6);
    }
}

static void video_alloc();
static void video_free();

extern "C" int pgm_load_zip(const char *zip_path, const char *bios_zip, const char *game, int region) {
    if (loaded) pgm_unload();
    if (game && strcmp(game, "martmast")) return 0;
    memset(zips, 0, sizeof zips); zip_open[0] = zip_open[1] = 0;
    if (!mz_zip_reader_init_file(&zips[0], zip_path, 0)) return 0;
    zip_open[0] = 1;
    if (bios_zip && mz_zip_reader_init_file(&zips[1], bios_zip, 0)) zip_open[1] = 1;
    cmc_4p_precalc();
    BurnSetRefreshRate(PGM_FPS);
    region_val = region;
    PgmInput[6] = dip_a; PgmInput[7] = (UINT8)region; PgmInput[8] = 1;  // BIOS nuova
    load_err = 0;
    int r = martmast_init();
    for (int z = 0; z < 2; ++z) if (zip_open[z]) { mz_zip_reader_end(&zips[z]); zip_open[z] = 0; }
    if (r || load_err) { pgmExit(); return 0; }
    video_alloc();
    loaded = 1;
    return 1;
}
extern "C" void pgm_unload(void) {
    if (!loaded) return;
    pgmExit();
    video_free();
    loaded = 0;
}
extern "C" void pgm_reset(void) { if (loaded) { PgmReset = 1; pgmFrame(); PgmReset = 0; } }
extern "C" void pgm_set_input(int port, unsigned joy, unsigned sys) { if (port >= 0 && port < 2) { in_joy[port] = joy; in_sys[port] = sys; } }
extern "C" void pgm_set_dip(unsigned d) { dip_a = d & 0xff; PgmInput[6] = dip_a; }
extern "C" void pgm_set_overclock(int pct) { nPgmOverclock = pct < 50 ? 50 : pct > 400 ? 400 : pct; }

static void apply_inputs() {
    UINT8 *joy[2] = { PgmJoy1, PgmJoy2 };
    for (int p = 0; p < 2; ++p) {
        unsigned j = in_joy[p], s = in_sys[p];
        joy[p][0] = (s & PGM_START) ? 1 : 0;
        joy[p][1] = (j & PGM_UP) ? 1 : 0;
        joy[p][2] = (j & PGM_DOWN) ? 1 : 0;
        joy[p][3] = (j & PGM_LEFT) ? 1 : 0;
        joy[p][4] = (j & PGM_RIGHT) ? 1 : 0;
        joy[p][5] = (j & PGM_B1) ? 1 : 0;
        joy[p][6] = (j & PGM_B2) ? 1 : 0;
        joy[p][7] = (j & PGM_B3) ? 1 : 0;
        PgmBtn2[p] = (j & PGM_B4) ? 1 : 0;
        PgmBtn1[p] = (s & PGM_COIN) ? 1 : 0;
    }
    PgmBtn1[4] = (in_sys[0] & PGM_TEST) ? 1 : 0;      // diagnostica
    PgmBtn1[5] = (in_sys[0] & PGM_SERVICE) ? 1 : 0;
    PgmInput[6] = dip_a;
    PgmInput[7] = (UINT8)region_val;
}

static void pgm_render_hd();
static uint32_t *hd_buf; static unsigned hd_w, hd_h;

extern "C" void pgm_frame(int render) {
    if (!loaded) return;
    apply_inputs();
    pBurnSoundOut = audio_buf;
    memset(audio_buf, 0, sizeof audio_buf);
    pgmFrame();
    audio_frames = nBurnSoundLen;
    if (render && hd_buf) pgm_render_hd();
}
extern "C" const int16_t *pgm_audio(size_t *frames) { if (frames) *frames = audio_frames; return audio_buf; }
extern "C" void pgm_set_hd(uint32_t *buf, unsigned w, unsigned h) { hd_buf = buf; hd_w = w; hd_h = h; }

// ------------------------------------------------------------------ accesso alla memoria (banco di prova / app)
extern "C" uint8_t pgm_read8(uint32_t a) {
    if (!loaded) return 0;
    a &= 0xffffff;
    if (a >= 0x800000 && a < 0x900000) return PGM68KRAM[(a & 0x1ffff) ^ 1];
    if (a < 0x80000) return PGM68KBIOS[a ^ 1];
    if (a >= 0x100000 && a < 0x100000 + (UINT32)nPGM68KROMLen) return PGM68KROM[(a - 0x100000) ^ 1];
    if (a >= 0xd00000 && a < 0xd10000) return PGMARMShareRAM[(a & 0xffff) ^ 1];
    return 0;
}
extern "C" uint16_t pgm_read16(uint32_t a) { return (uint16_t)((pgm_read8(a) << 8) | pgm_read8(a + 1)); }
extern "C" void pgm_write8(uint32_t a, uint8_t v) {
    if (!loaded) return;
    a &= 0xffffff;
    if (a >= 0x800000 && a < 0x900000) PGM68KRAM[(a & 0x1ffff) ^ 1] = v;
}
extern "C" unsigned pgm_pc(void) { if (!loaded) return 0; SekOpen(0); unsigned pc = SekGetPC(-1); SekClose(); return pc; }
extern "C" int pgm_patch16(uint32_t a, uint16_t expect, uint16_t val) {
    if (!loaded || a < 0x100000 || a + 2 > 0x100000 + (UINT32)nPGM68KROMLen || (a & 1)) return 0;
    UINT16 *p = (UINT16 *)(PGM68KROM + (a - 0x100000));
    if (*p != expect && *p != val) return 0;
    *p = val;
    return 1;
}
extern "C" const uint8_t *pgm_rom68k(size_t *len) { if (len) *len = loaded ? nPGM68KROMLen : 0; return loaded ? PGM68KROM : NULL; }
extern "C" const uint16_t *pgm_spritebuf(void) { return PGMSprBuf; }
extern "C" const uint16_t *pgm_palram(void) { return PGMPalRAM; }
extern "C" uint16_t pgm_get_video_control(void) { return pgm_video_control; }

// ------------------------------------------------------------------ stato
static UINT8 *st_ptr; static size_t st_pos, st_cap; static int st_mode;
static INT32 st_acb(struct BurnArea *ba) {
    if (st_mode == 1 && st_pos + ba->nLen <= st_cap) memcpy(st_ptr + st_pos, ba->Data, ba->nLen);
    if (st_mode == 2 && st_pos + ba->nLen <= st_cap) memcpy(ba->Data, st_ptr + st_pos, ba->nLen);
    st_pos += ba->nLen;
    return 0;
}
static void st_scan() {
    BurnAcb = st_acb;
    INT32 act = ACB_MEMORY_RAM | ACB_NVRAM | ACB_DRIVER_DATA | (st_mode == 2 ? ACB_WRITE : ACB_READ);
    pgmScan(act, NULL);
    struct BurnArea ba; ba.Data = audio_buf; ba.nLen = 0; ba.nAddress = 0; ba.szName = "";
    (void)ba;
}
extern "C" size_t pgm_state_size(void) { if (!loaded) return 0; st_mode = 0; st_pos = 0; st_scan(); return st_pos; }
extern "C" int pgm_state_save(void *buf, size_t sz) {
    if (!loaded) return 0;
    st_mode = 1; st_ptr = (UINT8 *)buf; st_cap = sz; st_pos = 0; st_scan();
    return st_pos <= sz;
}
extern "C" int pgm_state_load(const void *buf, size_t sz) {
    if (!loaded) return 0;
    st_mode = 2; st_ptr = (UINT8 *)buf; st_cap = sz; st_pos = 0; st_scan();
    return st_pos <= sz;
}

// debug
static uint32_t watch_lo = 1, watch_hi = 0, watch_val = 0xffffffff;
static int watch_frame;
#include "m68k.h"
static void print_stack() {
    unsigned sp = m68k_get_reg(NULL, M68K_REG_A7);
    printf("   sp %06x:", sp);
    for (int i = 0; i < 32; ++i) printf(" %08x", (pgm_read16(sp + i * 4) << 16) | pgm_read16(sp + i * 4 + 2));
    printf("\n");
}
static void __fastcall watch_ww(UINT32 a, UINT16 v) {
    *(UINT16 *)(PGM68KRAM + (a & 0x1fffe)) = v;
    a &= 0xffffff;
    if (a >= watch_lo && a <= watch_hi && (watch_val == 0xffffffff || watch_val == v))
        { printf("W16 f%d pc %06x [%06x] = %04x\n", watch_frame, (unsigned)SekGetPC(-1), a, v); if (getenv("WSTACK")) print_stack(); }
}
static void __fastcall watch_wb(UINT32 a, UINT8 v) {
    PGM68KRAM[(a & 0x1ffff) ^ 1] = v;
    a &= 0xffffff;
    if (a >= watch_lo && a <= watch_hi && (watch_val == 0xffffffff || watch_val == v))
        printf("W8  f%d pc %06x [%06x] = %02x\n", watch_frame, (unsigned)SekGetPC(-1), a, v);
}
extern "C" void pgm_set_watch(uint32_t lo, uint32_t hi) {
    watch_lo = lo; watch_hi = hi;
    if (!loaded) return;
    SekOpen(0);
    SekMapHandler(5, 0x800000, 0x81ffff, MAP_WRITE);
    SekSetWriteWordHandler(5, watch_ww);
    SekSetWriteByteHandler(5, watch_wb);
    SekClose();
}
extern "C" void pgm_set_watch_value(uint32_t v) { watch_val = v; }
extern "C" void pgm_watch_frame(int f) { watch_frame = f; }
extern "C" void pgm_set_pchook(uint32_t, void (*)(uint32_t)) {}

#include "pgm_video.inc"

// il rendering FBNeo (pgm_draw.cpp) e' sostituito da pgm_video.inc
void pgmInitDraw() {}
void pgmExitDraw() {}
INT32 pgmDraw() { return 0; }
