// SNKHD - impostazioni dei giochi Sega ST-V (Golden Axe: The Duel): regione, EEPROM, stato dei giocatori
#ifndef SNKHD_STVCFG_H
#define SNKHD_STVCFG_H
int  stvcfg_region(const char *id, int region_jp);              // 0 JP / 1 USA per emu_load_stv
void stvcfg_apply(const char *id, int difficulty);              // dopo emu_load_stv, prima del primo frame
int  stvcfg_unlock(const char *id);                             // G. AXE selezionabile (patch del programma), 1 = ok
void stvcfg_frame(const char *id);                              // dopo ogni frame
int  stvcfg_player_in(const char *id, int p);                   // 1 = giocatore p in partita (avvio veloce)
int  stvcfg_versus_pulse(const char *id);                       // 1 = in VERSUS START a impulsi per entrambi (rientro)
#endif
