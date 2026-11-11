// SNKHD - aspetto per gioco: ombre, interfaccia, effetti speciali
#ifndef SNKHD_VISUAL_H
#define SNKHD_VISUAL_H
#include <stdio.h>
#include "emu.h"
enum { SHADOW_SOLID = 0, SHADOW_75 = 1, SHADOW_50 = 2, SHADOW_COUNT = 3 };   // selettore SHADOWS
enum { VIS_DEFAULT = 0, VIS_SOLID = 1, VIS_75 = 2, VIS_50 = 3, VIS_COUNT = 4 }; // INTERFACE / SPECIAL EFFECTS
enum { AREA_BARS = 0, AREA_FULL = 1, AREA_COUNT = 2 };                         // INTERFACE AREA
const char *shadow_table(const char *id);   // NULL = nessuna tabella ombre
int shadow_apply(const char *id, int mode);
int visual_has_shadows(const char *id);
int visual_has_hud(const char *id);
int visual_has_fx(const char *id);
void visual_apply(const char *id, int shadows, int hud, int area, int fx); // dopo emu_load + rompatch_apply
void visual_frame(const char *id);                                         // dopo ogni frame
#endif
