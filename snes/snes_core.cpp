// SNKHD - wrapper C del core SNES (LakeSnes/FBNeo, MIT). Sostituisce d_snes.cpp (driver FBNeo).
#include "burnint.h"
#include "snes.h"
#include "snes_core.h"
#include <miniz.h>

static Snes *sn = NULL;
static UINT8 *rom = NULL;
static UINT32 *fb = NULL;
static INT16 audio[SNES_AUDIO_PER_FRAME * 2];
static UINT8 dummy_draw;

int snescore_load_mem(const uint8_t *data, size_t len) {
    snescore_unload();
    // la copia viene arrotondata a potenza di 2 come fa il driver FBNeo
    UINT32 rl = (UINT32)len;
    rom = BurnMalloc(BurnRoundPowerOf2(rl));
    memcpy(rom, data, len);
    sn = snes_init();
    if (!snes_loadRom(sn, rom, (int)len, NULL, 0)) { snescore_unload(); return 0; }
    fb = (UINT32 *)BurnMalloc(512 * 480 * 4);
    snes_reset(sn, true);
    return 1;
}

int snescore_load_zip(const char *zip_path) {
    mz_zip_archive z; memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_file(&z, zip_path, 0)) return 0;
    int n = (int)mz_zip_reader_get_num_files(&z), ok = 0;
    for (int i = 0; i < n && !ok; i++) {
        char name[256];
        mz_zip_reader_get_filename(&z, i, name, sizeof name);
        size_t l = strlen(name);
        if (l < 4) continue;
        const char *e = name + l - 4;
        if (strcasecmp(e, ".sfc") && strcasecmp(e, ".smc") && strcasecmp(e, ".swc") && strcasecmp(e, ".fig")) continue;
        size_t sz = 0;
        void *p = mz_zip_reader_extract_to_heap(&z, i, &sz, 0);
        if (!p) continue;
        ok = snescore_load_mem((const uint8_t *)p, sz);
        mz_free(p);
    }
    mz_zip_reader_end(&z);
    return ok;
}

void snescore_unload(void) {
    if (sn) snes_free(sn);
    sn = NULL;
    BurnFree(rom); rom = NULL;
    BurnFree(fb); fb = NULL;
    pBurnDraw = NULL;
}

void snescore_reset(void) { if (sn) snes_reset(sn, true); }

void snescore_set_input(int port, unsigned b) {
    if (!sn) return;
    for (int i = 0; i < 12; i++) snes_setButtonState(sn, port + 1, i, (b >> i) & 1, DEVICE_GAMEPAD);
}

void snescore_frame(int render) {
    if (!sn) return;
    pBurnDraw = render ? &dummy_draw : NULL;
    snes_runFrame(sn);
    if (render) snes_setPixels(sn, (UINT8 *)fb, 480);
    snes_setSamples(sn, audio, SNES_AUDIO_PER_FRAME);
    nCurrentFrame++;
}

const uint32_t *snescore_video(unsigned *w, unsigned *h, unsigned *pitch) {
    if (w) *w = SNES_FB_W;
    if (h) *h = SNES_FB_H;
    if (pitch) *pitch = 512;
    return fb;
}

const int16_t *snescore_audio(size_t *frames) { if (frames) *frames = SNES_AUDIO_PER_FRAME; return audio; }
int snescore_is_pal(void) { return sn ? snes_isPal(sn) : 0; }
double snescore_fps(void) { return snescore_is_pal() ? SNES_FPS_PAL : SNES_FPS_NTSC; }
uint8_t snescore_wram8(uint32_t a) { return sn ? sn->ram[a & 0x1ffff] : 0; }
void snescore_wram_write8(uint32_t a, uint8_t v) { if (sn) sn->ram[a & 0x1ffff] = v; }

void snescore_set_debug(void (*exec)(uint32_t), void (*wr)(uint32_t, uint8_t), void (*rd)(uint32_t, uint8_t)) { snes_dbg_exec = exec; snes_dbg_write = wr; snes_dbg_read = rd; }
void snescore_regs(uint32_t *r) { cpu_dbgRegs(r); }
const uint8_t *snescore_rom(size_t *len) { if (len) *len = sn && sn->cart ? sn->cart->romSize : 0; return sn && sn->cart ? sn->cart->rom : NULL; }
size_t snescore_state_size(void) { return sn ? (size_t)snes_saveState(sn, NULL) : 0; }
int snescore_state_save(void *buf) { return sn ? snes_saveState(sn, (UINT8 *)buf) : 0; }
int snescore_state_load(const void *buf, size_t sz) { return sn ? snes_loadState(sn, (UINT8 *)buf, (INT32)sz) : 0; }

// Weapon Lord (USA, HiROM 3 MB): limita la frequenza delle parate della CPU. Le parate dell'IA (comando "guard" degli script,
// EA:DE01) riuscivano sempre: ora, per ogni attacco, un tiro casuale con probabilita' per livello (tools/wl_guard_patch.py).
// Il codice sta nel padding della ROM a 4 MB (banco F0); il punto originale EA:DE16 salta li'. Verifica dei byte originali.
static const uint8_t wl_guard_code[] = { 0xad, 0x32, 0x36, 0x38, 0xff, 0xe0, 0xff, 0x7f, 0xc9, 0x09, 0x00, 0xb0, 0x06, 0xbf, 0xe4, 0xff, 0x7f, 0xd0, 0x2b, 0xda, 0x5a, 0x22, 0x2d, 0x64, 0xea, 0x29, 0xff, 0x00, 0x8f, 0xe8, 0xff, 0x7f, 0xad, 0x20, 0x20, 0x29, 0x03, 0x00, 0x0a, 0xaa, 0xaf, 0xe8, 0xff, 0x7f, 0xdf, 0x58, 0x00, 0xf0, 0xa9, 0x02, 0x00, 0xb0, 0x03, 0xa9, 0x01, 0x00, 0x7a, 0xfa, 0x9f, 0xe4, 0xff, 0x7f, 0xad, 0x32, 0x36, 0x9f, 0xe0, 0xff, 0x7f, 0xbf, 0xe4, 0xff, 0x7f, 0xc9, 0x01, 0x00, 0xd0, 0x05, 0xa9, 0x00, 0x00, 0x38, 0x6b, 0xa9, 0x00, 0x00, 0x18, 0x6b, 0x19, 0x00, 0x37, 0x00, 0x5a, 0x00, 0x8c, 0x00 };
int snescore_patch_weaponlord(void) {
    if (!sn || !sn->cart || sn->cart->romSize != 0x400000) return 0;   // il core riempie la ROM a 4 MB: gli ultimi 1 MB (banchi F0-FF) non sono usati dal gioco
    uint8_t *r = sn->cart->rom;
    static const uint8_t orig[5] = { 0xA9, 0x00, 0x00, 0x38, 0x6B };
    if (memcmp(r + 0x2ADE16, orig, 5)) return 0;
    memset(r + 0x300000, 0, 0x100000);
    memcpy(r + 0x300000, wl_guard_code, sizeof wl_guard_code);
    static const uint8_t jml[5] = { 0x5C, 0x00, 0x00, 0xF0, 0xEA };
    memcpy(r + 0x2ADE16, jml, 5);
    return 1;
}
