// SNKHD - banco di prova headless PGM (Martial Masters)
// uso: snkhd_pgm <zip> <bios.zip|-> <regione> <frames> <kx> <ky> <outprefix> [script]
// script (una riga per comando):
//   in frame port joy sys     ingressi (bitmask PGM_*, valgono dal frame indicato)
//   shot frame                outprefix_<frame>.png (HD) e _<frame>_r.png (riferimento 448x224)
//   ram frame addr len        stampa la memoria del 68000
//   w8 frame addr val         scrive un byte di RAM
//   save frame file / load frame file   stato
//   p16 frame addr orig val   patch di una parola della ROM del 68000
//   spr frame                 elenco sprite (lista bufferizzata)
//   dip frame val             DIP A
// variabili: OC=percento CPU, FAST=1 (render solo negli shot), SHADOWS=0-2, SHPAL=palette ombre,
//            RAND=seme (ingressi casuali P1/P2 dal frame RANDFROM), START=1 (START P1 ogni 30 frame finche' RAND)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ext/stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../pgm/pgmcore.h"

typedef struct { int frame; char cmd[8]; unsigned a, b, c; char s[256]; } cmd_t;
static cmd_t cmds[200000]; static int ncmd;

static void save_png(const char *path, const uint32_t *px, unsigned w, unsigned h) {
    unsigned char *rgb = malloc((size_t)w * h * 3);
    for (size_t i = 0; i < (size_t)w * h; ++i) { rgb[i*3] = px[i] >> 16; rgb[i*3+1] = px[i] >> 8; rgb[i*3+2] = px[i]; }
    stbi_write_png(path, w, h, 3, rgb, w * 3);
    free(rgb);
}
static unsigned rnd_state = 1;
static unsigned rnd(void) { rnd_state = rnd_state * 1103515245u + 12345u; return rnd_state >> 16; }

