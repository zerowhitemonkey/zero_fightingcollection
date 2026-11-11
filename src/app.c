// SNKHD - FIGHTING COLLECTION (divided by ZERO): front-end
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <SDL_syswm.h>
#endif
#include "gfxb.h"
#include "gfx.h"
#include "emu.h"
#include "../snes/snes_core.h"
#include "audio.h"
#include "input.h"
#include "settings.h"
#include "softdip.h"
#include "rompatch.h"
#include "visual.h"
#include "cps3cfg.h"
#include "psikyocfg.h"
#include "pgmcfg.h"
#include "stvcfg.h"
#include "extcap.h"
#include <dirent.h>
#include <strings.h>
#include "neorom.h"
#include "patchinfo.h"
#include "artdl.h"

// ------------------------------------------------------------------ giochi
typedef struct {
    const char *id, *title;
    const char *exe;          // gioco esterno (PC): eseguibili candidati separati da '|' (il primo trovato); NULL = .neo
    int sys;                  // SYS_NEOGEO (roms/<id>.neo) / SYS_CPS3 (roms/<id>.zip, romset FBNeo) / SYS_SNES (roms/<id>.zip)
                              // SYS_PSIKYO (roms/<id>.zip, romset FBNeo/MAME) / SYS_PGM (roms/<id>.zip + roms/pgm.zip)
    const char *args;         // gioco esterno: argomenti della riga di comando ({crt} = chain bgfx scelta da SCANLINES)
    int capture;              // gioco esterno ridisegnato dall'app (cattura dello schermo): SCREEN TYPE/MONITOR/SCANLINES/BLUR
    const char *dirs;         // gioco esterno: altri nomi di cartella cercati (separati da '|'), oltre a <id> e <id>_mod
    int nat_w, nat_h;         // gioco catturato: risoluzione logica (area mostrata, proporzioni e BLUR)
    int backexit;             // gioco esterno: BACK tenuto 3 s sul pad = chiusura del gioco e ritorno all'app
    int available;
    char ext_dir[1024];       // cartella del gioco esterno trovata
    char ext_exe[128];        // eseguibile scelto
    tex_t logo, logo_jp, bg;
} game_t;

static game_t games[] = {
    { "samsho",   "SAMURAI SHODOWN" },
    { "samsho2",  "SAMURAI SHODOWN II" },
    { "samsho3",  "SAMURAI SHODOWN III" },
    { "samsho4",  "SAMURAI SHODOWN IV" },
    { "samsho5",  "SAMURAI SHODOWN V" },
    { "samsh5sp", "SAMURAI SHODOWN V SPECIAL" },
    { "savagere", "SAVAGE REIGN" },
    { "kizuna",   "KIZUNA ENCOUNTER" },
    { "lastblad", "THE LAST BLADE" },
    { "lastbld2", "THE LAST BLADE 2" },
    { "ninjamas", "NINJA MASTER'S" },
    { "aof",      "ART OF FIGHTING" },
    { "aof2",     "ART OF FIGHTING 2" },
    { "aof3",     "ART OF FIGHTING 3" },
    { "fatfury1", "FATAL FURY" },
    { "fatfursp", "FATAL FURY SPECIAL" },
    { "fatfury3", "FATAL FURY 3" },
    { "rbff1",    "REAL BOUT FATAL FURY" },
    { "rbffspec", "REAL BOUT FATAL FURY SPECIAL" },
    { "rbff2",    "REAL BOUT FATAL FURY 2" },
    { "garou",    "GAROU: MARK OF THE WOLVES" },
    // KOF in ordine dopo Garou (richiesta utente 19/10): 94-99, 2000, 2001, 2002, 2002 UM, 2003
    { "kof94",    "THE KING OF FIGHTERS '94" },
    { "kof95",    "THE KING OF FIGHTERS '95" },
    { "kof96",    "THE KING OF FIGHTERS '96" },
    { "kof97",    "THE KING OF FIGHTERS '97" },
    { "kof98",    "THE KING OF FIGHTERS '98" },
    { "kof99",    "THE KING OF FIGHTERS '99" },
    { "kof2000",  "THE KING OF FIGHTERS 2000" },
    { "kof2001",  "THE KING OF FIGHTERS 2001" },
    { "kof2002",  "THE KING OF FIGHTERS 2002" },
    { "kof2003",  "THE KING OF FIGHTERS 2003" },
    { "wakuwak7", "WAKU WAKU 7" },
    { "galaxyfg", "GALAXY FIGHT" },
    { "rotd",     "RAGE OF THE DRAGONS" },
    // Psikyo PS3-V1 (core SH-2 + YMF278B), dopo Galaxy Fight (richiesta utente)
    { "daraku",   "DARAKU TENSHI: THE FALLEN ANGELS", NULL, SYS_PSIKYO },
    // IGS PGM (core FBNeo: 68000 + ARM7 ASIC27A + Z80/ICS2115): roms/martmast.zip + roms/pgm.zip (BIOS)
    { "martmast", "MARTIAL MASTERS", NULL, SYS_PGM },
    // Capcom CPS3 (core FBNeo): riga a parte in fondo (JoJo prima di Red Earth, richiesta utente 25/10)
    { "jojoba",   "JOJO'S BIZARRE ADVENTURE", NULL, SYS_CPS3 },
    { "redearth", "RED EARTH", NULL, SYS_CPS3 },
    // Super Nintendo (core LakeSnes/FBNeo): roms/<id>.zip con il .sfc
    { "weaponlord", "WEAPONLORD", NULL, SYS_SNES },
};
#define NGAMES ((int)(sizeof games / sizeof games[0]))
static int game_sys(int gi);
static void load_assets(void);
static void border_apply(void);
static void save_game_settings(void);
static int glist[NGAMES], nglist;     // indici dei giochi presenti

// ------------------------------------------------------------------ stato
enum { S_SPLASH, S_MAIN, S_QUIT, S_OPTIONS, S_GAMES, S_MODE, S_GSET, S_INPUT, S_LOADING, S_GAME, S_PAUSE, S_VIDCONF, S_EXTERNAL, S_ROMCHECK };
static int state = S_SPLASH, next_state = -1;
static float fade = 1.0f;          // 1 = nero
static int fading_out = 0;
static Uint32 state_time;
static int ignore_input;           // ignora gli ingressi per un ciclo dopo un cambio di stato

static char basedir[1024], assetdir[1100];
static SDL_Window *win;
static gsettings_t gs;
static tex_t t_title;

static int sel_vidconf;
static int sel_main, sel_quit, sel_opt, sel_game, sel_mode, sel_gset, sel_input, sel_pause;
static int input_player, input_waiting, input_from_pause;
static Uint32 input_wait_start;
static int cur_game = -1;          // indice in games[]
static game_settings_t gset;
static int play_versus;
static int boot_frames, booted;
static unsigned frame_count;
static double emu_acc;
static Uint64 last_counter;
static float bg_alpha[NGAMES];

// ------------------------------------------------------------------ colori
#define C_WHITE  0xffffffffu
#define C_DIM    0xa8a8b0ffu
#define C_GREY   0x606068ffu
#define C_GOLD   0x779812FFu   // accento FIGHTING COLLECTION
#define C_PANEL  0x0b0b10d8u
#define C_TILE   0x15151cf0u

// diagnostica: snkhd.log nella cartella dell'app
static FILE *dlog;
#include <stdarg.h>
static void dlogf(const char *fmt, ...) {
    if (!dlog) return;
    va_list ap; va_start(ap, fmt);
    fprintf(dlog, "[%7.2f] ", SDL_GetTicks() / 1000.0);
    vfprintf(dlog, fmt, ap); va_end(ap);
    fputc('\n', dlog); fflush(dlog);
}
static const char *state_name(int s) {
    static const char *n[] = { "SPLASH","MAIN","QUIT","OPTIONS","GAMES","MODE","GSET","INPUT","LOADING","GAME","PAUSE","VIDCONF","EXTERNAL","ROMCHECK" };
    return (s >= 0 && s < (int)(sizeof n / sizeof n[0])) ? n[s] : "?";
}

static void go(int s) {
    if (next_state >= 0) return;
    next_state = s;
    fading_out = 1;
}
static void set_state_now(int s) {
    dlogf("state %s -> %s", state_name(state), state_name(s));
    if (s == S_GAME || s == S_PAUSE) input_flush_start();
    state = s;
    state_time = SDL_GetTicks();
    ignore_input = 1;
}

// ------------------------------------------------------------------ video
static const int res_list[][2] = { {1280,720}, {1366,768}, {1600,900}, {1920,1080}, {2560,1440}, {3840,2160} };
#define NRES ((int)(sizeof res_list / sizeof res_list[0]))
static int res_avail[NRES], nres_avail;

static void build_res_list(void) {
    SDL_DisplayMode dm;
    int dw = 1920, dh = 1080;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { dw = dm.w; dh = dm.h; }
    nres_avail = 0;
    for (int i = 0; i < NRES; ++i)
        if (res_list[i][0] <= dw && res_list[i][1] <= dh) res_avail[nres_avail++] = i;
    if (!nres_avail) res_avail[nres_avail++] = 0;
}
static int cur_res_index(void) {
    for (int i = 0; i < nres_avail; ++i)
        if (res_list[res_avail[i]][0] == gs.win_w && res_list[res_avail[i]][1] == gs.win_h) return i;
    return 0;
}

static void apply_video(void) {
    if (gs.video == VID_WINDOW) {
        SDL_SetWindowFullscreen(win, 0);
        SDL_SetWindowBordered(win, SDL_TRUE);
        SDL_SetWindowSize(win, gs.win_w, gs.win_h);
        SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    else {
        // schermo intero esclusivo alla risoluzione del desktop
        SDL_DisplayMode dm;
        SDL_SetWindowFullscreen(win, 0);
        if (SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(win), &dm) == 0) {
            SDL_SetWindowSize(win, dm.w, dm.h);
            SDL_SetWindowDisplayMode(win, &dm);
        }
        if (SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN) != 0)
            SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    gfxb_set_vsync(gs.vsync);
}

static double display_refresh(void) {
    SDL_DisplayMode dm;
    if (SDL_GetWindowDisplayMode(win, &dm) == 0 && dm.refresh_rate > 0) return dm.refresh_rate;
    if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(win), &dm) == 0 && dm.refresh_rate > 0) return dm.refresh_rate;
    return 60.0;
}
static double t_emu, t_audio, t_upload, t_draw, t_swap;   // ms accumulati nel secondo (diagnostica)
static double now_ms(void) { return (double)SDL_GetPerformanceCounter() * 1000.0 / SDL_GetPerformanceFrequency(); }
static double dt_avg = 1.0 / 60.0;   // durata media di un ciclo completo
static int vsync_bad;                 // il V-SYNC non blocca (driver): ritmo gestito dall'app
static int vsync_locked(void) {
    // un frame emulato per refresh solo se il V-SYNC funziona davvero a ~60 Hz
    double r = display_refresh();
    return gs.vsync && !vsync_bad && fabs(r - 60.0) < 1.6;
}

// ------------------------------------------------------------------ disegno comune
// barra dei comandi: sempre sul fondo dello schermo (anche se piu' alto del 16:9)
static void draw_hint(const char *s) {
    float vx0, vy0, vx1, vy1; gfx_view(&vx0, &vy0, &vx1, &vy1);
    float dy = vy1 - CANVAS_H;
    gfx_rect(0, 1010 + dy, CANVAS_W, 70, 0x000000a0);
    gfx_text(FONT_REG, 30, vx1 - 60, 1055 + dy, C_DIM, AL_RIGHT, s);
}

static void draw_title_small(float y) {
    if (t_title.id) gfx_image_fit(&t_title, CANVAS_W / 2 - 330, y, 660, 412, C_WHITE);
}

// lista verticale centrata
static void draw_menu(const char **items, int n, int sel, float cx, float y, float size, float gap) {
    for (int i = 0; i < n; ++i) {
        uint32_t col = (i == sel) ? C_GOLD : C_WHITE;
        float w = gfx_text(FONT_BOLD, size, cx, y + i * gap, col, AL_CENTER, items[i]);
        if (i == sel) {
            gfx_rect(cx - w / 2 - 50, y + i * gap - size * 0.38f, 18, 4, C_GOLD);
            gfx_rect(cx + w / 2 + 32, y + i * gap - size * 0.38f, 18, 4, C_GOLD);
        }
    }
}

// riga opzione: etichetta a sinistra, valore a destra con frecce
static void draw_option(float x, float y, float w, const char *label, const char *val, int selected, int enabled) {
    uint32_t lc = !enabled ? C_GREY : (selected ? C_GOLD : C_WHITE);
    uint32_t vc = !enabled ? C_GREY : (selected ? C_WHITE : C_DIM);
    if (selected) gfx_rect(x - 30, y - 52, w + 60, 72, 0xffffff12);
    if (selected) gfx_rect(x - 30, y - 52, 6, 72, C_GOLD);
    gfx_text(FONT_BOLD, 44, x, y, lc, AL_LEFT, label);
    if (val) {
        gfx_text(FONT_REG, 42, x + w - 40, y, vc, AL_RIGHT, val);
        if (selected && enabled) {
            float tw = gfx_text_w(FONT_REG, 42, val);
            gfx_text(FONT_BOLD, 42, x + w - 40 - tw - 30, y, C_GOLD, AL_RIGHT, "<");
            gfx_text(FONT_BOLD, 42, x + w - 10, y, C_GOLD, AL_RIGHT, ">");
        }
    }
}

static void draw_panel_bg(void) {
    // sfondo dei menu: illustrazione del gioco corrente (se c'e') molto scurita
    if (cur_game >= 0 && games[cur_game].bg.id)
        gfx_image_cover(&games[cur_game].bg, 0, 0, CANVAS_W, CANVAS_H, 0x505058ff);
    gfx_rect(0, 0, CANVAS_W, CANVAS_H, 0x00000080);
}

static const tex_t *game_logo(int gi, int jp) {
    game_t *g = &games[gi];
    if (jp && g->logo_jp.id) return &g->logo_jp;
    return g->logo.id ? &g->logo : NULL;
}

// ------------------------------------------------------------------ schermate
static void draw_splash(void) {
    if (t_title.id) {
        gfx_image_fit(&t_title, CANVAS_W / 2 - 560, CANVAS_H / 2 - 350, 1120, 700, C_WHITE);
        gfx_text(FONT_BOLD, 64, CANVAS_W / 2, CANVAS_H / 2 + 390, C_WHITE, AL_CENTER, "FIGHTING COLLECTION");
        gfx_text(FONT_REG, 34, CANVAS_W / 2, CANVAS_H / 2 + 440, C_GOLD, AL_CENTER, "divided by ZERO");
    } else {
        gfx_text(FONT_BOLD, 90, CANVAS_W / 2, CANVAS_H / 2, C_WHITE, AL_CENTER, "FIGHTING COLLECTION");
        gfx_text(FONT_REG, 40, CANVAS_W / 2, CANVAS_H / 2 + 70, C_GOLD, AL_CENTER, "divided by ZERO");
    }
}

