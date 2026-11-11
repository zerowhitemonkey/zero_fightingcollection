// SNKHD - impostazioni (file .ini nella cartella dell'app)
#ifndef SNKHD_SETTINGS_H
#define SNKHD_SETTINGS_H
#include "input.h"

enum { VID_WINDOW = 0, VID_BORDERLESS = 1, VID_FULLSCREEN = 2 };
enum { SCR_SQUARE = 0, SCR_43 = 1, SCR_169 = 2, SCR_SQZOOM = 3, SCR_COUNT = 4 };

typedef struct {
    int video, win_w, win_h, vsync, scanlines, blur, overclock;
    int screen;         // SCR_SQUARE / SCR_43 / SCR_169 (globale, ricordato)
    int monitor;        // proporzione fisica dello schermo: MON_AUTO (pixel quadrati), MON_43, MON_54, MON_1610, MON_169
    int romcache;       // ROM CACHE: 1 = immagini dei giochi cifrati salvate in cache\ (vedi neorom.c), 0 = mai
} gsettings_t;
enum { MON_AUTO = 0, MON_43, MON_54, MON_1610, MON_169, MON_COUNT };

typedef struct {
    int difficulty;     // 1..8
    int region_jp;      // 0 = INT (USA), 1 = JAP
    int shadows;        // 0 = piene, 1 = 75%, 2 = 50% (SHADOW_*)
    int hud;            // INTERFACE: 0 DEFAULT, 1 SOLID, 2 75%, 3 50% (VIS_*)
    int hud_area;       // INTERFACE AREA: 0 BARS, 1 FULL
    int fx;             // SPECIAL EFFECTS: 0 DEFAULT, 1 SOLID, 2 75%, 3 50%
    int scanlines;      // -1 = GLOBAL (OPTIONS), altrimenti 0 OFF, 1 ON, 2 50% (ha la precedenza)
    int blur;           // -1 = GLOBAL, 0 OFF, 1 ON (320x240), 2 50% (480x360)
    int overclock;      // -1 = GLOBAL, 0 = 100%, 1 = 200%
    int maxlevel;       // giochi con livelli d'esperienza (Red Earth): 1 = personaggi al livello massimo
    int hudprio;        // HUD PRIORITY (JoJo): 0 ORIGINAL, 1 DYNAMIC, 2 DYNAMIC+
    int vsstage;        // VERSUS CPU (JoJo): stage, 0 = AUTO, altrimenti voce della lista di cps3cfg
    int screen;         // SCREEN TYPE del gioco (SCR_*), -1 = non ancora scelto (vale quello globale)
    int monitor;        // MONITOR del gioco (MON_*), -1 = non ancora scelto (vale quello di OPTIONS)
    int border;         // BORDERS: 1 = AUTO (bordi neri del gioco tolti), 0 = OFF
    int bn, bc[4][3];   // bordi appresi: campioni, e per bordo (alto, basso, sx, dx) quanti con >= 8 / 16 / 24 righe nere
    pmap_t map[2];
} game_settings_t;

void settings_load(const char *dir, gsettings_t *s);
void settings_save(const char *dir, const gsettings_t *s);
void game_settings_load(const char *dir, const char *id, game_settings_t *g);
void game_settings_save(const char *dir, const char *id, const game_settings_t *g);
#endif