int main(int argc, char **argv) {
    if (argc < 8) { fprintf(stderr, "uso: snkhd_pgm zip bios regione frames kx ky out [script]\n"); return 1; }
    int frames = atoi(argv[4]), kx = atoi(argv[5]), ky = atoi(argv[6]);
    const char *out = argv[7];
    const char *bios = strcmp(argv[2], "-") ? argv[2] : NULL;
    clock_t t0 = clock();
    if (!pgm_load_zip(argv[1], bios, "martmast", (int)strtol(argv[3], NULL, 0))) { fprintf(stderr, "caricamento fallito\n"); return 1; }
    fprintf(stderr, "caricato in %.2f s\n", (double)(clock() - t0) / CLOCKS_PER_SEC);
    if (getenv("DUMPROM")) {   // ROM del 68000 in big-endian (offset = indirizzo - 0x100000)
        size_t n; const uint8_t *r = pgm_rom68k(&n); FILE *f = fopen(getenv("DUMPROM"), "wb");
        for (size_t i = 0; i < n; i += 2) { fputc(r[i + 1], f); fputc(r[i], f); }
        fclose(f);
    }
    if (getenv("WATCH")) { unsigned lo, hi; if (sscanf(getenv("WATCH"), "%x-%x", &lo, &hi) == 2) pgm_set_watch(lo, hi); }
    if (getenv("WATCHV")) pgm_set_watch_value((uint32_t)strtoul(getenv("WATCHV"), NULL, 16));
    if (getenv("OC")) pgm_set_overclock(atoi(getenv("OC")));
    if (getenv("SHADOWS")) pgm_set_shadows(atoi(getenv("SHADOWS")));
    if (getenv("SHSPR")) { unsigned lo, hi; int h = -1; sscanf(getenv("SHSPR"), "%x-%x,%d", &lo, &hi, &h); pgm_set_shadow_sprites(lo, hi, h); }
    if (getenv("DIP")) pgm_set_dip((unsigned)strtol(getenv("DIP"), NULL, 0));
    int fast = getenv("FAST") != NULL;
    int rseed = getenv("RAND") ? atoi(getenv("RAND")) : 0;
    int rfrom = getenv("RANDFROM") ? atoi(getenv("RANDFROM")) : 0;
    if (rseed) rnd_state = rseed;
    if (argc > 8) {
        FILE *f = fopen(argv[8], "r");
        char line[512];
        while (f && fgets(line, sizeof line, f) && ncmd < 200000) {
            cmd_t *c = &cmds[ncmd];
            memset(c, 0, sizeof *c);
            if (sscanf(line, "%7s %d", c->cmd, &c->frame) < 2) continue;
            char *p = line; int skip = 2;
            while (skip && *p) { while (*p && *p != ' ') ++p; while (*p == ' ') ++p; --skip; }
            if (!strcmp(c->cmd, "save") || !strcmp(c->cmd, "load") || !strcmp(c->cmd, "dump")) sscanf(p, "%255s", c->s);
            else sscanf(p, "%x %x %x", &c->a, &c->b, &c->c);
            if (!strcmp(c->cmd, "in")) sscanf(p, "%u %x %x", &c->a, &c->b, &c->c);
            ++ncmd;
        }
        if (f) fclose(f);
    }
    unsigned W = PGM_W * kx, H = PGM_H * ky;
    uint32_t *hd = calloc((size_t)W * H, 4), *ref = calloc(PGM_W * PGM_H, 4);
    pgm_set_hd(hd, W, H);
    unsigned joy[2] = {0, 0}, sys[2] = {0, 0};
    double tsum = 0, tmax = 0;
    int slog_lo = 1, slog_hi = 0; FILE *slog = NULL;
    if (getenv("SPRLOG")) { sscanf(getenv("SPRLOG"), "%d-%d", &slog_lo, &slog_hi); slog = fopen(getenv("SPRLOGF") ? getenv("SPRLOGF") : "sprlog.txt", "w"); }
    for (int fr = 0; fr <= frames; ++fr) {
        int shot = 0;
        for (int i = 0; i < ncmd; ++i) {
            cmd_t *c = &cmds[i];
            if (c->frame != fr) continue;
            if (!strcmp(c->cmd, "in")) { joy[c->a & 1] = c->b; sys[c->a & 1] = c->c; }
            else if (!strcmp(c->cmd, "shot")) shot = 1;
            else if (!strcmp(c->cmd, "dip")) pgm_set_dip(c->a);
            else if (!strcmp(c->cmd, "w8")) pgm_write8(c->a, (uint8_t)c->b);
            else if (!strcmp(c->cmd, "p16")) printf("p16 %06x: %d\n", c->a, pgm_patch16(c->a, (uint16_t)c->b, (uint16_t)c->c));
            else if (!strcmp(c->cmd, "ram")) {
                printf("ram %d %06x:", fr, c->a);
                for (unsigned k = 0; k < c->b; ++k) printf("%s%02x", (k & 15) ? " " : "\n  ", pgm_read8(c->a + k));
                printf("\n");
            } else if (!strcmp(c->cmd, "spr")) {
                const uint16_t *s = pgm_spritebuf();
                printf("spr %d\n", fr);
                for (int k = 0; k < 256; ++k, s += 8) {
                    if ((s[4] & 0x7fff) == 0) break;
                    int x = s[0] & 0x7ff, y = s[1] & 0x3ff; if (x > 0x3ff) x -= 0x800; if (y > 0x1ff) y -= 0x400;
                    printf("  %3d x%4d y%4d pal%02x pri%d flip%d boff%06x w%2d h%3d z%04x/%04x\n", k, x, y, (s[2] >> 8) & 0x1f, (s[2] >> 7) & 1,
                           (s[2] >> 13) & 3, ((s[2] & 0x7f) << 16) | s[3], (s[4] >> 9) & 0x3f, s[4] & 0x1ff, s[0] & 0xf800, s[1] & 0xf800);
                }
            } else if (!strcmp(c->cmd, "dump")) {   // RAM del 68000 (0x800000-0x81ffff) in big-endian
                FILE *f = fopen(c->s, "wb"); for (unsigned k = 0; k < 0x20000; ++k) fputc(pgm_read8(0x800000 + k), f); fclose(f);
            } else if (!strcmp(c->cmd, "save")) {
                size_t n = pgm_state_size(); void *b = malloc(n); pgm_state_save(b, n);
                FILE *f = fopen(c->s, "wb"); if (f) { fwrite(b, 1, n, f); fclose(f); } free(b);
            } else if (!strcmp(c->cmd, "load")) {
                size_t n = pgm_state_size(); void *b = malloc(n);
                FILE *f = fopen(c->s, "rb"); if (f) { size_t r = fread(b, 1, n, f); (void)r; fclose(f); pgm_state_load(b, n); } free(b);
            }
        }
        pgm_watch_frame(fr);
        unsigned j0 = joy[0], j1 = joy[1], s0 = sys[0], s1 = sys[1];
        if (rseed && fr >= rfrom) {
            static unsigned rj[2]; if (fr % 6 == 0) { rj[0] = rnd() & 0xff; rj[1] = rnd() & 0xff; }
            j0 |= rj[0]; j1 |= rj[1];
            if (getenv("START") && fr % 30 < 3) { s0 |= PGM_START; if (atoi(getenv("START")) > 1) s1 |= PGM_START; }
        }
        if (getenv("VSAUTO")) {   // come auto_start dell'app: START a impulsi per chi non e' in partita
            int vs = atoi(getenv("VSAUTO")) > 1;
            for (int p = 0; p < (vs ? 2 : 1); ++p)
                if (!(pgm_read8(0x81b528 + p * 0x1e) & 0x20) && fr % 16 < 4) { if (p) s1 |= PGM_START; else s0 |= PGM_START; }
        }
        pgm_set_input(0, j0, s0); pgm_set_input(1, j1, s1);
        clock_t a = clock();
        pgm_frame(!fast || shot);
        double ms = (double)(clock() - a) * 1000.0 / CLOCKS_PER_SEC;
        tsum += ms; if (ms > tmax) tmax = ms;
        if (getenv("LOGB")) {   // LOGB=a1,a2,... stampa i byte quando cambiano
            static unsigned la[16]; static int nla = -1; static int prev[16];
            if (nla < 0) { nla = 0; char *q = getenv("LOGB"); while (*q && nla < 16) { la[nla] = strtoul(q, &q, 16); prev[nla] = -1; ++nla; if (*q == ',') ++q; } }
            int ch = 0; for (int k = 0; k < nla; ++k) if (pgm_read8(la[k]) != prev[k]) ch = 1;
            if (ch) { printf("B %d", fr); for (int k = 0; k < nla; ++k) { prev[k] = pgm_read8(la[k]); printf(" %02x", prev[k]); } printf("\n"); }
        }
        if (slog_lo <= fr && fr <= slog_hi) {
            const uint16_t *sp = pgm_spritebuf();
            fprintf(slog, "F %d", fr);
            for (int k = 0; k < 256; ++k, sp += 8) {
                if ((sp[4] & 0x7fff) == 0) break;
                int x = sp[0] & 0x7ff, y = sp[1] & 0x3ff; if (x > 0x3ff) x -= 0x800; if (y > 0x1ff) y -= 0x400;
                fprintf(slog, " %x:%x:%d:%d:%d:%d", ((sp[2] & 0x7f) << 16) | sp[3], (sp[2] >> 8) & 0x1f, x, y, (sp[4] >> 9) & 0x3f, sp[4] & 0x1ff);
            }
            fprintf(slog, "\n");
        }
        if (shot) {
            char p[600];
            snprintf(p, sizeof p, "%s_%06d.png", out, fr); save_png(p, hd, W, H);
            if (getenv("HIDE")) { unsigned lo, hi; sscanf(getenv("HIDE"), "%x-%x", &lo, &hi); pgm_debug_hide(lo, hi); }
            pgm_render_reference(ref);
            pgm_debug_hide(1, 0);
            snprintf(p, sizeof p, "%s_%06d_r.png", out, fr); save_png(p, ref, PGM_W, PGM_H);
        }
    }
    fprintf(stderr, "frame: media %.2f ms, max %.2f ms\n", tsum / (frames + 1), tmax);
    return 0;
}