static void draw_main(void) {
    draw_title_small(110);
    const char *items[] = { "PLAY", "OPTIONS" };
    draw_menu(items, 2, sel_main, CANVAS_W / 2, 690, 76, 110);
    draw_hint("(A) SELECT     (B) QUIT");
}

static void draw_quit(void) {
    draw_main();
    gfx_rect(0, 0, CANVAS_W, CANVAS_H, 0x000000b0);
    gfx_rect(CANVAS_W / 2 - 400, 360, 800, 360, C_PANEL);
    gfx_rect(CANVAS_W / 2 - 400, 360, 800, 4, C_GOLD);
    gfx_text(FONT_BOLD, 60, CANVAS_W / 2, 470, C_WHITE, AL_CENTER, "QUIT THE APPLICATION?");
    const char *items[] = { "YES", "NO" };
    draw_menu(items, 2, sel_quit, CANVAS_W / 2, 580, 56, 80);
}


// SCANLINES/BLUR/CPU SPEED: valore globale (OPTIONS) o per gioco (-1 = segue OPTIONS; altrimenti ha la precedenza)
static const char *crt_names[3] = { "OFF", "ON", "50%" };   // 0 = OFF, 1 = ON, 2 = 50%
static const char *cpu_names[2] = { "100%", "200%" };
static int eff_scanlines(void) { return (cur_game >= 0 && gset.scanlines >= 0) ? gset.scanlines : gs.scanlines; }
static int eff_blur(void) { return (cur_game >= 0 && gset.blur >= 0) ? gset.blur : gs.blur; }
static int eff_overclock(void) { return (cur_game >= 0 && gset.overclock >= 0) ? gset.overclock : gs.overclock; }

// MONITOR: proporzione fisica dello schermo (AUTO = pixel quadrati, come prima)
static const char *mon_names[MON_COUNT] = { "AUTO", "4:3", "5:4", "16:10", "16:9" };
// SCREEN TYPE / MONITOR per gioco (games\\<id>.ini): finche' il gioco non ne ha uno proprio vale quello globale
static int eff_screen(void) { return (cur_game >= 0 && gset.screen >= 0) ? gset.screen : gs.screen; }
static int eff_monitor(void) { return (cur_game >= 0 && gset.monitor >= 0) ? gset.monitor : gs.monitor; }
static void apply_monitor(void) {
    static const float a[MON_COUNT] = { 0.0f, 4.0f / 3.0f, 5.0f / 4.0f, 16.0f / 10.0f, 16.0f / 9.0f };
    gfx_set_monitor(a[eff_monitor() % MON_COUNT]);
}

static void draw_options(void) {
    gfx_text(FONT_BOLD, 80, 200, 200, C_WHITE, AL_LEFT, "OPTIONS");
    gfx_rect(200, 225, 120, 5, C_GOLD);
    float x = 200, w = 1100, y = 340, g = 88;
    draw_option(x, y + 0 * g, w, "V-SYNC", gs.vsync ? "ON" : "OFF", sel_opt == 0, 1);
    draw_option(x, y + 1 * g, w, "SCANLINES", crt_names[gs.scanlines], sel_opt == 1, 1);
    draw_option(x, y + 2 * g, w, "BLUR", crt_names[gs.blur], sel_opt == 2, 1);
    draw_option(x, y + 3 * g, w, "CPU SPEED", cpu_names[gs.overclock ? 1 : 0], sel_opt == 3, 1);
    draw_option(x, y + 4 * g, w, "MONITOR", mon_names[gs.monitor], sel_opt == 4, 1);
    draw_option(x, y + 5 * g, w, "ROM CACHE", gs.romcache ? "ON" : "OFF", sel_opt == 5, 1);
    draw_option(x, y + 6 * g, w, "ROM CHECK", "", sel_opt == 6, 1);
    if (sel_opt == 5) gfx_text(FONT_REG, 28, x, y + 7 * g - 10, C_DIM, AL_LEFT,
        gs.romcache ? "ENCRYPTED GAMES START FASTER: UP TO 1.5 GB IN THE CACHE FOLDER" : "CACHE OFF: NOTHING IS SAVED TO DISK, THE CACHE FOLDER IS EMPTY");
    draw_hint(sel_opt == 6 ? "(A) OPEN     (B) BACK" : "(LEFT/RIGHT) CHANGE     (B) BACK");
}

// ROM CHECK: stato dei romset nella cartella roms (file riconosciuti dal contenuto, nomi degli zip liberi)
// zip richiesti dai sistemi non Neo Geo (per ora cercati per nome)
static const char *zip_names(int gi) {
    const char *id = games[gi].id;
    return id;
}
static void draw_romcheck(void) {
    gfx_text(FONT_BOLD, 80, 200, 170, C_WHITE, AL_LEFT, "ROM CHECK");
    gfx_rect(200, 195, 120, 5, C_GOLD);
    char b[1300];
    snprintf(b, sizeof b, "%s/roms   (%d zip)", basedir, neorom_zip_count());
    gfx_text(FONT_REG, 26, 200, 245, C_DIM, AL_LEFT, b);
    gfx_text(FONT_REG, 26, 200, 280, C_DIM, AL_LEFT,
        "MAME ROM sets needed (Neo Geo: any zip name; other systems: <set>.zip)");
    // BIOS (separati dai giochi): neogeo.zip riconosciuto dal contenuto, pgm.zip per nome
    gfx_text(FONT_BOLD, 28, 200, 335, C_GOLD, AL_LEFT, "BIOS");
    {
        int ng = neorom_zip_with(0xe72943de, 0x20000) != NULL;
        if (!ng) { snprintf(b, sizeof b, "%s/bios/neogeo.zip", basedir); FILE *f = fopen(b, "rb"); if (f) { ng = 1; fclose(f); } }
        int pg = 0;
        const char *pp[2] = { "roms/pgm.zip", "bios/pgm.zip" };
        for (int k = 0; k < 2 && !pg; ++k) { snprintf(b, sizeof b, "%s/%s", basedir, pp[k]); FILE *f = fopen(b, "rb"); if (f) { pg = 1; fclose(f); } }
        const char *bn[2] = { "neogeo", "pgm" }, *bd[2] = { "SNK Neo Geo MVS", "IGS PolyGame Master (PGM)" };
        int ok[2] = { ng, pg };
        for (int k = 0; k < 2; ++k) {
            float y = 372 + k * 31;
            gfx_text(FONT_BOLD, 22, 200, y, C_WHITE, AL_LEFT, bn[k]);
            gfx_text(FONT_REG, 22, 330, y, C_DIM, AL_LEFT, bd[k]);
            gfx_text(FONT_BOLD, 22, 980, y, ok[k] ? C_GOLD : C_WHITE, AL_RIGHT, ok[k] ? "OK" : "MISSING");
        }
    }
    gfx_text(FONT_BOLD, 28, 200, 470, C_GOLD, AL_LEFT, "GAMES");
    const float cw = 780, x0 = 200, y0 = 506, rh = 26;
    int rows = (NGAMES + 1) / 2;
    for (int i = 0; i < NGAMES; ++i) {
        const game_t *g = &games[i];
        float x = x0 + (i / rows) * (cw + 60), y = y0 + (i % rows) * rh;
        char st[160]; uint32_t col = C_GOLD;
        if (g->sys == SYS_NEOGEO && neorom_known(g->id)) {
            char ms[120];
            int miss = neorom_missing(g->id), tot = neorom_total(g->id);
            if (!miss || g->available) snprintf(st, sizeof st, "OK");
            else if (miss == tot) { snprintf(st, sizeof st, "NEEDS: %s", neorom_sets(g->id)); col = C_DIM; }
            else { neorom_missing_sets(g->id, ms, sizeof ms); snprintf(st, sizeof st, "MISSING: %s (%d files)", ms, miss); col = C_WHITE; }
        } else if (g->available) snprintf(st, sizeof st, "OK");
        else { snprintf(st, sizeof st, "NEEDS: %s", zip_names(i)); col = C_DIM; }
        char t[40]; snprintf(t, sizeof t, "%.30s", g->title);
        gfx_text(FONT_REG, 21, x, y, col == C_GOLD ? C_WHITE : C_DIM, AL_LEFT, t);
        gfx_text(FONT_BOLD, 21, x + cw, y, col, AL_RIGHT, st);
    }
    artdl_status_t ds; artdl_get_status(&ds);
    if (ds.running || ds.done) {   // stato del download a destra della sezione BIOS
        char m1[200], m2[200] = "";
        if (ds.running) { snprintf(m1, sizeof m1, "DOWNLOADING ARTWORK %d / %d", ds.current, ds.total); snprintf(m2, sizeof m2, "%s   %s", ds.title, ds.message); }
        else {
            snprintf(m1, sizeof m1, "ARTWORK: %d IMAGES DOWNLOADED, %d NOT FOUND", ds.downloaded, ds.missing);
            if (!ds.has_key) { snprintf(m1, sizeof m1, "ARTWORK: NO STEAMGRIDDB KEY"); snprintf(m2, sizeof m2, "Put your free SteamGridDB API key in steamgriddb_key.txt (see README)"); }
        }
        gfx_text(FONT_BOLD, 26, 1100, 372, C_GOLD, AL_LEFT, m1);
        gfx_text(FONT_REG, 22, 1100, 405, C_WHITE, AL_LEFT, m2);
    }
    gfx_text(FONT_REG, 20, 1720, 245, C_GREY, AL_RIGHT, "Images: SteamGridDB (www.steamgriddb.com)");
    draw_hint("(A) RESCAN     (X / F2) DOWNLOAD ARTWORK     (B) BACK");
}

static void draw_vidconf(void) {
    draw_options();
    gfx_rect(0, 0, CANVAS_W, CANVAS_H, 0x000000b0);
    gfx_rect(CANVAS_W / 2 - 460, 340, 920, 400, C_PANEL);
    gfx_rect(CANVAS_W / 2 - 460, 340, 920, 4, C_GOLD);
    gfx_text(FONT_BOLD, 54, CANVAS_W / 2, 440, C_WHITE, AL_CENTER, "KEEP THIS VIDEO MODE?");
    int left = 10 - (int)((SDL_GetTicks() - state_time) / 1000);
    if (left < 0) left = 0;
    char s[64]; snprintf(s, sizeof s, "REVERTING IN %d SECONDS", left);
    gfx_text(FONT_REG, 32, CANVAS_W / 2, 500, C_DIM, AL_CENTER, s);
    const char *items[] = { "NO", "YES" };
    draw_menu(items, 2, sel_vidconf, CANVAS_W / 2, 600, 54, 76);
}

static char ext_msg[160];          // errore di avvio di un gioco esterno (mostrato per qualche secondo)
static Uint32 ext_msg_time;

// riquadro "PATCHES APPLIED" del gioco selezionato (patchinfo.h + opzioni disponibili), sopra il titolo
static void draw_patchinfo(int gi, float vx0, float vy1) {
    const char *id = games[gi].id;
    const patchinfo_t *pi = NULL;
    for (size_t i = 0; i < sizeof patchinfo / sizeof patchinfo[0]; ++i) if (!strcmp(patchinfo[i].id, id)) pi = &patchinfo[i];
    char lines[24][96]; int n = 0;
    if (pi) for (const char *s = pi->lines; *s && n < 16; ) {
        const char *e = strchr(s, '\n'); size_t l = e ? (size_t)(e - s) : strlen(s);
        if (l > 95) l = 95;
        memcpy(lines[n], s, l); lines[n][l] = 0; ++n;
        s = e ? e + 1 : s + l;
    }
    int sys = games[gi].sys;
    if (pi && pi->hd) snprintf(lines[n++], 96, "HD rendering of scaled (zoomed) sprites");
    if (visual_has_shadows(id)) snprintf(lines[n++], 96, "SHADOWS option: solid, 75%% or 50%% transparent");
    if (visual_has_hud(id)) snprintf(lines[n++], 96, sys == SYS_CPS3 ? "INTERFACE option: 75%% or 50%% transparent" : "INTERFACE option: no blinking (SOLID) or 75%% / 50%% transparent");
    if (visual_has_fx(id)) snprintf(lines[n++], 96, "SPECIAL EFFECTS option: steady (SOLID) or 75%% / 50%% transparent");
    if (sys != SYS_SNES) {
        snprintf(lines[n++], 96, "Free play, no credit counter on screen");
        snprintf(lines[n++], 96, "No slowdown (CPU SPEED 200%%)");
    }
    if (!n) return;
    float lx = (vx0 < 0 ? vx0 : 0) + 80, dy = vy1 - CANVAS_H;
    float lh = 34, h = 70 + n * lh + 20, y = 960 + dy - 80 - h, w = 820;
    gfx_rect(lx - 24, y, w, h, 0x000000b8);
    gfx_rect(lx - 24, y, 6, h, C_GOLD);
    gfx_text(FONT_BOLD, 34, lx, y + 50, C_GOLD, AL_LEFT, "PATCHES APPLIED");
    for (int i = 0; i < n; ++i) {
        int cont = lines[i][0] == ' ';
        if (!cont) gfx_rect(lx + 2, y + 70 + i * lh + 14, 8, 8, C_DIM);
        gfx_text(FONT_REG, 26, lx + 24, y + 70 + i * lh + 24, C_WHITE, AL_LEFT, cont ? lines[i] + 2 : lines[i]);
    }
}

