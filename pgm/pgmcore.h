// SNKHD - core IGS PGM (driver FBNeo "pgm" adattato, Martial Masters): API C
#ifndef SNKHD_PGMCORE_H
#define SNKHD_PGMCORE_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define PGM_W 448
#define PGM_H 224
#define PGM_FPS 59.185606
#define PGM_AUDIO_RATE 48000

// bit ingressi (1 = premuto): direzioni + 4 tasti
enum { PGM_UP = 1, PGM_DOWN = 2, PGM_LEFT = 4, PGM_RIGHT = 8, PGM_B1 = 16, PGM_B2 = 32, PGM_B3 = 64, PGM_B4 = 128 };
enum { PGM_START = 1, PGM_COIN = 2, PGM_SERVICE = 4, PGM_TEST = 8 };

// zip del gioco (set MAME/FBNeo, anche "merged" con la BIOS) + zip della BIOS (pgm.zip, puo' essere NULL)
// game: "martmast"; region: valore del DIP regione (0 China, 1 Taiwan, 2 Japan, 3 Korea, 4 Hong Kong, 5 World, 6 USA)
int  pgm_load_zip(const char *zip_path, const char *bios_zip, const char *game, int region);
void pgm_unload(void);
void pgm_reset(void);
void pgm_set_input(int port, unsigned joy, unsigned sys);
void pgm_set_dip(unsigned dip_a);                 // DIP A: 1 test, 2 music off, 4 voice off, 8 free play, 16 stop
void pgm_set_overclock(int percent);              // 100 = originale, 200 = 68000 + ARM7 al doppio
void pgm_frame(int render);                       // un fotogramma (render 0 = avvio veloce)

// uscita HD: buffer w x h (multiplo intero per asse di 448x224), formato 0xAARRGGBB
void pgm_set_hd(uint32_t *buf, unsigned w, unsigned h);
// riferimento: renderer FBNeo originale a 448x224 (banco di prova)
void pgm_render_reference(uint32_t *out448x224);

const int16_t *pgm_audio(size_t *frames);         // stereo int16 dell'ultimo fotogramma (48000 Hz)

uint8_t  pgm_read8(uint32_t addr);                // lettura nello spazio del 68000
void     pgm_write8(uint32_t addr, uint8_t v);    // solo RAM 0x800000-0x81ffff
uint16_t pgm_read16(uint32_t addr);
unsigned pgm_pc(void);
const uint8_t *pgm_rom68k(size_t *len);            // ROM del 68000 decifrata (parole little-endian, base 0x100000)
int      pgm_patch16(uint32_t addr, uint16_t expect, uint16_t val); // ROM del 68000 (0x100000+)
const uint16_t *pgm_spritebuf(void);              // lista sprite bufferizzata (0x800 word)
const uint16_t *pgm_palram(void);                 // palette RAM (0x1000 word)
uint16_t pgm_get_video_control(void);

// SNKHD: trasparenza ombre (0 = SOLID, 1 = 75%, 2 = 50%) e criterio: sprite con indirizzo grafico (boff)
// in [lo,hi] e altezza 'high' (-1 = qualsiasi); lo > hi = nessuna ombra
void pgm_set_shadows(int mode);
void pgm_set_shadow_sprites(uint32_t lo, uint32_t hi, int high);
// debug: callback chiamata per ogni sprite disegnata (indice, parola 0-4)
typedef void (*pgm_sprite_cb)(int idx, const uint16_t *w);
void pgm_set_sprite_cb(pgm_sprite_cb cb);
void pgm_debug_hide(uint32_t lo, uint32_t hi);     // debug: non disegna gli sprite con boff in [lo,hi]
// debug: stampa le scritture del 68000 in [lo,hi]
void pgm_set_watch(uint32_t lo, uint32_t hi);
void pgm_set_watch_value(uint32_t v);
void pgm_watch_frame(int f);
void pgm_set_pchook(uint32_t pc, void (*cb)(uint32_t pc));

// stato (banco di prova)
size_t pgm_state_size(void);
int  pgm_state_save(void *buf, size_t sz);
int  pgm_state_load(const void *buf, size_t sz);

#ifdef __cplusplus
}
#endif
#endif
