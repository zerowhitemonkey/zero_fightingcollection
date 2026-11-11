// Versione distribuibile: core Sega ST-V (Mednafen, GPL-2) escluso per compatibilita' di licenza con FBNeo.
// Queste funzioni sostituiscono il core: il caricamento fallisce sempre, Golden Axe: The Duel non e' nell'elenco.
#include "stvcore.h"
int  stv_load_zip(const char *z, const char *b, int r, const char *d) { (void)z; (void)b; (void)r; (void)d; return 0; }
void stv_unload(void) {}
const char *stv_last_error(void) { return "ST-V core not included"; }
void stv_set_input(int p, unsigned j, unsigned s) { (void)p; (void)j; (void)s; }
void stv_frame(int r) { (void)r; }
void stv_set_hd(uint32_t *b, unsigned w, unsigned h) { (void)b; (void)w; (void)h; }
const uint32_t *stv_native(unsigned *w, unsigned *h, unsigned *p) { (void)w; (void)h; (void)p; return 0; }
const int16_t *stv_audio(size_t *f) { if (f) *f = 0; return 0; }
uint8_t stv_read8(uint32_t a) { (void)a; return 0; }
void stv_write8(uint32_t a, uint8_t v) { (void)a; (void)v; }
uint16_t stv_eeprom_read(unsigned w) { (void)w; return 0; }
void stv_eeprom_write(unsigned w, uint16_t v) { (void)w; (void)v; }
uint8_t *stv_bram(void) { return 0; }
uint16_t stv_cart_read16(uint32_t a) { (void)a; return 0; }
int stv_cart_patch16(uint32_t a, uint16_t e, uint16_t v) { (void)a; (void)e; (void)v; return 0; }
void stv_set_fx(int m) { (void)m; }
void stv_set_overclock(int p) { (void)p; }