static void draw_games(void) {
    if (!nglist) {
        gfx_text(FONT_BOLD, 50, CANVAS_W / 2, CANVAS_H / 2, C_WHITE, AL_CENTER, "NO GAMES FOUND IN THE ROMS FOLDER");
        draw_hint("(B) BACK");
        return;
    }
    // illustrazioni con dissolvenza incrociata
    for (int k = 0; k < nglist; ++k) {
        int gi = glist[k];
        float target = (k == sel_game) ? 1.0f : 0.0f;
        bg_alpha[gi] += (target - bg_alpha[gi]) * 0.25f;
        if (bg_alpha[gi] > 0.01f && games[gi].bg.id)
            gfx_image_cover(&games[gi].bg, 0, 0, CANVAS_W, CANVAS_H, 0xffffff00u | (uint32_t)(bg_alpha[gi] * 255));
    }
    // velo a destra per la griglia
    for (int i = 0; i < 24; ++i)
        gfx_rect(760 + i * 20, 0, 20, CANVAS_H, (uint32_t)(i * 9) & 0xff);
    gfx_rect(1240, 0, CANVAS_W - 1240, CANVAS_H, 0x000000d8);

    // griglia 3 colonne: usa tutta l'altezza dello schermo (piu' righe se e' piu' alto del 16:9)
    float vx0, vy0, vx1, vy1; gfx_view(&vx0, &vy0, &vx1, &vy1);
    const int cols = 3;
    float tw = 290, th = 150, gx = 20, gy = 22;
    float x0 = 1000, y0 = vy0 + 120;
    int rows_vis = (int)((vy1 - 80 - y0) / (th + gy)); if (rows_vis < 5) rows_vis = 5;
    int selrow = sel_game / cols;
    static float scroll;
    float want = (float)(selrow - rows_vis + 2); if (want < 0) want = 0;
    int maxrow = (nglist - 1) / cols;
    if (want > maxrow - rows_vis + 1) want = (float)(maxrow - rows_vis + 1);
    if (want < 0) want = 0;
    scroll += (want - scroll) * 0.3f;
    for (int k = 0; k < nglist; ++k) {
        int gi = glist[k];
        int r = k / cols, c = k % cols;
        float x = x0 + c * (tw + gx), y = y0 + (r - scroll) * (th + gy);
        if (y < vy0 - th || y > vy1 - 80) continue;
        int s = (k == sel_game);
        gfx_rect(x, y, tw, th, s ? 0x262632f8 : C_TILE);
        const tex_t *lg = game_logo(gi, 0);
        if (lg) gfx_image_fit(lg, x + 14, y + 10, tw - 28, th - 20, s ? C_WHITE : 0xc8c8c8ffu);
        else {
            gfx_text(FONT_BOLD, 28, x + tw / 2, y + th / 2 + 10, s ? C_WHITE : C_DIM, AL_CENTER, games[gi].title);
        }
        if (s) gfx_rect_outline(x - 4, y - 4, tw + 8, th + 8, 4, C_GOLD);
    }
    // titolo del gioco selezionato
    int gi = glist[sel_game];
    draw_patchinfo(gi, vx0, vy1);
    float dy = vy1 - CANVAS_H;
    float lx = (vx0 < 0 ? vx0 : 0) + 80;   // titolo in basso a sinistra dello schermo
    gfx_text(FONT_BOLD, 54, lx, 960 + dy, C_WHITE, AL_LEFT, games[gi].title);
    if (ext_msg[0] && SDL_GetTicks() - ext_msg_time < 5000) gfx_text(FONT_REG, 34, lx, 1020 + dy, C_GOLD, AL_LEFT, ext_msg);
    draw_hint(games[gi].exe && !games[gi].capture ? (games[gi].backexit ? "(A) PLAY     (B) BACK     IN GAME: HOLD (BACK) 3 S = EXIT" : "(A) PLAY     (B) BACK") : "(A) PLAY     (START) GAME SETTINGS     (B) BACK");
}

static int mode_count(void) {
    return (cur_game >= 0 && game_sys(cur_game) == SYS_CPS3 && cps3cfg_has_vscpu(games[cur_game].id)) ? 3 : 2;
}

static void draw_mode(void) {
    draw_games();
    gfx_rect(0, 0, CANVAS_W, CANVAS_H, 0x000000a0);
    gfx_rect(CANVAS_W / 2 - 380, 340, 760, 400, C_PANEL);
    gfx_rect(CANVAS_W / 2 - 380, 340, 760, 4, C_GOLD);
    gfx_text(FONT_REG, 36, CANVAS_W / 2, 420, C_DIM, AL_CENTER, games[cur_game].title);
    const char *items[] = { "ARCADE", "VERSUS", "VERSUS CPU" };
    int n = mode_count();
    if (n == 3) {
        // VERSUS CPU (JoJo): il P1 sceglie anche l'avversario; stage con SINISTRA/DESTRA
        gfx_rect(CANVAS_W / 2 - 380, 740, 760, 110, C_PANEL);
        draw_menu(items, 3, sel_mode, CANVAS_W / 2, 520, 62, 92);
        if (sel_mode == 2) {
            char d[64]; snprintf(d, sizeof d, "STAGE  <  %s  >", cps3cfg_vs_stage_name(games[cur_game].id, gset.vsstage));
            gfx_text(FONT_REG, 34, CANVAS_W / 2, 800, C_GOLD, AL_CENTER, d);
        } else gfx_text(FONT_REG, 30, CANVAS_W / 2, 800, C_DIM, AL_CENTER, "VERSUS CPU: P1 CHOOSES BOTH FIGHTERS");
    } else draw_menu(items, 2, sel_mode, CANVAS_W / 2, 545, 66, 100);
}

static int in_game_view(void);
static const char *shadow_names[SHADOW_COUNT] = { "SOLID", "75%", "50%" };
static const char *vis_names[VIS_COUNT] = { "DEFAULT", "SOLID", "75%", "50%" };
static const char *area_names[AREA_COUNT] = { "BARS", "FULL" };
static int game_has_shadows(void) { return cur_game >= 0 && visual_has_shadows(games[cur_game].id); }

// voci dei menu GAME SETTINGS e pausa (dipendono da cosa supporta il gioco)
enum { IT_DIFF, IT_INPUT, IT_REGION, IT_EXIT, IT_SCREEN, IT_SHADOWS, IT_HUD, IT_AREA, IT_FX, IT_SCAN, IT_BLUR, IT_CPU, IT_LEVEL, IT_PRIO, IT_BORDER, IT_MONITOR };
static int menu_items(int pause, int *it) {
    int n = 0;
    if (cur_game >= 0 && games[cur_game].capture) {   // gioco PC catturato: solo schermo e filtri
        it[n++] = IT_SCREEN; it[n++] = IT_MONITOR; it[n++] = IT_SCAN; it[n++] = IT_BLUR;
        return n;
    }
    if (pause) { it[n++] = IT_EXIT; it[n++] = IT_INPUT; it[n++] = IT_SCREEN; it[n++] = IT_MONITOR; }
    else {
        if (!(cur_game >= 0 && game_sys(cur_game) == SYS_SNES)) it[n++] = IT_DIFF;   // SNES: difficolta' dal menu del gioco
        it[n++] = IT_INPUT;
        if (!(cur_game >= 0 && (game_sys(cur_game) == SYS_SNES || (game_sys(cur_game) == SYS_PGM && !pgmcfg_has_region(games[cur_game].id))))) it[n++] = IT_REGION;   // SNES: una sola versione (USA)
        if (cur_game >= 0 && game_sys(cur_game) == SYS_CPS3 && cps3cfg_has_maxlevel(games[cur_game].id)) it[n++] = IT_LEVEL;
    }
    if (cur_game >= 0) {
        const char *id = games[cur_game].id;
        if (visual_has_shadows(id)) it[n++] = IT_SHADOWS;
        if (visual_has_hud(id)) { it[n++] = IT_HUD; it[n++] = IT_AREA; }
        if (visual_has_fx(id)) it[n++] = IT_FX;
        if (game_sys(cur_game) == SYS_CPS3 && cps3cfg_has_hudprio(id)) it[n++] = IT_PRIO;
    }
    if (cur_game >= 0 && game_sys(cur_game) == SYS_NEOGEO) it[n++] = IT_BORDER;   // CPS3: immagine piena
    it[n++] = IT_SCAN; it[n++] = IT_BLUR;
    if (!(cur_game >= 0 && game_sys(cur_game) == SYS_STV)) it[n++] = IT_CPU;   // ST-V: Mednafen non ha l'overclock dell'SH-2
    return n;
}

// SCREEN TYPE: ordine 4:3 SQUARE -> 4:3 SQUARE ZOOM -> 4:3 -> 16:9
static int scr_step(int v, int d) {
    static const int ord[SCR_COUNT] = { SCR_SQUARE, SCR_SQZOOM, SCR_43, SCR_169 };
    int i = 0;
    while (i < SCR_COUNT && ord[i] != v) ++i;
    if (i == SCR_COUNT) i = 0;
    return ord[(i + (d < 0 ? SCR_COUNT - 1 : 1)) % SCR_COUNT];
}

static void draw_item(int item, float x, float y, float w, int selected) {
    static const char *scr_names[SCR_COUNT] = { "4:3 SQUARE", "4:3", "16:9", "4:3 SQUARE ZOOM" };
    char d[32];
    switch (item) {
    case IT_DIFF: snprintf(d, sizeof d, "LEVEL %d", gset.difficulty); draw_option(x, y, w, "DIFFICULTY", d, selected, 1); break;
    case IT_INPUT: draw_option(x, y, w, "INPUT", NULL, selected, 1); break;
    case IT_REGION: draw_option(x, y, w, "REGION", gset.region_jp ? "JAP" : "INT", selected, 1); break;
    case IT_LEVEL: draw_option(x, y, w, "CHARACTER LEVEL", gset.maxlevel ? "MAX" : "NORMAL", selected, 1); break;
    case IT_PRIO: { static const char *pn[3] = { "ORIGINAL", "DYNAMIC", "DYNAMIC+" }; draw_option(x, y, w, "HUD PRIORITY", pn[gset.hudprio % 3], selected, 1); } break;
    case IT_EXIT: draw_option(x, y, w, "EXIT GAME", NULL, selected, 1); break;
    case IT_SCREEN: draw_option(x, y, w, "SCREEN TYPE", scr_names[eff_screen()], selected, 1); break;
    case IT_MONITOR: draw_option(x, y, w, "MONITOR", mon_names[eff_monitor()], selected, 1); break;
    case IT_BORDER: draw_option(x, y, w, "BORDERS", gset.border ? "AUTO" : "OFF", selected, 1); break;
    case IT_SHADOWS: draw_option(x, y, w, "SHADOWS", shadow_names[gset.shadows], selected, 1); break;
    case IT_HUD: draw_option(x, y, w, "INTERFACE", vis_names[gset.hud], selected, 1); break;
    case IT_AREA: draw_option(x, y, w, "INTERFACE AREA", area_names[gset.hud_area], selected, gset.hud != VIS_DEFAULT); break;
    case IT_FX: draw_option(x, y, w, "SPECIAL EFFECTS", vis_names[gset.fx], selected, 1); break;
    case IT_SCAN:
        if (gset.scanlines < 0) { snprintf(d, sizeof d, "GLOBAL (%s)", crt_names[gs.scanlines]); draw_option(x, y, w, "SCANLINES", d, selected, 1); }
        else draw_option(x, y, w, "SCANLINES", crt_names[gset.scanlines], selected, 1);
        break;
    case IT_BLUR:
        if (gset.blur < 0) { snprintf(d, sizeof d, "GLOBAL (%s)", crt_names[gs.blur]); draw_option(x, y, w, "BLUR", d, selected, 1); }
        else draw_option(x, y, w, "BLUR", crt_names[gset.blur], selected, 1);
        break;
    case IT_CPU:
        if (gset.overclock < 0) { snprintf(d, sizeof d, "GLOBAL (%s)", cpu_names[gs.overclock ? 1 : 0]); draw_option(x, y, w, "CPU SPEED", d, selected, 1); }
        else draw_option(x, y, w, "CPU SPEED", cpu_names[gset.overclock], selected, 1);
        break;
    }
}

// cicli per gioco: SCANLINES/BLUR GLOBAL -> ON -> 50% -> OFF, CPU SPEED GLOBAL -> 200% -> 100%
static int cycle_val(int v, int d, const int *ord, int n) {
    int i = 0;
    while (i < n && ord[i] != v) ++i;
    if (i == n) i = 0;
    return ord[(i + (d < 0 ? n - 1 : 1)) % n];
}
// cambia SCANLINES/BLUR/CPU SPEED del gioco; ritorna 1 se l'ha gestita (CPU applicata subito)
static int change_crt_item(int item, int d) {
    static const int crt_ord[4] = { -1, 1, 2, 0 }, cpu_ord[3] = { -1, 1, 0 };
    switch (item) {
    case IT_SCAN: gset.scanlines = cycle_val(gset.scanlines, d, crt_ord, 4); return 1;
    case IT_BLUR: gset.blur = cycle_val(gset.blur, d, crt_ord, 4); return 1;
    case IT_CPU: gset.overclock = cycle_val(gset.overclock, d, cpu_ord, 3);
        if (in_game_view()) emu_set_overclock(eff_overclock());
        return 1;
    case IT_BORDER: gset.border ^= 1; border_apply(); return 1;
    case IT_MONITOR: gset.monitor = (eff_monitor() + MON_COUNT + (d < 0 ? -1 : 1)) % MON_COUNT; apply_monitor(); return 1;
    case IT_SCREEN: gset.screen = scr_step(eff_screen(), d); return 1;
    }
    return 0;
}

// cambia il valore di una voce dell'aspetto (d = +1/-1); ritorna 1 se l'ha gestita
static int change_visual_item(int item, int d) {
    switch (item) {
    case IT_SHADOWS: gset.shadows = (gset.shadows + SHADOW_COUNT + d) % SHADOW_COUNT; return 1;
    case IT_HUD:
        gset.hud = (gset.hud + VIS_COUNT + d) % VIS_COUNT;
        if (game_sys(cur_game) == SYS_CPS3 && gset.hud == VIS_SOLID) gset.hud = (gset.hud + VIS_COUNT + d) % VIS_COUNT;   // CPS3: niente lampeggi da togliere
        return 1;
    case IT_AREA: if (gset.hud == VIS_DEFAULT) return 1; gset.hud_area ^= 1; return 1;
    case IT_FX: gset.fx = (gset.fx + VIS_COUNT + d) % VIS_COUNT; return 1;
    case IT_PRIO: gset.hudprio = (gset.hudprio + 3 + d) % 3; return 1;
    }
    return 0;
}

