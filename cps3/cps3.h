// SNKHD - core CPS3 (driver FBNeo adattato): API C
#ifndef SNKHD_CPS3_H
#define SNKHD_CPS3_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define CPS3_W 384
#define CPS3_H 224
#define CPS3_FPS 59.59949
#define CPS3_AUDIO_RATE (42954500.0 / 3.0 / 384.0)   // 37286,89 Hz

// bit ingressi (1 = premuto): direzioni + 6 tasti (LP MP HP LK MK HK)
enum { C3_UP = 1, C3_DOWN = 2, C3_LEFT = 4, C3_RIGHT = 8, C3_B1 = 16, C3_B2 = 32, C3_B3 = 64,
       C3_B4 = 128, C3_B5 = 256, C3_B6 = 512 };
enum { C3_START = 1, C3_COIN = 4, C3_SERVICE = 8, C3_TEST = 16 };

// game: "redearth" / "jojoba"; region: valore del byte regione (come il DIP "Region" di FBNeo)
int  cps3_load_zip(const char *zip_path, const char *game, int region);
void cps3_unload(void);
void cps3_reset(void);
void cps3_set_input(int port, unsigned joy, unsigned sys);
void cps3_set_overclock(int percent);          // 100 = originale, 200 = SH-2 al doppio
void cps3_frame(int render);                   // un fotogramma (render 0 = avvio veloce)

// uscita HD: buffer w x h (multiplo intero per asse di 384x224), formato 0xAARRGGBB
void cps3_set_hd(uint32_t *buf, unsigned w, unsigned h);
// riferimento: renderer FBNeo originale a 384x224 (banco di prova)
void cps3_render_reference(uint32_t *out384x224);

const int16_t *cps3_audio(size_t *frames);     // stereo int16 dell'ultimo fotogramma, CPS3_AUDIO_RATE

uint8_t  cps3_read8(uint32_t addr);            // lettura SH-2 (RAM 0x02000000-0x0207ffff)
void     cps3_write8(uint32_t addr, uint8_t v);
uint16_t *cps3_eeprom(void);                   // 0x40 parole (ordine di gioco: indice ^ 1 gia' risolto da cps3_eeprom_rd/wr)
uint16_t cps3_eeprom_rd(int word);
void     cps3_eeprom_wr(int word, uint16_t v);
unsigned cps3_pc(void);
unsigned cps3_fszoom(void);                    // zoom a schermo intero (0x40 = 1:1)
uint32_t cps3_vreg(int i);                     // registri video bufferizzati (RamVRegBuf)
const uint32_t *cps3_spritelist(void);         // lista sprite bufferizzata (SprList)
const uint32_t *cps3_palette32(void);          // tavolozza convertita (0x20000 voci)
const uint32_t *cps3_ssram(void);              // RAM dello strato testo (64x32 celle per banco)
unsigned cps3_ss_bank(void);                   // indice della prima cella del banco visibile
unsigned cps3_ss_palbase(void);

// stato (banco di prova)
size_t cps3_state_size(void);
int  cps3_state_save(void *buf, size_t sz);
int  cps3_state_load(const void *buf, size_t sz);

// SNKHD: trasparenza dello strato testo (interfaccia): opacita' 0..255 per palette (256) e per tile (512); NULL = nessuna
void cps3_set_text_blend(const uint8_t *palw256, const uint8_t *tilew512);
void cps3_set_hud_dynamic(int mode);   // HUD PRIORITY (JoJo): 0 ORIGINAL, 1 DYNAMIC (HUD alto sotto i personaggi), 2 DYNAMIC+ (anche in basso)
int  cps3_patch16(uint32_t addr, uint16_t expect, uint16_t val); // patch codice SH-2 (fetch); 0 se i byte non corrispondono
void cps3_set_watch(uint32_t lo, uint32_t hi);  // stampa le scritture SH-2 in [lo,hi] (debug)

#ifdef __cplusplus
}
#endif
#endif
