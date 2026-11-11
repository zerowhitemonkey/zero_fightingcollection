// API del core Sega ST-V. Nella versione distribuibile il core (Mednafen, GPL-2) non e' incluso: vedi src/stv_stub.c
#ifndef SNKHD_STVCORE_H
#define SNKHD_STVCORE_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define STV_W 352
#define STV_H 224
#define STV_FPS 59.883028
#define STV_AUDIO_RATE 48000

// bit ingressi (1 = premuto): direzioni + 6 tasti del pannello ST-V (SW1-SW3 = B1-B3, SW4-SW6 = B4-B6)
enum { STV_UP = 1, STV_DOWN = 2, STV_LEFT = 4, STV_RIGHT = 8, STV_B1 = 16, STV_B2 = 32, STV_B3 = 64, STV_B4 = 128, STV_B5 = 256, STV_B6 = 512 };
enum { STV_START = 1, STV_COIN = 2, STV_SERVICE = 4, STV_TEST = 8 };
enum { STV_REGION_JP = 0, STV_REGION_NA = 1, STV_REGION_EU = 2 };

// zip del gioco (set MAME, letto direttamente da Mednafen) + zip della BIOS (stvbios.zip; la BIOS si cerca prima nello
// zip del gioco). basedir = cartella di lavoro di Mednafen (firmware estratto, EEPROM salvata). 1 = ok
int  stv_load_zip(const char *zip_path, const char *bios_zip, int region, const char *basedir);
void stv_unload(void);
const char *stv_last_error(void);
void stv_set_input(int port, unsigned joy, unsigned sys);
void stv_frame(int render);                       // un fotogramma (render 0 = avvio veloce)
// uscita: buffer w x h (multiplo intero per asse di 352x240), formato 0xAARRGGBB
void stv_set_hd(uint32_t *buf, unsigned w, unsigned h);
const uint32_t *stv_native(unsigned *w, unsigned *h, unsigned *pitch); // ultimo fotogramma nativo (banco di prova)
const int16_t *stv_audio(size_t *frames);         // stereo int16 dell'ultimo fotogramma (48000 Hz)

uint8_t  stv_read8(uint32_t addr);                // work RAM: 0x06000000-0x060FFFFF (H), 0x00200000-0x002FFFFF (L)
void     stv_write8(uint32_t addr, uint8_t v);
uint16_t stv_eeprom_read(unsigned word);          // EEPROM 93C46: 64 parole
void     stv_eeprom_write(unsigned word, uint16_t v);
uint8_t *stv_bram(void);                         // backup RAM 32 KB (byte dispari dello spazio 0x00180000)
uint16_t stv_cart_read16(uint32_t addr);         // ROM della cartuccia 0x02000000-0x04FFFFFF
int      stv_cart_patch16(uint32_t addr, uint16_t expect, uint16_t val); // 1 = applicata (verifica dei byte originali)
void     stv_set_fx(int mode);                   // SPECIAL EFFECTS: 0 DEFAULT, 1 SOLID, 2 75%, 3 50%
void     stv_set_overclock(int percent);          // riservato (Mednafen non ha l'overclock dell'SH-2)

// debug: write watch sulla work RAM [lo,hi] (val = 0xFFFFFFFF qualsiasi), stampa su stdout con PC/PR dei due SH-2
void stv_set_watch(uint32_t lo, uint32_t hi, uint32_t val, unsigned max);
void stv_watch_frame(int f);
uint32_t stv_debug_pc(int cpu, int pr);            // debug: PC (pr 0) o PR (pr 1) dell'SH-2 master (0) / slave (1)
void stv_hd_stats(void);                         // debug: tempo totale del passo HD del VDP1 (stderr)
const uint16_t *stv_vdp2_regs(void);             // debug: registri VDP2 scritti (0x100 parole, indice = offset/2)
void stv_set_vdp1_cb(void (*cb)(const uint16_t *cmd, uint32_t addr));   // debug: ogni comando VDP1 eseguito

// stato (banco di prova / avvio veloce)
int  stv_state_save(void **buf, size_t *sz);      // buffer allocato con malloc
int  stv_state_load(const void *buf, size_t sz);

#ifdef __cplusplus
}
#endif
#endif