// descrizione (inglese) della voce selezionata in GAME SETTINGS e nel menu di pausa
static const char *item_desc(int item) {
    int cps3 = cur_game >= 0 && game_sys(cur_game) == SYS_CPS3;
    switch (item) {
    case IT_DIFF: return "CPU difficulty, from LEVEL 1 (easiest) to LEVEL 8 (hardest). It is written into the game's own settings.";
    case IT_INPUT: return "Assign the game buttons for player 1 and player 2, separately for controller and keyboard.";
    case IT_REGION: return "INT: international version (English titles and texts).\nJAP: Japanese version, with Japanese titles, logos and texts where the game has them.";
    case IT_LEVEL: return "MAX: every fighter starts at the highest level (LV 32), with full life and all moves.\nNORMAL: fighters start at level 1, as in the original game.";
    case IT_PRIO: return "ORIGINAL: life bars and timer are drawn above the fighters.\nDYNAMIC: fighters, Stands and effects are drawn above the life bars and timer.\nDYNAMIC+: also above the super meters at the bottom.";
    case IT_EXIT: return "Leave the game and go back to the game list. Settings are saved.";
    case IT_SCREEN: return "4:3 SQUARE: square pixels, as in the official collections.\n4:3 SQUARE ZOOM: square pixels enlarged to fill the screen, cropping a few pixels at the edges.\n4:3: shape of an arcade CRT.\n16:9: stretched to widescreen.";
    case IT_MONITOR: return "The physical shape of your screen. AUTO assumes square pixels (right for most PC monitors).\nChoose 4:3, 5:4, 16:10 or 16:9 when the resolution does not match the screen shape, for example a 4:3 CRT at 1280x1024.";
    case IT_BORDER: return "AUTO: detects the black borders some games draw around the picture and enlarges the image to hide them (learned while you play).\nOFF: always shows the full original picture.";
    case IT_SHADOWS: return "Shadows of the fighters, which flickered on the original hardware.\nSOLID: always fully visible.\n75% / 50%: always visible and semi-transparent, closer to how they looked on a CRT.";
    case IT_HUD: return cps3 ? "Life bars, gauges and counters.\nDEFAULT: original look.\n75% / 50%: semi-transparent during fights, so the action behind them stays visible."
                             : "Life bars, gauges and counters.\nDEFAULT: original look (fast flicker already removed).\nSOLID: no blinking at all.\n75% / 50%: semi-transparent during fights, so the action behind them stays visible.";
    case IT_AREA: return "Which parts of the interface the INTERFACE option changes.\nBARS: bars, gauges, icons and counters.\nFULL: also timer, scores, names and messages.";
    case IT_FX: return "Effects that flicker on the original hardware: weapon trails, sparks, projectiles, dust.\nDEFAULT: original.\nSOLID: always visible.\n75% / 50%: always visible and semi-transparent.";
    case IT_SCAN: return "CRT scanlines: ON, 50% (lighter) or OFF.\nGLOBAL uses the value chosen in OPTIONS.";
    case IT_BLUR: return "CRT softness: ON (strong), 50% (lighter) or OFF (sharp pixels).\nGLOBAL uses the value chosen in OPTIONS.";
    case IT_CPU: return "200%: removes the slowdown of the original hardware; the game speed does not change.\n100%: original behaviour, slowdown included.\nGLOBAL uses the value chosen in OPTIONS.";
    }
    return NULL;
}
// testo a capo automatico entro la larghezza w ('\n' = nuovo paragrafo)
static float draw_wrapped(float x, float y, float w, float size, float lh, uint32_t col, const char *s) {
    char line[256]; size_t ll = 0;
    while (*s) {
        const char *e = s; while (*e && *e != ' ' && *e != '\n') ++e;
        char word[128]; size_t wl = (size_t)(e - s) < 127 ? (size_t)(e - s) : 127; memcpy(word, s, wl); word[wl] = 0;
        char test[384]; snprintf(test, sizeof test, "%s%s%s", line, ll ? " " : "", word);
        if (ll && gfx_text_w(FONT_REG, size, test) > w) { gfx_text(FONT_REG, size, x, y, col, AL_LEFT, line); y += lh; snprintf(line, sizeof line, "%s", word); ll = wl; }
        else { snprintf(line, sizeof line, "%s", test); ll = strlen(line); }
        s = e;
        if (*s == ' ') ++s;
        else if (*s == '\n') { gfx_text(FONT_REG, size, x, y, col, AL_LEFT, line); y += lh * 1.35f; line[0] = 0; ll = 0; ++s; }
    }
    if (ll) { gfx_text(FONT_REG, size, x, y, col, AL_LEFT, line); y += lh; }
    return y;
}
static void draw_desc_panel(int item, float x, float y, float w) {
    const char *d = item_desc(item);
    if (!d) return;
    gfx_rect(x - 30, y - 50, 6, 60, C_GOLD);
    draw_wrapped(x, y, w, 30, 42, C_WHITE, d);
}

static void draw_gset(void) {
    draw_panel_bg();
    const tex_t *lg = game_logo(cur_game, gset.region_jp);
    int it[16], n = menu_items(0, it);
    float x = 200, w = 860, y, g;
    if (n <= 7) {
        if (lg) gfx_image_fit(lg, 200, 80, 620, 260, C_WHITE);
        else gfx_text(FONT_BOLD, 60, 200, 220, C_WHITE, AL_LEFT, games[cur_game].title);
        gfx_text(FONT_BOLD, 64, 200, 430, C_WHITE, AL_LEFT, "GAME SETTINGS");
        gfx_rect(200, 452, 120, 5, C_GOLD);
        y = n > 4 ? 560 : 580; g = n > 4 ? (950.0f - 560) / (n - 1) : 100;
    } else {
        // molte voci: logo in alto a destra, elenco piu' alto
        if (lg) gfx_image_fit(lg, 1380, 90, 460, 260, C_WHITE);
        else gfx_text(FONT_BOLD, 44, 1840, 220, C_WHITE, AL_RIGHT, games[cur_game].title);
        gfx_text(FONT_BOLD, 64, 200, 170, C_WHITE, AL_LEFT, "GAME SETTINGS");
        gfx_rect(200, 192, 120, 5, C_GOLD);
        y = 300; g = (960.0f - 300) / (n - 1);
        if (g > 80) g = 80;
    }
    if (g > 100) g = 100;
    for (int i = 0; i < n; ++i) draw_item(it[i], x, y + i * g, w, sel_gset == i);
    if (sel_gset >= 0 && sel_gset < n && item_desc(it[sel_gset])) {
        float dy0 = n <= 7 ? 560 : 440;
        gfx_rect(1150, dy0 - 90, 740, 950 - dy0 + 90, 0x000000a0);   // fondo per la leggibilita' sopra l'illustrazione
        draw_desc_panel(it[sel_gset], 1200, dy0, 640);
    }
    draw_hint("(LEFT/RIGHT) CHANGE     (A) SELECT     (B) BACK");
}

static void draw_input_panel(void) {
    float x = 220, w = 1480;
    gfx_text(FONT_BOLD, 64, x, 170, C_WHITE, AL_LEFT, "INPUT");
    gfx_rect(x, 192, 120, 5, C_GOLD);
    gfx_text(FONT_REG, 34, x + w, 170, C_DIM, AL_RIGHT, games[cur_game].title);
    char pl[32]; snprintf(pl, sizeof pl, "PLAYER %d", input_player + 1);
    int s0 = (sel_input == 0);
    if (s0) gfx_rect(x - 30, 240, w + 60, 70, 0xffffff12), gfx_rect(x - 30, 240, 6, 70, C_GOLD);
    gfx_text(FONT_BOLD, 46, x, 292, s0 ? C_GOLD : C_WHITE, AL_LEFT, pl);
    if (s0) gfx_text(FONT_BOLD, 40, x + 300, 290, C_GOLD, AL_LEFT, "< >");
    gfx_text(FONT_REG, 30, x + 820, 360, C_DIM, AL_CENTER, "CONTROLLER");
    gfx_text(FONT_REG, 30, x + 1260, 360, C_DIM, AL_CENTER, "KEYBOARD");
    pmap_t *m = &gset.map[input_player];
    for (int a = 0; a < NACT; ++a) {
        float y = 430 + a * 60;
        int s = (sel_input == a + 1);
        if (s) gfx_rect(x - 30, y - 44, w + 60, 58, 0xffffff12), gfx_rect(x - 30, y - 44, 6, 58, C_GOLD);
        gfx_text(FONT_BOLD, 40, x, y, s ? C_GOLD : C_WHITE, AL_LEFT, act_names[a]);
        if (s && input_waiting) {
            gfx_text(FONT_BOLD, 36, x + 1040, y, C_GOLD, AL_CENTER, "PRESS A BUTTON OR KEY...");
        }
        else {
            gfx_text(FONT_REG, 38, x + 820, y, m->pad[a] < 0 ? C_GREY : C_WHITE, AL_CENTER, input_pad_name(m->pad[a]));
            gfx_text(FONT_REG, 38, x + 1260, y, m->key[a] < 0 ? C_GREY : C_WHITE, AL_CENTER, input_key_name(m->key[a]));
        }
    }
    int sr = (sel_input == NACT + 1);
    float y = 430 + NACT * 60 + 20;
    if (sr) gfx_rect(x - 30, y - 44, w + 60, 58, 0xffffff12), gfx_rect(x - 30, y - 44, 6, 58, C_GOLD);
    gfx_text(FONT_BOLD, 38, x, y, sr ? C_GOLD : C_DIM, AL_LEFT, "RESET TO DEFAULT");
    draw_hint("(A) MAP     (LEFT/RIGHT) PLAYER     (B) BACK");
}

static void draw_input(void) {
    draw_panel_bg();
    gfx_rect(0, 0, CANVAS_W, CANVAS_H, 0x00000080);
    draw_input_panel();
}

static void draw_loading(void) {
    const tex_t *lg = cur_game >= 0 ? game_logo(cur_game, gset.region_jp) : NULL;
    if (lg) gfx_image_fit(lg, CANVAS_W / 2 - 450, CANVAS_H / 2 - 260, 900, 420, C_WHITE);
    float t = (SDL_GetTicks() % 1200) / 1200.0f;
    gfx_text(FONT_REG, 34, CANVAS_W / 2, 820, 0xffffff00u | (uint32_t)(140 + 100 * sinf(t * 6.2832f)), AL_CENTER, "LOADING");
}

static void draw_pause_overlay(void) {
    gfx_rect(0, 0, CANVAS_W, CANVAS_H, 0x000000a0);
    if (state == S_INPUT) { draw_input_panel(); return; }
    int it[16], n = menu_items(1, it);
    float g = n > 8 ? 78 : n > 4 ? 86 : 100;
    if (g > (CANVAS_H - 260.0f) / n) g = (CANVAS_H - 260.0f) / n;
    float h = 190 + n * g, top = (CANVAS_H - h) / 2;
    if (h < 560) { top -= (560 - h) / 2; h = 560; }
    float px = CANVAS_W / 2 - 820, pw = 1640;
    gfx_rect(px, top, pw, h, 0x0b0b10f0);
    gfx_rect(px, top, pw, 4, C_GOLD);
    gfx_text(FONT_REG, 34, px + 80, top + 70, C_DIM, AL_LEFT, games[cur_game].title);
    float x = px + 80, w = 800, y = top + 190;
    for (int i = 0; i < n; ++i) draw_item(it[i], x, y + i * g, w, sel_pause == i);
    gfx_rect(px + 960, top + 120, 2, h - 170, 0xffffff20);
    if (sel_pause >= 0 && sel_pause < n) draw_desc_panel(it[sel_pause], px + 1020, top + 200, 560);
}

// ------------------------------------------------------------------ gioco
static int game_sys(int gi) { return gi >= 0 ? games[gi].sys : SYS_NEOGEO; }
static void rom_path_g(char *out, size_t n, int gi) {
    snprintf(out, n, "%s/roms/%s.%s", basedir, games[gi].id, game_sys(gi) == SYS_NEOGEO ? "neo" : "zip");
    if (game_sys(gi) == SYS_NEOGEO && neorom_known(games[gi].id) && neorom_missing(games[gi].id) == 0) {
        FILE *f = fopen(out, "rb");   // un .neo gia' pronto ha la precedenza (uso personale); altrimenti dai romset MAME
        if (f) { fclose(f); return; }
        snprintf(out, n, "neorom:%s", games[gi].id);
    }
    if (game_sys(gi) == SYS_SNES) {
        // i set SNES hanno nomi liberi ("WeaponLord (USA).zip"): se <id>.zip manca si prende il primo .zip che inizia con l'id
        FILE *f = fopen(out, "rb");
        if (f) { fclose(f); return; }
        char dir[1100]; snprintf(dir, sizeof dir, "%s/roms", basedir);
        DIR *d = opendir(dir);
        if (!d) return;
        size_t il = strlen(games[gi].id);
        for (struct dirent *e; (e = readdir(d)); ) {
            size_t l = strlen(e->d_name);
            if (l > il + 4 && !strncasecmp(e->d_name, games[gi].id, il) && !strcasecmp(e->d_name + l - 4, ".zip")) {
                snprintf(out, n, "%s/%s", dir, e->d_name);
                break;
            }
        }
        closedir(d);
    }
}
static unsigned native_w(void) { return game_sys(cur_game) == SYS_SNES ? 256 : game_sys(cur_game) == SYS_CPS3 ? 384 : game_sys(cur_game) == SYS_PGM ? 448 : game_sys(cur_game) == SYS_STV ? 352 : 320; }
static unsigned native_h(void) { return 224; }   // ST-V: 352x224 (righe attive; Mednafen mostra 8-231)

static int hd_drop = 0;             // passi di scala HD tolti se il PC non tiene i 60 fps
static double emu_time_avg;
// HD a scala intera per asse (blocchi di pixel regolari), poi adattato con sharp bilinear
static void hd_for_window(int ww, int wh, unsigned *w, unsigned *h) {
    if (game_sys(cur_game) == SYS_SNES) { *w = 512; *h = 448; return; }   // SNES: immagine nativa 512x448
    int x, y, gw, gh;
    gfx_game_rect(eff_screen(), ww, wh, &x, &y, &gw, &gh);
    int nw = (int)native_w();
    double dvw, dvh; gfx_game_visible(eff_screen(), ww, wh, &dvw, &dvh);   // scala sull'area mostrata (bordi/overscan tolti)
    int vw = (int)dvw, vh = (int)dvh;
    if (vw < 1) vw = 1;
    if (vh < 1) vh = 1;
    int k0x = gw / vw < 1 ? 1 : gw / vw, k0y = gh / vh < 1 ? 1 : gh / vh;
    int kx = k0x - hd_drop, ky = k0y - hd_drop;
    if (kx < (k0x < 2 ? k0x : 2)) kx = k0x < 2 ? k0x : 2;
    if (ky < (k0y < 2 ? k0y : 2)) ky = k0y < 2 ? k0y : 2;
    *w = (unsigned)(nw * kx);
    *h = (unsigned)(native_h() * ky);
}

