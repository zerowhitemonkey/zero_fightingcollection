// SNKHD - banco di prova headless Psikyo (Daraku Tenshi)
// uso: snkhd_psikyo <zip> <gioco daraku> <regione 0/1> <frames> <kx> <ky> <outprefix> [script]
// script (una riga per comando):
//   in frame port joy sys     ingressi (bitmask PSK_*, valgono dal frame indicato)
//   shot frame                outprefix_<frame>.png (HD) e _<frame>_r.png (riferimento 320x224)
//   ram frame addr len        stampa la RAM SH-2
//   ee frame                  stampa la EEPROM (0x100 byte)
//   eew frame addr val        scrive un byte della EEPROM
//   w8 frame addr val         scrive un byte di RAM
//   save frame file / load frame file   stato
//   p16 frame addr orig val   patch di una parola del codice (fetch)
//   cmp frame                 confronta HD (deve essere kx=ky=1) con il riferimento
// variabili: OC=percento CPU, WATCH=lo-hi (scritture RAM), FAST=1 (senza render tranne gli shot)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ext/stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../psikyo/psikyo.h"

typedef struct { int frame; char cmd[8]; unsigned a, b, c; char s[256]; } cmd_t;
static cmd_t cmds[200000]; static int ncmd;

static void save_png(const char *path, const uint32_t *px, unsigned w, unsigned h) {
    unsigned char *rgb = malloc((size_t)w * h * 3);
    for (size_t i = 0; i < (size_t)w * h; ++i) { rgb[i*3] = px[i] >> 16; rgb[i*3+1] = px[i] >> 8; rgb[i*3+2] = px[i]; }
    stbi_write_png(path, w, h, 3, rgb, w * 3);
    free(rgb);
}

