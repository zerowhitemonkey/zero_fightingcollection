// SNKHD - impostazioni dei giochi Psikyo PS3-V1 (Daraku Tenshi): regione, EEPROM, patch ROM, stato dei giocatori
#ifndef SNKHD_PSIKYOCFG_H
#define SNKHD_PSIKYOCFG_H
int  psikyocfg_region(const char *id, int region_jp);              // valore per emu_load_psikyo (0 Giappone, 1 World)
void psikyocfg_apply(const char *id, int difficulty);              // dopo emu_load_psikyo, prima del primo frame
int  psikyocfg_player_in(const char *id, int p);                   // 1 = giocatore p in partita
#endif