// ------------------------------------------------------------------ bordi neri del gioco (BORDERS AUTO)
// Molti giochi lasciano righe/colonne nere ai bordi (es. 8 righe in alto, 8 pixel ai lati nei giochi a 304 pixel):
// si misurano durante il gioco e, se presenti in almeno l'85% dei campioni, si tolgono (l'area resta piena).
// I conteggi restano nel .ini del gioco: dalla seconda partita il ritaglio vale da subito.
enum { BORDER_MIN_N = 150, BORDER_MAX_N = 6000 };
static int border_edge_crop(int e, int cur) {
    if (gset.bn < BORDER_MIN_N) return 0;
    for (int k = 3; k >= 1; --k) {
        int need = (cur >= 8 * k) ? 75 : 85;   // isteresi: niente salti avanti e indietro
        if ((long)gset.bc[e][k - 1] * 100 >= (long)gset.bn * need) return 8 * k;
    }
    return 0;
}
static void border_apply(void) {
    int c[4] = { 0, 0, 0, 0 };
    if (cur_game >= 0 && game_sys(cur_game) == SYS_NEOGEO && gset.border) {
        int o[4]; gfx_get_crop(&o[2], &o[0], &o[3], &o[1]);
        for (int e = 0; e < 4; ++e) c[e] = border_edge_crop(e, o[e]);
    }
    int l, t, r, b; gfx_get_crop(&l, &t, &r, &b);
    if (l == c[2] && t == c[0] && r == c[3] && b == c[1]) return;
    gfx_set_crop(c[2], c[0], c[3], c[1]);
    gfx_get_crop(&l, &t, &r, &b);
    dlogf("borders: top %d bottom %d left %d right %d (samples %d)", t, b, l, r, gset.bn);
}
static inline int px_dark(uint32_t p) {
    return ((p >> 16) & 255) < 24 && ((p >> 8) & 255) < 24 && (p & 255) < 24;
}
static void border_sample(void) {
    unsigned w, h, pitch;
    const uint32_t *px = emu_video(&w, &h, &pitch);
    int nw = (int)native_w(), nh = 224;
    if (!px || (int)w < nw || (int)h < nh) return;
    int kx = (int)w / nw, ky = (int)h / nh;
#define BPX(c, r) px[(size_t)((r) * ky + ky / 2) * pitch + (size_t)((c) * kx + kx / 2)]
    int v[4] = { 0, 0, 0, 0 }, c, r;
    for (r = 0; r < 32; ++r) { for (c = 0; c < nw && px_dark(BPX(c, r)); ++c) {} if (c < nw) break; }
    v[0] = r;
    for (r = 0; r < 32; ++r) { for (c = 0; c < nw && px_dark(BPX(c, nh - 1 - r)); ++c) {} if (c < nw) break; }
    v[1] = r;
    for (c = 0; c < 32; ++c) { for (r = 0; r < nh && px_dark(BPX(c, r)); ++r) {} if (r < nh) break; }
    v[2] = c;
    for (c = 0; c < 32; ++c) { for (r = 0; r < nh && px_dark(BPX(nw - 1 - c, r)); ++r) {} if (r < nh) break; }
    v[3] = c;
#undef BPX
    static int logn;
    if (++logn % 600 == 1) dlogf("borders measured: top %d bottom %d left %d right %d", v[0], v[1], v[2], v[3]);
    // schermate scure/dissolvenze: non rappresentative
    if (v[0] >= 32 || v[1] >= 32 || v[2] >= 32 || v[3] >= 32) return;
    gset.bn++;
    for (int e = 0; e < 4; ++e)
        for (int k = 0; k < 3; ++k) if (v[e] >= 8 * (k + 1)) gset.bc[e][k]++;
    if (gset.bn >= BORDER_MAX_N) {
        gset.bn /= 2;
        for (int e = 0; e < 4; ++e) for (int k = 0; k < 3; ++k) gset.bc[e][k] /= 2;
    }
    border_apply();
}

static int start_game(void) {
    char rp[1200], bp[1200];
    rom_path_g(rp, sizeof rp, cur_game);
    snprintf(bp, sizeof bp, "%s/bios/neogeo.zip", basedir);
    { const char *z = neorom_zip_with(0xe72943de, 0x20000); if (z) snprintf(bp, sizeof bp, "%s", z); }   // sp-u2.sp1 (USA)
    int ww, wh; gfxb_drawable(&ww, &wh);
    gfx_set_native((int)native_w(), (int)native_h());   // SNES: 256x224 logici (buffer 512x448); ST-V 352x240
    gfx_set_crop(0, 0, 0, 0);
    apply_monitor();   // MONITOR del gioco (o quello di OPTIONS)
    border_apply();
    emu_prepare(game_sys(cur_game));
    { unsigned hw, hh; hd_for_window(ww, wh, &hw, &hh); emu_set_hd(hw, hh); }
    if (game_sys(cur_game) == SYS_SNES) {
        if (!emu_load_snes(rp)) { dlogf("SNES load failed: %s", rp); return 0; }
        dlogf("SNES %s: %s", games[cur_game].id, rp);
        if (!strcmp(games[cur_game].id, "weaponlord")) dlogf("weaponlord: CPU blocking limited: %d", snescore_patch_weaponlord());
    } else
    if (game_sys(cur_game) == SYS_CPS3) {
        int reg = cps3cfg_region(games[cur_game].id, gset.region_jp);
        if (!emu_load_cps3(rp, games[cur_game].id, reg)) { dlogf("CPS3 load failed: %s", rp); return 0; }
        emu_set_overclock(eff_overclock());
        cps3cfg_apply(games[cur_game].id, gset.difficulty, gset.region_jp);
        cps3cfg_vscpu_setup(games[cur_game].id, play_versus == 2, gset.vsstage);
        dlogf("CPS3 %s: region %02x, HD %ux%u", games[cur_game].id, reg, emu_hd_w(), emu_hd_h());
    } else
    if (game_sys(cur_game) == SYS_PSIKYO) {
        int reg = psikyocfg_region(games[cur_game].id, gset.region_jp);
        if (!emu_load_psikyo(rp, games[cur_game].id, reg)) { dlogf("Psikyo load failed: %s", rp); return 0; }
        emu_set_overclock(eff_overclock());
        psikyocfg_apply(games[cur_game].id, gset.difficulty);
        dlogf("Psikyo %s: region %d, HD %ux%u", games[cur_game].id, reg, emu_hd_w(), emu_hd_h());
    } else
    if (game_sys(cur_game) == SYS_PGM) {
        // BIOS: roms/pgm.zip (anche dentro lo zip del gioco se il set e' "merged")
        char pb[1200]; snprintf(pb, sizeof pb, "%s/roms/pgm.zip", basedir);
        { FILE *f = fopen(pb, "rb"); if (f) fclose(f); else snprintf(pb, sizeof pb, "%s/bios/pgm.zip", basedir); }
        int reg = pgmcfg_region(games[cur_game].id, gset.region_jp);
        emu_set_overclock(eff_overclock());
        if (!emu_load_pgm(rp, pb, games[cur_game].id, reg)) { dlogf("PGM load failed: %s (BIOS %s)", rp, pb); return 0; }
        pgmcfg_apply(games[cur_game].id, gset.difficulty);
        dlogf("PGM %s: region %d, HD %ux%u", games[cur_game].id, reg, emu_hd_w(), emu_hd_h());
    } else
    if (game_sys(cur_game) == SYS_STV) {
        // BIOS: dentro lo zip del gioco (set MAME) o roms/stvbios.zip; cartella di lavoro del core: <app>/nvram/stv
        char sb[1200], nd[1200];
        snprintf(sb, sizeof sb, "%s/roms/stvbios.zip", basedir);
        snprintf(nd, sizeof nd, "%s/nvram", basedir); MKDIR(nd);
        snprintf(nd, sizeof nd, "%s/nvram/stv", basedir);
        int reg = stvcfg_region(games[cur_game].id, gset.region_jp);
        emu_set_overclock(eff_overclock());
        if (!emu_load_stv(rp, sb, reg, nd)) { dlogf("ST-V load failed: %s (%s)", rp, emu_stv_error()); return 0; }
        stvcfg_apply(games[cur_game].id, gset.difficulty);
        // G. AXE selezionabile solo in VERSUS: in ARCADE con G. AXE il gioco si blocca dopo la selezione (mappa del
        // percorso, eccezione dell'SH-2; succede anche con la ROM modificata originale)
        if (play_versus == 1) dlogf("ST-V %s: G. AXE unlock %s", games[cur_game].id, stvcfg_unlock(games[cur_game].id) ? "ok" : "not applied (different ROM)");
        dlogf("ST-V %s: region %d, HD %ux%u", games[cur_game].id, reg, emu_hd_w(), emu_hd_h());
    } else {
    emu_set_overclock(eff_overclock());
    if (!strncmp(rp, "neorom:", 7)) {
        Uint32 t0 = SDL_GetTicks();
        char cd[1200]; snprintf(cd, sizeof cd, "%s/cache", basedir);   // cache facoltativa (ROM CACHE, vedi neorom.c)
        if (gs.romcache) MKDIR(cd);
        size_t isz = 0; void *img = neorom_build(rp + 7, gs.romcache ? cd : NULL, &isz);
        if (!img) { dlogf("romset %s: %s", rp + 7, neorom_error()); return 0; }
        dlogf("romset %s: %u KB image ready in %u ms", rp + 7, (unsigned)(isz >> 10), SDL_GetTicks() - t0);
        if (!emu_load_image(img, isz, bp, gset.region_jp, NULL)) { dlogf("load failed (BIOS %s)", bp); return 0; }
    } else
    if (!emu_load(rp, bp, gset.region_jp, NULL)) return 0;
    size_t psz; const uint8_t *p = emu_prom(&psz);
    softdip_apply(p, psz, gset.difficulty, gset.region_jp);
    { int r = rompatch_apply(games[cur_game].id); if (r) dlogf("ROM patches %s: %d", games[cur_game].id, r); }
    }
    visual_apply(games[cur_game].id, gset.shadows, gset.hud, gset.hud_area, gset.fx);
    if (game_sys(cur_game) == SYS_CPS3) cps3cfg_set_hudprio(games[cur_game].id, gset.hudprio);
    dlogf("look %s: shadows %d interface %d area %d effects %d hud prio %d, scanlines %d blur %d cpu %d (game %d/%d/%d)", games[cur_game].id, gset.shadows, gset.hud, gset.hud_area, gset.fx, gset.hudprio,
        eff_scanlines(), eff_blur(), eff_overclock(), gset.scanlines, gset.blur, gset.overclock);
    dlogf("screen %s: screen %d monitor %d borders %d (game %d/%d)", games[cur_game].id, eff_screen(), eff_monitor(), gset.border, gset.screen, gset.monitor);
    boot_frames = 0; booted = 0; frame_count = 0;
    emu_acc = 0;
    return 1;
}

static void stop_game(void) {
    if (cur_game >= 0 && game_sys(cur_game) == SYS_NEOGEO)
        dlogf("borders learned %s: samples %d, top %d/%d/%d bottom %d/%d/%d left %d/%d/%d right %d/%d/%d", games[cur_game].id, gset.bn,
            gset.bc[0][0], gset.bc[0][1], gset.bc[0][2], gset.bc[1][0], gset.bc[1][1], gset.bc[1][2],
            gset.bc[2][0], gset.bc[2][1], gset.bc[2][2], gset.bc[3][0], gset.bc[3][1], gset.bc[3][2]);
    save_game_settings();   // bordi appresi
    audio_pause(1);
    audio_clear();
    emu_unload();
}

// giocatore p in partita: Neo Geo = stato BIOS $10FDB6/7 == 1; CPS3 / Psikyo = indirizzo per gioco (cps3cfg / psikyocfg)
static int player_in(int p) {
    if (game_sys(cur_game) == SYS_SNES) return 1;   // nessuno stato di BIOS: si parte dal titolo del gioco
    if (game_sys(cur_game) == SYS_CPS3) return cps3cfg_player_in(games[cur_game].id, p);
    if (game_sys(cur_game) == SYS_PSIKYO) return psikyocfg_player_in(games[cur_game].id, p);
    if (game_sys(cur_game) == SYS_PGM) return pgmcfg_player_in(games[cur_game].id, p);
    if (game_sys(cur_game) == SYS_STV) return stvcfg_player_in(games[cur_game].id, p);
    return emu_read8(0xfdb6 + p) == 1;
}

// Pressioni automatiche di START: avvio (ARCADE: P1; VERSUS: P1 poi P2) e, in VERSUS,
// rientro automatico del giocatore che ha perso (si torna sempre alla selezione personaggi)
static void auto_start(unsigned *sys0, unsigned *sys1, int boot) {
    int m1 = player_in(0), m2 = player_in(1);
    int pulse = (frame_count % 16) < 4;
    if (boot) {
        if (!m1) { if (pulse) *sys0 |= SYS_START; }
        else if (play_versus && !m2) { if (pulse) *sys1 |= SYS_START; }
        return;
    }
    if (play_versus && game_sys(cur_game) == SYS_STV && stvcfg_versus_pulse(games[cur_game].id)) {   // ST-V: rientro senza flag affidabili
        if (pulse) { *sys0 |= SYS_START; *sys1 |= SYS_START; }
        return;
    }
    if (play_versus) {
        if (!m1 && pulse) *sys0 |= SYS_START;
        if (!m2 && pulse) *sys1 |= SYS_START;
    }
}

static int boot_done(void) {
    return player_in(0) && (!play_versus || player_in(1));
}

static void run_boot(void) {
    // avvio veloce senza rendering, al massimo ~25 ms per ciclo
    Uint64 t0 = SDL_GetPerformanceCounter(), f = SDL_GetPerformanceFrequency();
    while ((SDL_GetPerformanceCounter() - t0) * 1000 / f < 25) {
        unsigned s0 = 0, s1 = 0;
        auto_start(&s0, &s1, 1);
        emu_set_input(0, 0, s0);
        emu_set_input(1, 0, s1);
        emu_frame_fast();
        if (game_sys(cur_game) == SYS_CPS3) cps3cfg_frame(games[cur_game].id, gset.maxlevel);
        if (game_sys(cur_game) == SYS_PGM) pgmcfg_frame(games[cur_game].id);
        if (game_sys(cur_game) == SYS_STV) stvcfg_frame(games[cur_game].id);
        ++frame_count;
        if (++boot_frames > 120 && boot_done()) { booted = 1; break; }
        if (boot_frames > 4000) { booted = 1; break; }
    }
}

