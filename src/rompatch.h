// SNKHD - patch applicate alla P ROM al caricamento (per gioco)
#ifndef SNKHD_ROMPATCH_H
#define SNKHD_ROMPATCH_H
// ritorna il numero di gruppi di patch applicati (-1 = ROM diversa dall'attesa, nulla applicato)
int rompatch_apply(const char *game_id);
// gruppi opzionali: RP_HUD_BARS/RP_HUD_TEXT = INTERFACE SOLID (barre / scritte), RP_FX = SPECIAL EFFECTS senza lampeggio
enum { RP_HUD_BARS = 1, RP_HUD_TEXT = 2, RP_FX = 4, RP_HUD_ANIM = 8 };   // ANIM: animazioni dell'HUD fermate solo con SOLID
int rompatch_options(const char *game_id, unsigned flags); // applica i gruppi con flag, annulla gli altri
int rompatch_has_options(const char *game_id, unsigned flag);
#endif
