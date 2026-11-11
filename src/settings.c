// SNKHD - impostazioni
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"

static int ini_get(const char *path, const char *key, int def) {
    FILE *f = fopen(path, "r");
    if (!f) return def;
    char l[256];
    size_t kl = strlen(key);
    int v = def;
    while (fgets(l, sizeof l, f)) {
        if (!strncmp(l, key, kl) && l[kl] == '=') { v = atoi(l + kl + 1); break; }
    }
    fclose(f);
    return v;
}

void settings_load(const char *dir, gsettings_t *s) {
    char p[1024];
    snprintf(p, sizeof p, "%s/settings.ini", dir);
    s->video = ini_get(p, "video", VID_FULLSCREEN);
    s->win_w = ini_get(p, "window_width", 1280);
    s->win_h = ini_get(p, "window_height", 720);
    s->vsync = ini_get(p, "vsync", 1);
    s->scanlines = ini_get(p, "scanlines", 1);
    s->blur = ini_get(p, "blur", 1);
    s->overclock = ini_get(p, "overclock", 1);
    if (s->scanlines < 0 || s->scanlines > 2) s->scanlines = 1;   // 0 OFF, 1 ON, 2 50%
    if (s->blur < 0 || s->blur > 2) s->blur = 1;                  // 0 OFF, 1 ON (320x240), 2 50% (480x360)
    s->screen = ini_get(p, "screen", SCR_SQUARE);
    if (s->screen < 0 || s->screen >= SCR_COUNT) s->screen = SCR_SQUARE;
    s->monitor = ini_get(p, "monitor", MON_AUTO);
    if (s->monitor < 0 || s->monitor >= MON_COUNT) s->monitor = MON_AUTO;
    s->romcache = ini_get(p, "romcache", 1) ? 1 : 0;
    if (s->video != VID_WINDOW) s->video = VID_FULLSCREEN;   // borderless eliminato
}

void settings_save(const char *dir, const gsettings_t *s) {
    char p[1024];
    snprintf(p, sizeof p, "%s/settings.ini", dir);
    FILE *f = fopen(p, "w");
    if (!f) return;
    fprintf(f, "video=%d\nwindow_width=%d\nwindow_height=%d\nvsync=%d\nscanlines=%d\nblur=%d\noverclock=%d\nscreen=%d\nmonitor=%d\nromcache=%d\n",
        s->video, s->win_w, s->win_h, s->vsync, s->scanlines, s->blur, s->overclock, s->screen, s->monitor, s->romcache);
    fclose(f);
}

void game_settings_load(const char *dir, const char *id, game_settings_t *g) {
    char p[1024], k[64];
    snprintf(p, sizeof p, "%s/games/%s.ini", dir, id);
    g->difficulty = ini_get(p, "difficulty", 4);
    if (g->difficulty < 1 || g->difficulty > 8) g->difficulty = 4;
    g->region_jp = ini_get(p, "region_jp", 0) ? 1 : 0;
    g->shadows = ini_get(p, "shadows", 2);
    if (g->shadows < 0 || g->shadows > 2) g->shadows = 2;
    g->hud = ini_get(p, "hud", 0); if (g->hud < 0 || g->hud > 3) g->hud = 0;
    g->hud_area = ini_get(p, "hud_area", 0) ? 1 : 0;
    g->fx = ini_get(p, "fx", 0); if (g->fx < 0 || g->fx > 3) g->fx = 0;
    g->scanlines = ini_get(p, "scanlines", -1); if (g->scanlines < -1 || g->scanlines > 2) g->scanlines = -1;
    g->blur = ini_get(p, "blur", -1); if (g->blur < -1 || g->blur > 2) g->blur = -1;
    g->overclock = ini_get(p, "overclock", -1); if (g->overclock < -1 || g->overclock > 1) g->overclock = -1;
    g->maxlevel = ini_get(p, "maxlevel", 1) ? 1 : 0;
    g->hudprio = ini_get(p, "hudprio", 1); if (g->hudprio < 0 || g->hudprio > 2) g->hudprio = 1;
    g->vsstage = ini_get(p, "vsstage", 0); if (g->vsstage < 0 || g->vsstage > 255) g->vsstage = 0;
    g->border = ini_get(p, "border", 1) ? 1 : 0;
    g->screen = ini_get(p, "screen", -1); if (g->screen < -1 || g->screen >= SCR_COUNT) g->screen = -1;
    g->monitor = ini_get(p, "monitor", -1); if (g->monitor < -1 || g->monitor >= MON_COUNT) g->monitor = -1;
    g->bn = ini_get(p, "brd_n", 0); if (g->bn < 0 || g->bn > 100000) g->bn = 0;
    for (int e = 0; e < 4; ++e)
        for (int k = 0; k < 3; ++k) {
            char kk[32]; snprintf(kk, sizeof kk, "brd_%d_%d", e, k);
            g->bc[e][k] = ini_get(p, kk, 0);
            if (g->bc[e][k] < 0 || g->bc[e][k] > g->bn) g->bc[e][k] = 0;
        }
    for (int pl = 0; pl < 2; ++pl) {
        input_default_map(pl, &g->map[pl]);
        for (int a = 0; a < NACT; ++a) {
            snprintf(k, sizeof k, "p%d_pad_%d", pl + 1, a);
            g->map[pl].pad[a] = ini_get(p, k, g->map[pl].pad[a]);
            snprintf(k, sizeof k, "p%d_key_%d", pl + 1, a);
            g->map[pl].key[a] = ini_get(p, k, g->map[pl].key[a]);
        }
    }
}

void game_settings_save(const char *dir, const char *id, const game_settings_t *g) {
    char p[1024];
    snprintf(p, sizeof p, "%s/games/%s.ini", dir, id);
    FILE *f = fopen(p, "w");
    if (!f) return;
    fprintf(f, "difficulty=%d\nregion_jp=%d\nshadows=%d\nhud=%d\nhud_area=%d\nfx=%d\nscanlines=%d\nblur=%d\noverclock=%d\nmaxlevel=%d\nhudprio=%d\nvsstage=%d\nborder=%d\nscreen=%d\nmonitor=%d\nbrd_n=%d\n",
        g->difficulty, g->region_jp, g->shadows, g->hud, g->hud_area, g->fx, g->scanlines, g->blur, g->overclock, g->maxlevel, g->hudprio, g->vsstage, g->border, g->screen, g->monitor, g->bn);
    for (int e = 0; e < 4; ++e)
        for (int k = 0; k < 3; ++k) fprintf(f, "brd_%d_%d=%d\n", e, k, g->bc[e][k]);
    for (int pl = 0; pl < 2; ++pl)
        for (int a = 0; a < NACT; ++a)
            fprintf(f, "p%d_pad_%d=%d\np%d_key_%d=%d\n", pl + 1, a, g->map[pl].pad[a], pl + 1, a, g->map[pl].key[a]);
    fclose(f);
}