static void run_game_frames(double dt) {
    int n;
    if (vsync_locked()) n = 1;
    else {
        emu_acc += dt;
        double fd = 1.0 / emu_fps();
        n = (int)(emu_acc / fd);
        emu_acc -= n * fd;
        if (n > 2) { n = 2; emu_acc = 0; }
    }
    for (int i = 0; i < n; ++i) {
        unsigned js[2], ss[2] = { 0, 0 };
        for (int p = 0; p < 2; ++p) {
            int st_edge, sel;
            js[p] = input_game(p, &gset.map[p], &st_edge, &sel, 1);
            if (sel) ss[p] |= SYS_START;          // SELECT = START del gioco (sfidante / continue)
            if (st_edge && !ignore_input) { sel_pause = 0; set_state_now(S_PAUSE); audio_pause(1); return; }
        }
        auto_start(&ss[0], &ss[1], 0);
        if (play_versus == 2 && game_sys(cur_game) == SYS_CPS3) cps3cfg_vscpu_input(games[cur_game].id, &js[0], &js[1]);
        emu_set_input(0, js[0], ss[0]);
        emu_set_input(1, js[1], ss[1]);
        Uint64 t0 = SDL_GetPerformanceCounter();
        emu_frame();
        if (game_sys(cur_game) == SYS_CPS3) cps3cfg_frame(games[cur_game].id, gset.maxlevel);
        if (game_sys(cur_game) == SYS_PGM) pgmcfg_frame(games[cur_game].id);
        if (game_sys(cur_game) == SYS_STV) stvcfg_frame(games[cur_game].id);
        visual_frame(games[cur_game].id);
        if ((frame_count & 3) == 0 && game_sys(cur_game) == SYS_NEOGEO && player_in(0)) border_sample();
        double ft = (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency();
        emu_time_avg = emu_time_avg * 0.97 + ft * 0.03;
        t_emu += ft;
        if (frame_count > 240 && emu_time_avg > 12.5 && hd_drop < 4) { hd_drop++; dlogf("slow emulated frame (%.1f ms) -> HD scale -%d", emu_time_avg, hd_drop); emu_time_avg = 0; }
        ++frame_count;
        size_t na; const int16_t *a = emu_audio(&na);
        double ta = now_ms();
        audio_push(a, na);
        t_audio += now_ms() - ta;
    }
}

// ------------------------------------------------------------------ logica dei menu
// Cambio di modo video: si applica subito ma va confermato entro 10 s, altrimenti torna al precedente
static gsettings_t gs_prev;

// SCANLINES/BLUR: ordine ON -> 50% -> OFF (valori 1, 2, 0)
static int crt_step(int v, int dir) {
    static const int ord[3] = { 1, 2, 0 };
    int i = 0;
    while (i < 3 && ord[i] != v) ++i;
    if (i == 3) i = 0;
    return ord[(i + (dir < 0 ? 2 : 1)) % 3];
}

static void change_option(int dir) {
    switch (sel_opt) {
    case 0: gs.vsync ^= 1; gfxb_set_vsync(gs.vsync); break;
    case 1: gs.scanlines = crt_step(gs.scanlines, dir); break;
    case 2: gs.blur = crt_step(gs.blur, dir); break;
    case 3: gs.overclock ^= 1; emu_set_overclock(gs.overclock); break;
    case 4: gs.monitor = (gs.monitor + MON_COUNT + (dir < 0 ? -1 : 1)) % MON_COUNT; apply_monitor(); break;
    case 5:
        gs.romcache ^= 1;
        if (!gs.romcache) { char cd[1200]; snprintf(cd, sizeof cd, "%s/cache", basedir); neorom_cache_clear(cd); }
        break;
    }
    settings_save(basedir, &gs);
}

static void save_game_settings(void) {
    if (cur_game >= 0) game_settings_save(basedir, games[cur_game].id, &gset);
}

static void open_game_settings_for(int gi) {
    cur_game = gi;
    input_set_system(games[gi].sys);   // azioni e mappatura predefinita del sistema
    if (games[gi].sys == SYS_CPS3) input_set_layout(cps3cfg_layout(games[gi].id));
    game_settings_load(basedir, games[gi].id, &gset);
    if (games[gi].sys == SYS_CPS3 && gset.hud == VIS_SOLID) gset.hud = VIS_DEFAULT;
}

static void input_assign(int a, int pad, int key) {
    pmap_t *m = &gset.map[input_player];
    if (pad >= 0) {
        for (int i = 0; i < NACT; ++i) if (m->pad[i] == pad) m->pad[i] = -1;
        m->pad[a] = pad;
    }
    if (key >= 0) {
        for (int i = 0; i < NACT; ++i) if (m->key[i] == key) m->key[i] = -1;
        m->key[a] = key;
    }
    save_game_settings();
}

static void update_input_screen(unsigned ev, int back_state) {
    if (input_waiting) {
        int pad, key;
        if (input_capture_poll(&pad, &key)) {
            input_assign(sel_input - 1, pad, key);
            input_waiting = 0;
            ignore_input = 1;
        }
        else if (SDL_GetTicks() - input_wait_start > 5000) {
            input_capture_cancel();
            input_waiting = 0;
        }
        return;
    }
    int n = NACT + 2;
    if (ev & M_UP) sel_input = (sel_input + n - 1) % n;
    if (ev & M_DOWN) sel_input = (sel_input + 1) % n;
    if (ev & (M_LEFT | M_RIGHT)) input_player ^= 1;
    if (ev & M_OK) {
        if (sel_input == 0) input_player ^= 1;
        else if (sel_input <= NACT) { input_waiting = 1; input_wait_start = SDL_GetTicks(); input_capture_begin(); }
        else { input_default_map(input_player, &gset.map[input_player]); save_game_settings(); }
    }
    if (ev & M_BACK) {
        if (back_state == S_PAUSE) set_state_now(S_PAUSE);
        else go(back_state);
    }
}

static void update(double dt) {
    {   // fine del download delle immagini: ricarica loghi e sfondi (thread principale)
        static int was_running;
        artdl_status_t ds; artdl_get_status(&ds);
        if (was_running && !ds.running && ds.done && !in_game_view()) {
            load_assets(); if (sel_game >= nglist) sel_game = 0;
            dlogf("artwork download finished: %d images, %d not found, SteamGridDB key %s", ds.downloaded, ds.missing, ds.has_key ? "yes" : "no");
            was_running = 0;
        } else if (ds.running) was_running = 1;
    }
    unsigned ev = ignore_input ? 0 : input_menu();
    if (next_state >= 0) ev = 0;
    Uint32 t = SDL_GetTicks() - state_time;
    switch (state) {
    case S_SPLASH:
        if (t > 2600 || (ev & (M_OK | M_START | M_BACK))) go(S_MAIN);
        break;
    case S_MAIN:
        if (ev & (M_UP | M_DOWN)) sel_main ^= 1;
        if (ev & M_OK) go(sel_main == 0 ? S_GAMES : S_OPTIONS);
        if (ev & M_BACK) { sel_quit = 1; set_state_now(S_QUIT); }
        break;
    case S_QUIT:
        if (ev & (M_UP | M_DOWN | M_LEFT | M_RIGHT)) sel_quit ^= 1;
        if (ev & M_OK) { if (sel_quit == 0) { SDL_Event q; q.type = SDL_QUIT; SDL_PushEvent(&q); } else set_state_now(S_MAIN); }
        if (ev & M_BACK) set_state_now(S_MAIN);
        break;
    case S_VIDCONF:
        if (ev & (M_UP | M_DOWN | M_LEFT | M_RIGHT)) sel_vidconf ^= 1;
        if ((ev & M_OK) && sel_vidconf == 1) { settings_save(basedir, &gs); set_state_now(S_OPTIONS); }
        else if ((ev & (M_OK | M_BACK)) || t > 10000) {
            gs = gs_prev; apply_video(); settings_save(basedir, &gs); set_state_now(S_OPTIONS);
        }
        break;
    case S_OPTIONS:
        if (ev & M_UP) sel_opt = (sel_opt + 6) % 7;
        if (ev & M_DOWN) sel_opt = (sel_opt + 1) % 7;
        if (sel_opt == 6) { if (ev & M_OK) go(S_ROMCHECK); }
        else {
            if (ev & M_LEFT) change_option(-1);
            if (ev & (M_RIGHT | M_OK)) change_option(1);
        }
        if (ev & M_BACK) go(S_MAIN);
        break;
    case S_ROMCHECK:
        if (ev & M_OK) { load_assets(); if (sel_game >= nglist) sel_game = 0; }
        if (ev & M_X) {   // nuova ricerca, poi download delle immagini mancanti dei giochi completi
            load_assets(); if (sel_game >= nglist) sel_game = 0;
            static artdl_game_t dl[64]; int n = 0;
            for (int k = 0; k < nglist && n < 64; ++k) {
                game_t *g = &games[glist[k]];
                dl[n].id = g->id; dl[n].title = g->title; dl[n].search = NULL;
                dl[n].has_logo = g->logo.id != 0; dl[n].has_bg = g->bg.id != 0;
                ++n;
            }
            if (artdl_start(basedir, dl, n)) dlogf("artwork download started (%d games)", n);
        }
        if (ev & M_BACK) go(S_OPTIONS);
        break;
    case S_GAMES:
        if (nglist) {
            if (ev & M_LEFT) sel_game = (sel_game + nglist - 1) % nglist;
            if (ev & M_RIGHT) sel_game = (sel_game + 1) % nglist;
            if (ev & M_UP) { if (sel_game >= 3) sel_game -= 3; }
            if (ev & M_DOWN) { if (sel_game + 3 < nglist) sel_game += 3; else if (sel_game / 3 < (nglist - 1) / 3) sel_game = nglist - 1; }
            if (games[glist[sel_game]].exe) {
                // gioco esterno: nessuna modalita'/impostazioni dell'app, si lancia l'eseguibile
                if (ev & M_OK) { cur_game = glist[sel_game]; if (games[cur_game].capture) open_game_settings_for(cur_game); go(S_EXTERNAL); }
                // gioco catturato: START = impostazioni (schermo e filtri dell'app)
                if ((ev & M_START) && games[glist[sel_game]].capture) { open_game_settings_for(glist[sel_game]); sel_gset = 0; go(S_GSET); }
            }
            else {
                if ((ev & M_OK) && games[glist[sel_game]].sys == SYS_SNES) { open_game_settings_for(glist[sel_game]); play_versus = 0; go(S_LOADING); }   // SNES: niente ARCADE/VERSUS, si parte dal titolo
                else if (ev & M_OK) { open_game_settings_for(glist[sel_game]); sel_mode = 0; set_state_now(S_MODE); }
                if (ev & M_START) { open_game_settings_for(glist[sel_game]); sel_gset = 0; go(S_GSET); }
            }
        }
        if (ev & M_BACK) go(S_MAIN);
        break;
    case S_MODE:
        { int n = mode_count();
          if (sel_mode >= n) sel_mode = 0;
          if (ev & M_UP) sel_mode = (sel_mode + n - 1) % n;
          if (ev & M_DOWN) sel_mode = (sel_mode + 1) % n;
          if (sel_mode == 2 && (ev & (M_LEFT | M_RIGHT))) {
              int ns = cps3cfg_vs_stage_count(games[cur_game].id);
              if (ns > 0) { gset.vsstage = (gset.vsstage + ns + ((ev & M_LEFT) ? -1 : 1)) % ns; save_game_settings(); }
          } }
        if (ev & M_OK) { play_versus = sel_mode; go(S_LOADING); }
        if (ev & M_BACK) set_state_now(S_GAMES);
        break;
    case S_GSET:
        { int it[16], n = menu_items(0, it);
          if (sel_gset >= n) sel_gset = 0;
          if (ev & M_UP) sel_gset = (sel_gset + n - 1) % n;
          if (ev & M_DOWN) sel_gset = (sel_gset + 1) % n;
          int item = it[sel_gset];
          if (ev & (M_LEFT | M_RIGHT)) {
              int d = (ev & M_LEFT) ? -1 : 1;
              if (item == IT_DIFF) { gset.difficulty += d; if (gset.difficulty < 1) gset.difficulty = 1; if (gset.difficulty > 8) gset.difficulty = 8; }
              if (item == IT_REGION) gset.region_jp ^= 1;
              if (item == IT_LEVEL) gset.maxlevel ^= 1;
              change_visual_item(item, d);
              change_crt_item(item, d);
              save_game_settings();
          }
          if (ev & M_OK) {
              if (item == IT_INPUT) { sel_input = 1; input_player = 0; input_from_pause = 0; go(S_INPUT); }
              else if (item == IT_REGION) { gset.region_jp ^= 1; save_game_settings(); }
              else if (item == IT_LEVEL) { gset.maxlevel ^= 1; save_game_settings(); }
              else if (item == IT_DIFF) { gset.difficulty = gset.difficulty % 8 + 1; save_game_settings(); }
              else if (change_visual_item(item, 1)) save_game_settings();
              else if (change_crt_item(item, 1)) save_game_settings();
          } }
        if (ev & (M_BACK | M_START)) go(S_GAMES);
        break;
    case S_INPUT:
        update_input_screen(ev, input_from_pause ? S_PAUSE : S_GSET);
        break;
    case S_LOADING:
        if (!booted) {
            run_boot();
            if (booted) {
                audio_clear();
                audio_pause(0);
                set_state_now(S_GAME);
                fade = 1.0f; fading_out = 0;
            }
        }
        break;
    case S_GAME:
        run_game_frames(dt);
        break;
    case S_PAUSE: {
        int st_edge = 0, sel;
        for (int p = 0; p < 2; ++p) { int e; input_game(p, &gset.map[p], &e, &sel, 0); st_edge |= e; }
        if (ignore_input) st_edge = 0;
        if (st_edge || (ev & M_BACK)) { set_state_now(S_GAME); audio_clear(); audio_pause(0); emu_acc = 0; break; }
        { int it[16], n = menu_items(1, it);
          if (sel_pause >= n) sel_pause = 0;
          if (ev & M_UP) sel_pause = (sel_pause + n - 1) % n;
          if (ev & M_DOWN) sel_pause = (sel_pause + 1) % n;
          int item = it[sel_pause];
          if ((ev & (M_LEFT | M_RIGHT | M_OK)) && change_visual_item(item, (ev & M_LEFT) ? -1 : 1)) {
              // cambio durante la pausa: visibile alla ripresa del gioco (l'immagine in pausa e' congelata)
              visual_apply(games[cur_game].id, gset.shadows, gset.hud, gset.hud_area, gset.fx);
              if (game_sys(cur_game) == SYS_CPS3) cps3cfg_set_hudprio(games[cur_game].id, gset.hudprio);
              save_game_settings();
          }
          else if ((ev & (M_LEFT | M_RIGHT | M_OK)) && change_crt_item(item, (ev & M_LEFT) ? -1 : 1)) {
              save_game_settings();   // filtro CRT visibile subito sul menu di pausa, CPU applicata subito
          }
          else if (ev & M_OK) {
              if (item == IT_EXIT) { go(S_GAMES); }
              else if (item == IT_INPUT) { sel_input = 1; input_player = 0; input_from_pause = 1; set_state_now(S_INPUT); }
          } }
        break; }
    }
}

static int in_game_view(void) {
    return state == S_GAME || state == S_PAUSE || (state == S_INPUT && input_from_pause);
}

static void render(void) {
    int ww, wh;
    gfxb_drawable(&ww, &wh);
    { static int lw, lh; if (ww != lw || wh != lh) {
        int sw = 0, sh = 0; SDL_GetWindowSize(win, &sw, &sh);
        dlogf("drawable %dx%d (window %dx%d, display %d)", ww, wh, sw, sh, SDL_GetWindowDisplayIndex(win)); lw = ww; lh = wh; } }
    gfx_frame_begin(ww, wh);
    if (in_game_view()) {
        unsigned hw, hh;
        hd_for_window(ww, wh, &hw, &hh);
        if (state == S_GAME && (hw != emu_hd_w() || hh != emu_hd_h())) { dlogf("HD %ux%u -> %ux%u (screen %dx%d)", emu_hd_w(), emu_hd_h(), hw, hh, ww, wh); emu_set_hd(hw, hh); }
        unsigned w, h, pitch;
        const uint32_t *px = emu_video(&w, &h, &pitch);
        double t0 = now_ms();
        if (state == S_GAME) gfx_game_upload(px, (int)w, (int)h, (int)pitch);
        double t1 = now_ms();
        gfx_game_begin(eff_screen());
        if (state == S_GAME && play_versus == 2 && game_sys(cur_game) == SYS_CPS3) {
            const char *hint = cps3cfg_vscpu_hint(games[cur_game].id);
            if (hint) {
                gfx_rect(CANVAS_W / 2 - 420, CANVAS_H - 150, 840, 70, 0x000000c0);
                gfx_text(FONT_BOLD, 40, CANVAS_W / 2, CANVAS_H - 102, C_GOLD, AL_CENTER, hint);
            }
        }
        gfx_game_end(eff_scanlines(), eff_blur());
        if (state != S_GAME) draw_pause_overlay();   // dopo il filtro CRT: niente scanline/blur sul menu
        gfx_fade(fade);
        t_upload += t1 - t0;
        t_draw += now_ms() - t1;
        return;
    }
    switch (state) {
    case S_SPLASH: draw_splash(); break;
    case S_MAIN: draw_main(); break;
    case S_QUIT: draw_quit(); break;
    case S_OPTIONS: draw_options(); break;
    case S_ROMCHECK: draw_romcheck(); break;
    case S_VIDCONF: draw_vidconf(); break;
    case S_GAMES: draw_games(); break;
    case S_MODE: draw_mode(); break;
    case S_GSET: draw_gset(); break;
    case S_INPUT: draw_input(); break;
    case S_LOADING: draw_loading(); break;
    }
    gfx_fade(fade);
}

// ------------------------------------------------------------------ giochi esterni (PC)
// Cartella del gioco: <app>\<id>, <app>\<id>_mod, oppure accanto alla cartella dell'app (..\<id>, ..\<id>_mod)
static int find_external(game_t *g) {
#ifdef _WIN32
    static const char *fmt[] = { "%s/%s", "%s/%s_mod", "%s/../%s", "%s/../%s_mod" };
    // nomi di cartella: <id> (con le varianti di fmt), poi quelli di g->dirs in <app> e accanto ad essa
    const char *names[16]; char nbuf[16][256]; int nn = 0;
    names[nn++] = g->id;
    for (const char *n = g->dirs; n && *n && nn < 16; ) {
        const char *bar = strchr(n, '|');
        size_t l = bar ? (size_t)(bar - n) : strlen(n);
        if (l < sizeof nbuf[0]) { memcpy(nbuf[nn], n, l); nbuf[nn][l] = 0; names[nn] = nbuf[nn]; ++nn; }
        n = bar ? bar + 1 : n + l;
    }
    for (int i = 0; i < 4 + 2 * (nn - 1); ++i) {
        char d[1100], e[1300];
        if (i < 4) snprintf(d, sizeof d, fmt[i], basedir, g->id);
        else snprintf(d, sizeof d, (i - 4) % 2 ? "%s/../%s" : "%s/%s", basedir, names[1 + (i - 4) / 2]);
        FILE *f = NULL;
        for (const char *n = g->exe; n && *n; ) {
            const char *bar = strchr(n, '|');
            size_t l = bar ? (size_t)(bar - n) : strlen(n);
            if (l < sizeof g->ext_exe) {
                memcpy(g->ext_exe, n, l); g->ext_exe[l] = 0;
                snprintf(e, sizeof e, "%s/%s", d, g->ext_exe);
                if ((f = fopen(e, "rb")) != NULL) break;
            }
            n = bar ? bar + 1 : n + l;
        }
        if (!f) continue;
        fclose(f);
        wchar_t wd[1100], wf[1100];
        if (MultiByteToWideChar(CP_UTF8, 0, d, -1, wd, 1100) && GetFullPathNameW(wd, 1100, wf, NULL))
            WideCharToMultiByte(CP_UTF8, 0, wf, -1, g->ext_dir, sizeof g->ext_dir, NULL, NULL);
        else snprintf(g->ext_dir, sizeof g->ext_dir, "%s", d);
        dlogf("external game %s: %s\\%s", g->id, g->ext_dir, g->ext_exe);
        return 1;
    }
#else
    (void)g;
#endif
    return 0;
}

#ifdef _WIN32
static void force_foreground(HWND h) {
    if (!h) return;
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    if (GetForegroundWindow() == h) return;
    // Windows puo' negare il primo piano: si aggancia l'input al thread in primo piano
    HWND fg = GetForegroundWindow();
    DWORD ft = fg ? GetWindowThreadProcessId(fg, NULL) : 0, me = GetCurrentThreadId();
    if (ft && ft != me) AttachThreadInput(me, ft, TRUE);
    BringWindowToTop(h);
    SetForegroundWindow(h);
    SetFocus(h);
    if (ft && ft != me) AttachThreadInput(me, ft, FALSE);
}

// WM_CLOSE alle finestre principali del processo (MAME esce salvando cfg/nvram)
static BOOL CALLBACK close_pid_windows(HWND h, LPARAM pid) {
    DWORD p = 0; GetWindowThreadProcessId(h, &p);
    if (p == (DWORD)pid && IsWindowVisible(h) && !GetWindow(h, GW_OWNER)) PostMessageW(h, WM_CLOSE, 0, 0);
    return TRUE;
}
#endif

#ifdef _WIN32
// processi del gioco esterno (job) e loro finestre
#define WDA_EXCLUDEFROMCAPTURE_ 0x11
typedef BOOL (WINAPI *swda_fn)(HWND, DWORD);
static BOOL set_display_affinity(HWND h, DWORD a) {   // Windows 10 2004+ per WDA_EXCLUDEFROMCAPTURE
    static swda_fn f;
    if (!f) f = (swda_fn)(void*)GetProcAddress(GetModuleHandleA("user32.dll"), "SetWindowDisplayAffinity");
    return f ? f(h, a) : FALSE;
}
// processi del gioco = processo principale + tutti i discendenti (es. CF3Pad.exe -> CF3_Legacy_0.5.exe)
static int ext_job_pids(HANDLE job, DWORD main_pid, DWORD *out, int max) {
    (void)job;
    int n = 0;
    out[n++] = main_pid;
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (sn == INVALID_HANDLE_VALUE) return n;
    for (int added = 1; added && n < max; ) {   // si ripete finche' si trovano nuovi figli (nipoti, ...)
        added = 0;
        PROCESSENTRY32W pe; pe.dwSize = sizeof pe;
        for (BOOL ok = Process32FirstW(sn, &pe); ok && n < max; ok = Process32NextW(sn, &pe)) {
            int parent = 0, known = 0;
            for (int i = 0; i < n; ++i) { if (out[i] == pe.th32ParentProcessID) parent = 1; if (out[i] == pe.th32ProcessID) known = 1; }
            if (parent && !known && pe.th32ProcessID) { out[n++] = pe.th32ProcessID; added = 1; }
        }
    }
    CloseHandle(sn);
    return n;
}
typedef struct { DWORD pids[64]; int npids; HWND game, dialog; long long area; int close, dump; } extwin_t;
static BOOL CALLBACK ext_enum_cb(HWND h, LPARAM lp) {
    extwin_t *f = (extwin_t*)lp;
    DWORD p = 0; GetWindowThreadProcessId(h, &p);
    int mine = 0;
    for (int i = 0; i < f->npids; ++i) if (f->pids[i] == p) { mine = 1; break; }
    if (!mine) return TRUE;
    char cls[64] = ""; GetClassNameA(h, cls, sizeof cls);
    RECT r; GetWindowRect(h, &r);
    if (f->dump) dlogf("  window %p pid %lu class \"%s\" %ldx%ld visible %d owner %p", (void*)h, (unsigned long)p, cls,
        r.right - r.left, r.bottom - r.top, IsWindowVisible(h) ? 1 : 0, (void*)GetWindow(h, GW_OWNER));
    if (!IsWindowVisible(h)) return TRUE;
    // la finestra del proiettore Flash puo' avere un owner nascosto: le finestre con owner non vanno escluse
    if (f->close) { PostMessageW(h, WM_CLOSE, 0, 0); return TRUE; }
    if (!strcmp(cls, "#32770")) { if (!f->dialog) f->dialog = h; return TRUE; }   // finestra di dialogo
    long long a = (long long)(r.right - r.left) * (r.bottom - r.top);
    if (a > f->area) { f->area = a; f->game = h; }
    return TRUE;
}
static void ext_find_windows(HANDLE job, DWORD pid, extwin_t *f, int dump) {
    memset(f, 0, sizeof *f);
    f->npids = ext_job_pids(job, pid, f->pids, 64);
    f->dump = dump;
    if (dump) { char b[256] = ""; for (int i = 0; i < f->npids && i < 16; ++i) snprintf(b + strlen(b), sizeof b - strlen(b), " %lu", (unsigned long)f->pids[i]);
        dlogf("game processes:%s", b); }
    EnumWindows(ext_enum_cb, (LPARAM)f);
}
// WM_CLOSE alle finestre visibili di tutti i processi del gioco (es. CF3Pad.exe -> CF3_Legacy_0.5.exe)
static void ext_close_windows(HANDLE job, DWORD pid) {
    extwin_t f; memset(&f, 0, sizeof f);
    f.npids = ext_job_pids(job, pid, f.pids, 64);
    f.close = 1;
    EnumWindows(ext_enum_cb, (LPARAM)&f);
}
#endif

// Lancia il gioco esterno e attende che termini (anche gli eventuali processi figli), poi torna all'app
static void run_external(int gi) {
#ifdef _WIN32
    game_t *g = &games[gi];
    char exe[1200], cmd[1300];
    snprintf(exe, sizeof exe, "%s\\%s", g->ext_dir, g->ext_exe);
    snprintf(cmd, sizeof cmd, "\"%s\"", exe);
    if (g->args) {   // argomenti: {crt} = crtzero con SCANLINES attive (OPTIONS), altrimenti default
        const char *crt = gs.scanlines ? "crtzero" : "default", *a = g->args, *ph = strstr(a, "{crt}");
        size_t n = strlen(cmd);
        if (ph) snprintf(cmd + n, sizeof cmd - n, " %.*s%s%s", (int)(ph - a), a, crt, ph + 5);
        else snprintf(cmd + n, sizeof cmd - n, " %s", a);
        if (!GetEnvironmentVariableA("PSXHD", NULL, 0)) SetEnvironmentVariableA("PSXHD", "4");   // fattore HD di mamehd.exe
    }
    wchar_t wexe[1200], wcmd[1300], wdir[1100];
    MultiByteToWideChar(CP_UTF8, 0, exe, -1, wexe, 1200);
    MultiByteToWideChar(CP_UTF8, 0, cmd, -1, wcmd, 1300);
    MultiByteToWideChar(CP_UTF8, 0, g->ext_dir, -1, wdir, 1100);
    audio_pause(1);
    audio_clear();
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li;
        memset(&li, 0, sizeof li);
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_BREAKAWAY_OK;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof li);
    }
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);
    if (!CreateProcessW(wexe, wcmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, wdir, &si, &pi)) {
        DWORD err = GetLastError();
        dlogf("start of %s failed (error %lu)", exe, (unsigned long)err);
        snprintf(ext_msg, sizeof ext_msg, "CANNOT START %s (ERROR %lu)", g->ext_exe, (unsigned long)err);
        ext_msg_time = SDL_GetTicks();
        if (job) CloseHandle(job);
        return;
    }
    if (job && !AssignProcessToJobObject(job, pi.hProcess)) { CloseHandle(job); job = NULL; }
    dlogf("started %s (pid %lu)%s", cmd, (unsigned long)pi.dwProcessId, job ? "" : " without job");
    // gioco catturato: la finestra dell'app resta sopra il gioco, esclusa dalla cattura e senza focus
    SDL_SysWMinfo wmi; SDL_VERSION(&wmi.version);
    HWND self = SDL_GetWindowWMInfo(win, &wmi) ? wmi.info.win.window : NULL;
    int cap = 0; LONG self_ex = 0; RECT mon = { 0, 0, 0, 0 };
    if (g->capture && self) {
        char err[128] = "";
        if (!set_display_affinity(self, WDA_EXCLUDEFROMCAPTURE_)) dlogf("capture: WDA_EXCLUDEFROMCAPTURE not available (error %lu)", (unsigned long)GetLastError());
        else if (!extcap_open(self, err, sizeof err)) { dlogf("capture: %s", err); set_display_affinity(self, 0); }
        else {
            cap = 1;
            extcap_monitor(&mon);
            self_ex = GetWindowLongW(self, GWL_EXSTYLE);
            SetWindowLongW(self, GWL_EXSTYLE, self_ex | WS_EX_NOACTIVATE);
            SetWindowPos(self, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            gfx_set_native(g->nat_w, g->nat_h);
            gfx_set_crop(0, 0, 0, 0);
            apply_monitor();
            dlogf("capture on: monitor %ld,%ld %ldx%ld, screen %d monitor %d scanlines %d blur %d", mon.left, mon.top,
                mon.right - mon.left, mon.bottom - mon.top, eff_screen(), eff_monitor(), eff_scanlines(), eff_blur());
        }
    }
    if (!cap) SDL_HideWindow(win);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    Uint32 t0 = SDL_GetTicks(), combo_t = 0, t_scan = 0, t_seen = 0;
    int closing = 0, focused = 0, hidden = 0, fit_tries = 0, dumped = 0;
    Uint32 close_t = 0;
    HWND gw = NULL;
    for (;;) {
        DWORD r = WaitForSingleObject(pi.hProcess, cap ? 0 : 100);
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);   // gli ingressi durante il gioco non arrivano all'app
        // SELECT/BACK + START tenuti 1 s sul pad = chiusura del gioco (per i giochi senza EXIT, es. MAME)
        if (r != WAIT_OBJECT_0 && (g->args || g->backexit) && !closing) {
            // gioco catturato (Capoeira Fighter 3): BACK tenuto 3 s; MAME: SELECT/BACK + START tenuti 1 s
            if (!(g->backexit ? input_pad_back() : input_pad_back_start())) combo_t = 0;
            else if (!combo_t) combo_t = SDL_GetTicks();
            else if (SDL_GetTicks() - combo_t >= (g->backexit ? 3000u : 1000u)) {
                closing = 1; close_t = SDL_GetTicks();
                dlogf("%s: closing %s", g->backexit ? "BACK 3 s" : "SELECT+START", g->ext_exe);
                ext_close_windows(job, pi.dwProcessId);
            }
        }
        // gioco catturato che non si chiude con WM_CLOSE entro 4 s: chiusura forzata di tutti i suoi processi
        if (r != WAIT_OBJECT_0 && closing == 1 && g->backexit && SDL_GetTicks() - close_t >= 4000) {
            closing = 2;
            dlogf("forced close of %s", g->ext_exe);
            if (job) TerminateJobObject(job, 0); else TerminateProcess(pi.hProcess, 0);
        }
        if (cap && r != WAIT_OBJECT_0) {
            Uint32 tn = SDL_GetTicks();
            if (tn - t_scan >= 250) {   // finestra del gioco, dialoghi, primo piano e posizione sopra il gioco
                t_scan = tn;
                extwin_t fw; ext_find_windows(job, pi.dwProcessId, &fw, !gw && !dumped && tn - t0 > 3000);
                if (!gw && !dumped && tn - t0 > 3000) dumped = 1;
                if (fw.game != gw) { gw = fw.game; focused = 0; if (gw) dlogf("game window %p", (void*)gw); }
                if (gw) {
                    t_seen = tn;
                    // il gioco occupa tutto il monitor (senza bordi) cosi' la cattura e' alla risoluzione dello schermo
                    RECT wr; GetWindowRect(gw, &wr);
                    if (!IsIconic(gw) && memcmp(&wr, &mon, sizeof wr) && fit_tries < 20) {
                        ++fit_tries;
                        LONG st = GetWindowLongW(gw, GWL_STYLE);
                        SetWindowLongW(gw, GWL_STYLE, (st & ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) | WS_POPUP);
                        SetWindowPos(gw, NULL, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                        dlogf("game window %ldx%ld -> borderless fullscreen", wr.right - wr.left, wr.bottom - wr.top);
                    }
                    if (!focused) { force_foreground(gw); focused = 1; }
                }
                // un dialogo del gioco (errore, conferma) o nessuna finestra dopo 10 s: l'app si toglie di mezzo
                int hide = fw.dialog || (!gw && tn - t0 > 10000) || (gw && IsIconic(gw));
                if (hide != hidden) {
                    hidden = hide;
                    ShowWindow(self, hide ? SW_HIDE : SW_SHOWNOACTIVATE);
                    if (!hide) SetWindowPos(self, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                    if (hide && fw.dialog) force_foreground(fw.dialog);
                    dlogf("app window %s", hide ? "hidden" : "back above the game");
                }
                else if (!hidden) SetWindowPos(self, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            if (!hidden) {
                // area di gioco: stage 640x480 (proporzioni nat_w:nat_h) centrato nell'area client ("showAll")
                int ok = 0; unsigned tex = 0; int tw = 0, th = 0;
                if (gw && IsWindow(gw)) {
                    RECT cr; GetClientRect(gw, &cr);
                    POINT p0 = { 0, 0 }; ClientToScreen(gw, &p0);
                    int cw = cr.right, ch = cr.bottom;
                    if (cw > 0 && ch > 0) {
                        int sw = cw, sh = ch;
                        if ((long long)cw * g->nat_h > (long long)ch * g->nat_w) sw = (int)((long long)ch * g->nat_w / g->nat_h);
                        else sh = (int)((long long)cw * g->nat_h / g->nat_w);
                        RECT src = { p0.x + (cw - sw) / 2, p0.y + (ch - sh) / 2, 0, 0 };
                        src.right = src.left + sw; src.bottom = src.top + sh;
                        ok = extcap_frame(&src, &tex, &tw, &th);
                    }
                }
                int ww, wh; gfxb_drawable(&ww, &wh);
                gfx_frame_begin(ww, wh);
                if (ok) {
                    gfx_game_set_tex(tex, tw, th);
                    gfx_game_begin(eff_screen());
                    gfx_game_end(eff_scanlines(), eff_blur());
                }
                gfx_present();
                if (!gs.vsync || vsync_bad) SDL_Delay(8);
            }
            else SDL_Delay(50);
            (void)t_seen;
        }
        if (r != WAIT_OBJECT_0) continue;
        if (!job) break;
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION ai;
        if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation, &ai, sizeof ai, NULL) || ai.ActiveProcesses == 0) break;
        SDL_Delay(200);   // il processo principale e' uscito ma restano processi figli
    }
    if (cap) {
        gfx_game_set_tex(0, 0, 0);
        extcap_close();
        set_display_affinity(self, 0);
        SetWindowLongW(self, GWL_EXSTYLE, self_ex);
        SetWindowPos(self, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (hidden) ShowWindow(self, SW_SHOW);
        int ww, wh; gfxb_drawable(&ww, &wh);   // schermo nero fino al ritorno al menu
        gfx_frame_begin(ww, wh); gfx_present();
    }
    DWORD code = 0; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    if (job) CloseHandle(job);
    dlogf("%s ended (code %lu) after %u s", g->ext_exe, (unsigned long)code, (SDL_GetTicks() - t0) / 1000);
    // ritorno all'app: finestra di nuovo a schermo intero e in primo piano
    SDL_ShowWindow(win);
    SDL_RaiseWindow(win);
    { SDL_SysWMinfo wi; SDL_VERSION(&wi.version); if (SDL_GetWindowWMInfo(win, &wi)) force_foreground(wi.info.win.window); }
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    input_update(); input_update();   // tasti ancora premuti (es. A di conferma dell'uscita) non diventano pressioni nuove
    input_flush_start();
    last_counter = SDL_GetPerformanceCounter();
    vsync_bad = 0; dt_avg = 1.0 / 60.0;
#else
    (void)gi;
#endif
}

static void step_fade(double dt) {
    const float speed = (float)(dt / 0.14);   // ~140 ms
    if (fading_out) {
        fade += speed;
        if (fade >= 1.0f) {
            fade = 1.0f;
            fading_out = 0;
            int ns = next_state; next_state = -1;
            int from = state;
            if (from == S_PAUSE && ns == S_GAMES) stop_game();
            if (ns == S_LOADING) {
                if (!start_game()) { ns = S_GAMES; }
            }
            if (ns == S_EXTERNAL) {
                // schermo gia' nero: si lancia il gioco e si attende che termini (EXIT nel gioco)
                run_external(cur_game);
                ns = S_GAMES;
            }
            if (ns == S_GAMES) {
                // riposiziona sulla voce del gioco corrente
                for (int k = 0; k < nglist; ++k) if (glist[k] == cur_game) sel_game = k;
            }
            set_state_now(ns);
        }
    }
    else if (fade > 0.0f) {
        if (state == S_LOADING) fade -= speed;   // il caricamento appare
        else fade -= speed;
        if (fade < 0) fade = 0;
    }
}

// ------------------------------------------------------------------ main
static void load_assets(void) {
    char p[1400];
    snprintf(p, sizeof p, "%s/title.png", assetdir);
    gfx_tex_free(&t_title); gfx_tex_load(&t_title, p);
    snprintf(p, sizeof p, "%s/roms", basedir);
    neorom_scan(p);
    dlogf("romsets: %d zip files indexed in roms", neorom_zip_count());
    nglist = 0;
    for (int i = 0; i < NGAMES; ++i) {
        game_t *g = &games[i];
        rom_path_g(p, sizeof p, i);
        FILE *f = strncmp(p, "neorom:", 7) ? fopen(p, "rb") : NULL;
        if (!f && strncmp(p, "neorom:", 7)) { strncat(p, ".001", sizeof p - strlen(p) - 1); f = fopen(p, "rb"); }   // .neo diviso in parti
        g->available = f != NULL || !strncmp(p, "neorom:", 7);
        if (f) fclose(f);
        if (g->exe) g->available = find_external(g);
        if (!g->available) continue;
        glist[nglist++] = i;
        gfx_tex_free(&g->logo); gfx_tex_free(&g->logo_jp); gfx_tex_free(&g->bg);   // nuova ricerca (ROM CHECK)
        // immagini dell'utente / scaricate (artwork\) prima di quelle dell'app (assets\games\)
        snprintf(p, sizeof p, "%s/artwork/%s_logo.png", basedir, g->id);
        if (!gfx_tex_load(&g->logo, p)) { snprintf(p, sizeof p, "%s/games/%s_logo.png", assetdir, g->id); gfx_tex_load(&g->logo, p); }
        snprintf(p, sizeof p, "%s/games/%s_logo_jp.png", assetdir, g->id); gfx_tex_load(&g->logo_jp, p);
        snprintf(p, sizeof p, "%s/artwork/%s_bg.jpg", basedir, g->id);
        if (!gfx_tex_load_bg(&g->bg, p)) { snprintf(p, sizeof p, "%s/artwork/%s_bg.png", basedir, g->id);
            if (!gfx_tex_load_bg(&g->bg, p)) { snprintf(p, sizeof p, "%s/games/%s_bg.jpg", assetdir, g->id); gfx_tex_load_bg(&g->bg, p); } }
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FIGHTING COLLECTION", SDL_GetError(), NULL);
        return 1;
    }
    char *bp = SDL_GetBasePath();
    snprintf(basedir, sizeof basedir, "%s", bp ? bp : ".");
    SDL_free(bp);
    size_t bl = strlen(basedir);
    if (bl && (basedir[bl - 1] == '/' || basedir[bl - 1] == '\\')) basedir[bl - 1] = 0;
    snprintf(assetdir, sizeof assetdir, "%s/assets", basedir);
    { char gd[1200]; snprintf(gd, sizeof gd, "%s/games", basedir); MKDIR(gd); }
    // mappature dei controller aggiuntive (opzionale)
    { char db[1200]; snprintf(db, sizeof db, "%s/gamecontrollerdb.txt", basedir); SDL_GameControllerAddMappingsFromFile(db); }

    { char lp[1200]; snprintf(lp, sizeof lp, "%s/fightingcollection.log", basedir); dlog = fopen(lp, "w"); }
    { SDL_version v; SDL_GetVersion(&v); dlogf("FIGHTING COLLECTION start, SDL %d.%d.%d, displays %d", v.major, v.minor, v.patch, SDL_GetNumVideoDisplays()); }
    { SDL_DisplayMode dm; if (SDL_GetDesktopDisplayMode(0, &dm) == 0) dlogf("desktop %dx%d @%d Hz", dm.w, dm.h, dm.refresh_rate);
      float dd, hd2, vd; if (SDL_GetDisplayDPI(0, &dd, &hd2, &vd) == 0) dlogf("DPI %.1f", dd); }
    settings_load(basedir, &gs);
    apply_monitor();
    gs.video = VID_FULLSCREEN;   // solo schermo intero
    dlogf("settings: video %d, %dx%d, vsync %d, scanlines %d, blur %d, overclock %d", gs.video, gs.win_w, gs.win_h, gs.vsync, gs.scanlines, gs.blur, gs.overclock);
    // se l'ultima esecuzione non si e' chiusa normalmente, si riparte in borderless
    char lockp[1200];
    snprintf(lockp, sizeof lockp, "%s/running.lock", basedir);
    { FILE *lf = fopen(lockp, "wb"); if (lf) fclose(lf); }
    build_res_list();

    Uint32 flags = gfxb_window_flags() | SDL_WINDOW_ALLOW_HIGHDPI;
    int cw = gs.win_w, ch = gs.win_h;
    {
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { cw = dm.w; ch = dm.h; }
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;   // schermo intero alla risoluzione del desktop
    }
    win = SDL_CreateWindow("FIGHTING COLLECTION", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cw, ch, flags);
    if (!win) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FIGHTING COLLECTION", SDL_GetError(), NULL);
        return 1;
    }
    char ginfo[256] = "";
    if (!gfxb_init(win, gs.vsync, ginfo, sizeof ginfo)) {
        dlogf("graphics not initialised: %s", ginfo);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FIGHTING COLLECTION", ginfo[0] ? ginfo : "Graphics init failed.", win);
        return 1;
    }
    dlogf("graphics: %s", ginfo);
    SDL_ShowCursor(SDL_DISABLE);
    if (!gfx_init(assetdir)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FIGHTING COLLECTION", "Missing assets folder.", win);
        return 1;
    }
    load_assets();
    input_init();
    audio_init();
    audio_pause(1);

    set_state_now(S_SPLASH);
    fade = 1.0f;
    last_counter = SDL_GetPerformanceCounter();
    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_WINDOWEVENT) {
                int dw, dh; gfxb_drawable(&dw, &dh);
                dlogf("window event %d (%d,%d) drawable %dx%d", e.window.event, e.window.data1, e.window.data2, dw, dh);
            }
            if (e.type == SDL_DISPLAYEVENT) dlogf("display event %d", e.display.event);
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) { vsync_bad = 0; dt_avg = 1.0 / 60.0; }
            input_handle_event(&e);
        }
        {
            static Uint32 last_stat; static unsigned loops, last_fc;
            ++loops;
            Uint32 tnow = SDL_GetTicks();
            if (tnow - last_stat >= 1000) {
                int dw, dh, ww2, wh2; gfxb_drawable(&dw, &dh); SDL_GetWindowSize(win, &ww2, &wh2);
                dlogf("stat %s: loop/s %u, emu frame/s %u, emu ms %.2f, HD %u, win %dx%d, drawable %dx%d, refresh %.1f, lock %d, cycle %.1f ms, flags %08x",
                    state_name(state), loops, frame_count - last_fc, emu_time_avg, emu_hd_w(), ww2, wh2, dw, dh,
                    display_refresh(), vsync_locked(), dt_avg * 1000.0, SDL_GetWindowFlags(win));
#ifdef _WIN32
                { PROCESS_MEMORY_COUNTERS pmc; MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
                  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc) && GlobalMemoryStatusEx(&ms))
                      dlogf("  memory: app %u MB, free %u MB of %u MB", (unsigned)(pmc.WorkingSetSize >> 20),
                          (unsigned)(ms.ullAvailPhys >> 20), (unsigned)(ms.ullTotalPhys >> 20)); }
