// SNKHD - impostazioni dei giochi CPS3
#ifndef SNKHD_CPS3CFG_H
#define SNKHD_CPS3CFG_H
int  cps3cfg_region(const char *id, int region_jp);           // byte regione della BIOS
void cps3cfg_apply(const char *id, int difficulty, int region_jp); // EEPROM: free play, difficolta' 1-8 (dopo il caricamento)
int  cps3cfg_player_in(const char *id, int p);                 // 1 = giocatore p in partita
int  cps3cfg_layout(const char *id);                           // disposizione dei tasti: 1 = 6 tasti, 2 = JoJo (A B C S)
int  cps3cfg_has_maxlevel(const char *id);                     // il gioco ha livelli d'esperienza (Red Earth)
void cps3cfg_frame(const char *id, int maxlevel);              // dopo ogni frame: livello massimo ai personaggi in gioco
int  cps3cfg_has_hudprio(const char *id);                      // il gioco ha HUD PRIORITY (JoJo)
void cps3cfg_set_hudprio(const char *id, int mode);            // 0 ORIGINAL, 1 DYNAMIC, 2 DYNAMIC+ (0 per gli altri giochi)
int  cps3cfg_has_vscpu(const char *id);                        // il gioco ha la modalita' VERSUS CPU (JoJo)
void cps3cfg_vscpu_input(const char *id, unsigned *j0, unsigned *j1); // VERSUS CPU: comandi P1/P2 e P2 alla CPU (ogni frame)
int  cps3cfg_vs_stage_count(const char *id);                   // voci dello stage di VERSUS CPU (0 = nessuna; voce 0 = AUTO)
const char *cps3cfg_vs_stage_name(const char *id, int idx);
void cps3cfg_vscpu_setup(const char *id, int enabled, int stage_idx); // dopo il caricamento: patch dello stage scelto
const char *cps3cfg_vscpu_hint(const char *id);                // VERSUS CPU: messaggio da mostrare (o NULL)
#endif