int main(int argc, char **argv) {
    if (argc < 8) { fprintf(stderr, "uso: snkhd_psikyo zip gioco regione frames kx ky out [script]\n"); return 1; }
    int frames = atoi(argv[4]), kx = atoi(argv[5]), ky = atoi(argv[6]);
    const char *out = argv[7];
    if (getenv("WATCHV")) { extern void psikyo_set_watch_value(int); psikyo_set_watch_value((int)strtol(getenv("WATCHV"), NULL, 16)); }
    if (getenv("WATCH")) { unsigned lo, hi; if (sscanf(getenv("WATCH"), "%x-%x", &lo, &hi) == 2) psikyo_set_watch(lo, hi); }
    if (!psikyo_load_zip(argv[1], argv[2], (int)strtol(argv[3], NULL, 0))) { fprintf(stderr, "caricamento fallito\n"); return 1; }
    if (getenv("OC")) psikyo_set_overclock(atoi(getenv("OC")));
    if (getenv("THREADS")) { extern void psikyo_set_threads(int); psikyo_set_threads(atoi(getenv("THREADS"))); }
    if (getenv("NOIDLE")) { extern void psikyo_set_idle_skip(int); psikyo_set_idle_skip(0); }
    if (getenv("SHADOWS")) psikyo_set_shadows(atoi(getenv("SHADOWS")));
    if (getenv("PCMAP")) { extern unsigned char *sh2_pcmap_rom; sh2_pcmap_rom = calloc(0x10000, 1); }
    if (getenv("SPRWATCH")) {   // SPRWATCH=mask,val[,lo-hi]
        extern void psikyo_set_sprwatch(uint32_t, uint32_t, uint32_t, uint32_t);
        unsigned m = 0, v = 0, lo = 1, hi = 0; sscanf(getenv("SPRWATCH"), "%x,%x,%x-%x", &m, &v, &lo, &hi);
        psikyo_set_sprwatch(m, v, lo, hi);
    }
    if (getenv("MVID")) {   // MVID=prev.bin,cur.bin,out : rende la RAM video di MAME (out_r.png 1x, out_hd.png 4x)
        extern void psikyo_debug_load_video(const uint8_t *, const uint8_t *);
        char pf[512], cf[512], of[512];
        if (sscanf(getenv("MVID"), "%511[^,],%511[^,],%511s", pf, cf, of) != 3) return 1;
        static uint8_t pb[0x15220], cb[0x15220];
        FILE *f = fopen(pf, "rb"); if (!f || fread(pb, 1, sizeof pb, f) != sizeof pb) return 1; fclose(f);
        f = fopen(cf, "rb"); if (!f || fread(cb, 1, sizeof cb, f) != sizeof cb) return 1; fclose(f);
        psikyo_debug_load_video(pb, cb);
        uint32_t *r = calloc(PSIKYO_W * PSIKYO_H, 4); char p[600];
        psikyo_render_reference(r); snprintf(p, sizeof p, "%s_r.png", of); save_png(p, r, PSIKYO_W, PSIKYO_H);
        unsigned k = getenv("MVK") ? atoi(getenv("MVK")) : 4, W = PSIKYO_W * k, H = PSIKYO_H * k;
        uint32_t *h = calloc((size_t)W * H, 4); psikyo_set_hd(h, W, H);
        extern void psikyo_debug_render_hd(void); psikyo_debug_render_hd();
        snprintf(p, sizeof p, "%s_hd.png", of); save_png(p, h, W, H);
        return 0;
    }
    unsigned W = PSIKYO_W * kx, H = PSIKYO_H * ky;
    uint32_t *hd = calloc((size_t)W * H, 4), *ref = calloc(PSIKYO_W * PSIKYO_H, 4);
    psikyo_set_hd(hd, W, H);
    if (argc > 8) {
        FILE *f = fopen(argv[8], "r");
        char l[512];
        while (f && fgets(l, sizeof l, f) && ncmd < 200000) {
            cmd_t *c = &cmds[ncmd];
            memset(c, 0, sizeof *c);
            char a1[64] = "", a2[64] = "", a3[64] = "";
            int n = sscanf(l, "%7s %d %63s %63s %63s", c->cmd, &c->frame, a1, a2, a3);
            if (n < 2 || c->cmd[0] == '#') continue;
            c->a = (unsigned)strtoul(a1, NULL, 0); c->b = (unsigned)strtoul(a2, NULL, 0); c->c = (unsigned)strtoul(a3, NULL, 0);
            snprintf(c->s, sizeof c->s, "%s", a1);
            ncmd++;
        }
        if (f) fclose(f);
    }
    unsigned joy[2] = { 0, 0 }, sys[2] = { 0, 0 };
    int fast = getenv("FAST") != NULL;
    clock_t t0 = clock();
    long asum = 0;
    for (int fr = 0; fr < frames; ++fr) {
        int want_shot = 0, want_cmp = 0;
        for (int i = 0; i < ncmd; ++i) {
            cmd_t *c = &cmds[i];
            if (c->frame != fr) continue;
            if (!strcmp(c->cmd, "in")) { joy[c->a & 1] = c->b; sys[c->a & 1] = c->c; }
            else if (!strcmp(c->cmd, "shot")) want_shot = 1;
            else if (!strcmp(c->cmd, "cmp")) want_cmp = 1;
            else if (!strcmp(c->cmd, "w8")) psikyo_write8(c->a, (uint8_t)c->b);
            else if (!strcmp(c->cmd, "eew")) psikyo_eeprom_wr((int)c->a, (uint16_t)c->b);
            else if (!strcmp(c->cmd, "p16")) { if (!psikyo_patch16(c->a, (uint16_t)c->b, (uint16_t)c->c)) fprintf(stderr, "p16 %08x: byte diversi\n", c->a); }
            else if (!strcmp(c->cmd, "load")) {
                FILE *f = fopen(c->s, "rb"); if (!f) { fprintf(stderr, "stato %s mancante\n", c->s); continue; }
                size_t n = psikyo_state_size(); void *b = malloc(n);
                if (fread(b, 1, n, f) == n) psikyo_state_load(b, n);
                fclose(f); free(b);
            }
        }
        for (int p = 0; p < 2; ++p) psikyo_set_input(p, joy[p], sys[p]);
        psikyo_frame(!fast || want_shot || want_cmp);
        { size_t na; const int16_t *ab = psikyo_audio(&na); asum += (long)na;
          static FILE *wf; if (!wf && getenv("WAV")) wf = fopen(getenv("WAV"), "wb");
          if (wf) fwrite(ab, 4, na, wf); }
        { static int se = -1; if (se < 0) se = getenv("SHOTEVERY") ? atoi(getenv("SHOTEVERY")) : 0;
          if (se > 0 && fr % se == 0) want_shot = 1; }
        if (want_shot) {
            char p[600];
            snprintf(p, sizeof p, "%s_%d.png", out, fr); save_png(p, hd, W, H);
            psikyo_render_reference(ref);
            snprintf(p, sizeof p, "%s_%d_r.png", out, fr); save_png(p, ref, PSIKYO_W, PSIKYO_H);
        }
        if (want_cmp) {
            psikyo_render_reference(ref);
            long diff = 0;
            for (unsigned y = 0; y < PSIKYO_H && kx == 1 && ky == 1; ++y)
                for (unsigned x = 0; x < PSIKYO_W; ++x) if ((hd[y * W + x] & 0xffffff) != (ref[y * PSIKYO_W + x] & 0xffffff)) diff++;
            printf("cmp %d: diff %ld\n", fr, diff);
        }
        for (int i = 0; i < ncmd; ++i) {
            cmd_t *c = &cmds[i];
            if (c->frame != fr) continue;
            if (!strcmp(c->cmd, "ram")) {
                printf("ram %d %08x:", fr, c->a);
                for (unsigned k = 0; k < c->b; ++k) printf("%s%02x", (k % 16) ? " " : "\n  ", psikyo_read8(c->a + k));
                printf("\n");
            } else if (!strcmp(c->cmd, "ee")) {
                printf("ee %d:", fr);
                for (int k = 0; k < 0x100; ++k) printf("%s%02x", (k % 16) ? " " : "\n  ", (unsigned)psikyo_eeprom_rd(k));
                printf("\n");
            } else if (!strcmp(c->cmd, "save")) {
                size_t n = psikyo_state_size(); void *b = malloc(n);
                psikyo_state_save(b, n);
                FILE *f = fopen(c->s, "wb"); if (f) { fwrite(b, 1, n, f); fclose(f); }
                free(b);
            } else if (!strcmp(c->cmd, "vid")) {
                printf("vid %d:", fr);
                for (int k = 0; k < 8; ++k) printf(" r%d=%08x", k, psikyo_vidreg(k));
                printf("\n");
            } else if (!strcmp(c->cmd, "pal")) {
                const uint32_t *p = psikyo_palette32();
                printf("pal %d [%u..%u]:", fr, c->a, c->b);
                for (unsigned k = c->a; k <= c->b; ++k) printf(" %06x", p[k] & 0xffffff);
                printf("\n");
            } else if (!strcmp(c->cmd, "prog")) {
                extern const uint8_t *psikyo_prog(void);
                const uint8_t *pg = psikyo_prog();
                printf("prog %d [0x%x..0x%x]:", fr, c->a, c->b);
                for (unsigned k = c->a; k <= c->b; ++k) printf(" %02x", pg[k]);
                printf("\n");
            } else if (!strcmp(c->cmd, "pcmap")) {   // pcmap frame file: PC in ROM eseguiti nel frame (bit per istruzione), poi azzera
                extern unsigned char *sh2_pcmap_rom;
                if (sh2_pcmap_rom) { FILE *f = fopen(c->s, "wb"); if (f) { fwrite(sh2_pcmap_rom, 1, 0x10000, f); fclose(f); } memset(sh2_pcmap_rom, 0, 0x10000); }
            } else if (!strcmp(c->cmd, "dump")) {   // dump frame file: RAM SH-2 (1 MB, big-endian)
                FILE *f = fopen(c->s, "wb");
                for (unsigned k = 0; f && k < 0x100000; ++k) fputc(psikyo_read8(0x06000000 + k), f);
                if (f) fclose(f);
            } else if (!strcmp(c->cmd, "vram")) {
                const uint32_t *v = psikyo_sprram(); char p[600]; snprintf(p, sizeof p, "%s_%d.vram", out, fr);
                FILE *f = fopen(p, "wb"); if (f) { fwrite(v, 4, 0x4000, f); fwrite(psikyo_spritebuf(), 4, 0x1000, f);
                    for (int k = 0; k < 8; ++k) { uint32_t r = psikyo_vidreg(k); fwrite(&r, 4, 1, f); } fclose(f); }
            } else if (!strcmp(c->cmd, "rbench")) {   // rbench frame N: tempo del renderer HD (N chiamate)
                extern void psikyo_debug_render_hd(void); { extern double psk_prof[8]; extern int psk_prof_on; psk_prof_on = 1; for (int q = 0; q < 6; ++q) psk_prof[q] = 0; }
                struct timespec ta, tb; clock_gettime(CLOCK_MONOTONIC, &ta);
                for (unsigned k = 0; k < c->a; ++k) psikyo_debug_render_hd();
                clock_gettime(CLOCK_MONOTONIC, &tb);
                { extern double psk_prof[8]; extern int psk_prof_on; psk_prof_on = 0; for (int q = 0; q < 6; ++q) { printf(" p%d %.3f", q, psk_prof[q] / c->a); psk_prof[q] = 0; } }
                printf("rbench %d: %.3f ms/frame (%ux%u)\n", fr, ((tb.tv_sec - ta.tv_sec) * 1e3 + (tb.tv_nsec - ta.tv_nsec) / 1e6) / (c->a ? c->a : 1), W, H);
            } else if (!strcmp(c->cmd, "rawhd")) {   // rawhd frame file: immagine HD grezza (confronti)
                extern void psikyo_debug_render_hd(void); psikyo_debug_render_hd();
                FILE *f = fopen(c->s, "wb"); if (f) { fwrite(hd, 4, (size_t)W * H, f); fclose(f); }
            } else if (!strcmp(c->cmd, "sprb")) {
                const uint32_t *s = psikyo_spritebuf();
                printf("sprb %d (0x%x..0x%x long):", fr, c->a, c->b);
                for (unsigned k = c->a; k <= c->b; ++k) printf(" %08x", s[k]);
                printf("\n");
            }
        }
    }
    double el = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "%d frame in %.2f s (%.2f ms/frame), audio %ld campioni (%.1f/frame), pc %08x\n",
            frames, el, el * 1000.0 / (frames ? frames : 1), asum, (double)asum / (frames ? frames : 1), psikyo_pc());
    psikyo_unload();
    return 0;
}