#endif
                if (state == S_GAME || state == S_PAUSE)
                    dlogf("  times/s (ms): emu %.0f, audio %.0f, upload %.0f, draw %.0f, swap %.0f", t_emu, t_audio, t_upload, t_draw, t_swap);
                t_emu = t_audio = t_upload = t_draw = t_swap = 0;
                loops = 0; last_fc = frame_count; last_stat = tnow;
            }
        }
        input_update();
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last_counter) / SDL_GetPerformanceFrequency();
        last_counter = now;
        if (dt > 0.1) dt = 0.1;
        if (gs.vsync == 0) vsync_bad = 0;
        audio_set_input_rate(emu_audio_rate() * (vsync_locked() ? display_refresh() / emu_fps() : 1.0));
        update(dt);
        ignore_input = ignore_input > 0 ? ignore_input - 1 : 0;
        step_fade(dt);
        render();
        double tsw = now_ms();
        gfx_present();
        t_swap += now_ms() - tsw;
        dt_avg = dt_avg * 0.95 + dt * 0.05;
        // V-SYNC: il Present blocca fino al refresh (1 frame emulato per refresh).
        // Se il driver lo ignora (ciclo molto piu' veloce del refresh) l'app tiene il ritmo da sola.
        if (gs.vsync && !vsync_bad && dt_avg < 0.012 && SDL_GetTicks() > 3000) {
            vsync_bad = 1;
            dlogf("V-SYNC not effective (cycle %.1f ms): pacing done by the app", dt_avg * 1000.0);
        }
        if (!gs.vsync || vsync_bad) {
            double target = gs.vsync ? 1.0 / display_refresh() : 1.0 / emu_fps();
            Uint64 f = SDL_GetPerformanceFrequency();
            for (;;) {
                double el = (double)(SDL_GetPerformanceCounter() - now) / f;
                if (el >= target) break;
                if (target - el > 0.002) SDL_Delay(1);
            }
        }
    }
    if (state == S_GAME || state == S_PAUSE || state == S_LOADING || (state == S_INPUT && input_from_pause)) stop_game();
    settings_save(basedir, &gs);
    audio_quit();
    gfxb_quit();
    SDL_DestroyWindow(win);
    SDL_Quit();
    remove(lockp);
    return 0;
}
