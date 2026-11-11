// SNKHD - impostazioni dei giochi IGS PGM (Martial Masters): regione, patch ROM, difficolta', stato dei giocatori
#ifndef SNKHD_PGMCFG_H
#define SNKHD_PGMCFG_H
int  pgmcfg_region(const char *id, int region_jp);              // valore del DIP regione per emu_load_pgm
void pgmcfg_apply(const char *id, int difficulty);              // dopo emu_load_pgm, prima del primo frame
void pgmcfg_frame(const char *id);                              // dopo ogni frame: impostazioni del gioco nella RAM
int  pgmcfg_player_in(const char *id, int p);                   // 1 = giocatore p in partita
int  pgmcfg_has_region(const char *id);                         // 0 = voce REGION nascosta
#endif
