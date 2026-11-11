// SNKHD - wrapper del core Geolith
#ifndef SNKHD_EMU_H
#define SNKHD_EMU_H
#include <stdint.h>
#include <stddef.h>

#define EMU_W 320
#define EMU_H 224
#define EMU_FPS 59.185606

// Bit ingressi joystick (1 = premuto). CPS3: A-F = LP MP HP LK MK HK
enum { IN_UP = 1, IN_DOWN = 2, IN_LEFT = 4, IN_RIGHT = 8, IN_A = 16, IN_B = 32, IN_C = 64, IN_D = 128, IN_E = 256, IN_F = 512 };
// sistema caricato
enum { SYS_NEOGEO = 0, SYS_CPS3 = 1, SYS_SNES = 2, SYS_PSIKYO = 3, SYS_PGM = 4, SYS_STV = 5 };
// Bit di sistema (1 = premuto)
enum { SYS_START = 1, SYS_SELECT = 2, SYS_COIN = 4 };

int  emu_load_image(void *img, size_t sz, const char *bios_zip, int region_jp, const char *nvram_path);   // immagine .neo in memoria (neorom), proprieta' dell'emulatore
int  emu_load(const char *neo_path, const char *bios_zip, int region_jp, const char *nvram_path);
int  emu_load_cps3(const char *zip_path, const char *game, int region);   // romset FBNeo (zip), regione = byte BIOS
int  emu_load_snes(const char *rom_zip);   // Super Nintendo (zip con .sfc), core LakeSnes/FBNeo
int  emu_load_psikyo(const char *zip_path, const char *game, int region);   // Psikyo PS3-V1 (romset FBNeo/MAME), regione 0 JP / 1 World
int  emu_load_pgm(const char *zip_path, const char *bios_zip, const char *game, int region);   // IGS PGM (romset MAME/FBNeo + pgm.zip)
int  emu_load_stv(const char *zip_path, const char *bios_zip, int region, const char *basedir);   // Sega ST-V (romset MAME + stvbios.zip, core Mednafen), regione 0 JP / 1 USA / 2 EU
const char *emu_stv_error(void);          // ultimo errore del core ST-V
int  emu_system(void);                    // SYS_NEOGEO / SYS_CPS3 / SYS_SNES / SYS_PSIKYO
void emu_prepare(int sys);                // sistema del prossimo gioco, da chiamare prima di emu_set_hd (nessun gioco caricato)
unsigned emu_native_w(void);              // 320 (Neo Geo, Psikyo) / 384 (CPS3) / 448 (PGM) / 256 (SNES, buffer 512x448)
unsigned emu_native_h(void);              // 224
double emu_fps(void);                     // 59,1856 (Neo Geo) / 59,5995 (CPS3) / 60 (Psikyo)
uint8_t emu_read8(uint32_t addr);         // Neo Geo: RAM $10xxxx (offset 0-FFFF); CPS3 / Psikyo: indirizzo SH-2; SNES: WRAM 0-1FFFF
void emu_write8(uint32_t addr, uint8_t v); // SNES: WRAM; Psikyo: indirizzo SH-2
void emu_unload(void);
void emu_set_hd(unsigned w, unsigned h);  // dimensione dell'immagine HD (320x224 -> w x h)
unsigned emu_hd_w(void);
unsigned emu_hd_h(void);
void emu_set_softdip(int idx, int val);   // idx 0..15 = $10FD84+idx ; val<0 = nessun override
void emu_set_input(int port, unsigned joy, unsigned sys);
void emu_set_overclock(int on);
void emu_frame(void);
void emu_frame_fast(void);              // senza rendering (avvio veloce)
// immagine visibile HD (w x h), pitch in pixel
const uint32_t *emu_video(unsigned *w, unsigned *h, unsigned *pitch);
const uint32_t *emu_video_native(unsigned *pitch);
// audio grezzo dell'ultimo frame (stereo int16, ~56319 Hz)
const int16_t *emu_audio(size_t *frames);
double emu_audio_rate(void);
uint8_t *emu_ram(void);                   // 64K RAM principale ($100000), ordine byte 68000
const uint8_t *emu_prom(size_t *sz);      // P ROM (big-endian) dopo il caricamento
int emu_save_nvram(const char *path);
void emu_stv_fx(int mode);                         // ST-V: SPECIAL EFFECTS 0 DEFAULT, 1 SOLID, 2 75%, 3 50%
int emu_load_blend(const char *path, int w1, int w2); // SNKHD: trasparenze .bld (FBNeo), -1 = assente
int emu_load_blend_text(const char *txt, int w1, int w2); // idem da testo (NULL = nessuna trasparenza)
enum { EMU_BLEND_SHADOWS = 0, EMU_BLEND_EFFECTS, EMU_BLEND_BARS, EMU_BLEND_TEXT, EMU_BLEND_EXT, EMU_BLEND_NCAT };
int emu_blend_set(int cat, const char *txt, int weight, int gated); // peso 0 = categoria spenta
void emu_blend_gate(int on);                                         // 1 = incontro in corso (categorie gated attive)
int emu_blend_tile_present(unsigned lo, unsigned hi);                // una sprite visibile usa tile in [lo,hi]
int emu_blend_fix_present(unsigned lo, unsigned hi);                 // il fix layer usa tile in [lo,hi] (CPS3: strato testo)
int emu_blend_fixpal_present(unsigned pal);                          // il fix layer / strato testo usa la palette
#endif
