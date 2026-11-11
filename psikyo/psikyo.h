// SNKHD - core Psikyo PS3-V1 (driver FBNeo "psikyosh" adattato): API C
#ifndef SNKHD_PSIKYO_H
#define SNKHD_PSIKYO_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define PSIKYO_W 320
#define PSIKYO_H 224
#define PSIKYO_FPS 60.0
#define PSIKYO_CPU_RATE 28636350.0
#define PSIKYO_AUDIO_RATE (28636350.0 / 768.0)   // YMF278B: 37286,89 Hz

// bit ingressi (1 = premuto): direzioni + 4 tasti
enum { PSK_UP = 1, PSK_DOWN = 2, PSK_LEFT = 4, PSK_RIGHT = 8, PSK_B1 = 16, PSK_B2 = 32, PSK_B3 = 64, PSK_B4 = 128 };
enum { PSK_START = 1, PSK_COIN = 4, PSK_SERVICE = 8, PSK_TEST = 16 };

// game: "daraku"; region: 0 = Japan, 1 = World (bit 0 del DIP B)
int  psikyo_load_zip(const char *zip_path, const char *game, int region);
void psikyo_unload(void);
void psikyo_reset(void);
void psikyo_set_input(int port, unsigned joy, unsigned sys);
void psikyo_set_overclock(int percent);          // 100 = originale, 200 = SH-2 al doppio
void psikyo_set_threads(int n);                  // thread del renderer HD (fasce): 0 = automatico (CPU, max 4)
void psikyo_set_idle_skip(int on);               // speed hack del ciclo di attesa (1 = attivo, predefinito)
void psikyo_frame(int render);                   // un fotogramma (render 0 = avvio veloce)

// uscita HD: buffer w x h (multiplo intero per asse di 320x224), formato 0xAARRGGBB
void psikyo_set_hd(uint32_t *buf, unsigned w, unsigned h);
// riferimento: renderer FBNeo originale a 320x224 (banco di prova)
void psikyo_render_reference(uint32_t *out320x224);

const int16_t *psikyo_audio(size_t *frames);     // stereo int16 dell'ultimo fotogramma

uint8_t  psikyo_read8(uint32_t addr);             // lettura SH-2 (RAM 0x06000000-0x060fffff)
void     psikyo_write8(uint32_t addr, uint8_t v);
unsigned psikyo_pc(void);
uint32_t psikyo_vidreg(int i);                    // registri video (pPsikyoshVidRegs, 0x80 byte = 0x20 long)
const uint32_t *psikyo_spritebuf(void);           // sprite RAM bufferizzata (0x4000 long: 0x0000 sprite, 0x3800 lista)
const uint32_t *psikyo_palette32(void);           // tavolozza convertita (0x1400 voci)
const uint32_t *psikyo_sprram(void);              // sprite/tilemap RAM corrente (0x4000 long, 32 banchi da 0x200)
void psikyo_set_region(int region);               // 0 = Japan, 1 = World (letto dal gioco al boot)

// SNKHD: ombre (palette 0xFE) 0 = SOLID, 1 = 75%, 2 = 50%
void psikyo_set_shadows(int mode);

// stato (banco di prova)
size_t psikyo_state_size(void);
int  psikyo_state_save(void *buf, size_t sz);
int  psikyo_state_load(const void *buf, size_t sz);

// EEPROM seriale 93C56 (256 byte)
uint16_t psikyo_eeprom_rd(int word);
void     psikyo_eeprom_wr(int word, uint16_t v);
int      psikyo_patch16(uint32_t addr, uint16_t expect, uint16_t val); // patch codice SH-2 (fetch)
void     psikyo_set_watch(uint32_t lo, uint32_t hi);  // stampa le scritture SH-2 in [lo,hi] (debug)

#ifdef __cplusplus
}
#endif
#endif
