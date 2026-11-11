// SNKHD - banco di prova headless CPS3
// uso: snkhd_cps3 <zip> <gioco> <regione hex> <frames> <kx> <ky> <outprefix> [script]
// script (una riga per comando):
//   in frame port joy sys     ingressi (bitmask C3_*, valgono dal frame indicato)
//   shot frame                outprefix_<frame>.png (HD) e _<frame>_r.png (riferimento FBNeo 384x224)
//   ram frame addr len        stampa la RAM SH-2
//   ee frame                  stampa la EEPROM (0x40 parole)
//   eew frame word val        scrive una parola della EEPROM
//   w8 frame addr val         scrive un byte di RAM
//   save frame file / load frame file   stato
//   spr frame                 stampa la lista sprite (voci principali)
//   p16 frame addr orig val   patch di una parola del codice (fetch)
//   cmp frame                 confronta HD (deve essere kx=ky=1) con il riferimento
// variabili: OC=percento CPU, WATCH=lo-hi (scritture RAM), FAST=1 (senza render tranne gli shot)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ext/stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../cps3/cps3.h"

typedef struct { int frame; char cmd[8]; unsigned a, b, c; char s[256]; } cmd_t;
static cmd_t cmds[200000]; static int ncmd;

static void save_png(const char *path, const uint32_t *px, unsigned w, unsigned h) {
    unsigned char *rgb = malloc((size_t)w * h * 3);
    for (size_t i = 0; i < (size_t)w * h; ++i) { rgb[i*3] = px[i] >> 16; rgb[i*3+1] = px[i] >> 8; rgb[i*3+2] = px[i]; }
    stbi_write_png(path, w, h, 3, rgb, w * 3);
    free(rgb);
}

int main(int argc, char **argv) {
    if (argc < 8) { fprintf(stderr, "uso: snkhd_cps3 zip gioco regione frames kx ky out [script]\n"); return 1; }
    int frames = atoi(argv[4]), kx = atoi(argv[5]), ky = atoi(argv[6]);
    const char *out = argv[7];
    if (getenv("WATCH")) { unsigned lo, hi; if (sscanf(getenv("WATCH"), "%x-%x", &lo, &hi) == 2) cps3_set_watch(lo, hi); }
    { extern void cps3_set_rwatch(uint32_t, uint32_t); unsigned lo, hi; if (getenv("RWATCH") && sscanf(getenv("RWATCH"), "%x-%x", &lo, &hi) == 2) cps3_set_rwatch(lo, hi); }
    { extern unsigned cps3_dbg_bioswatch; if (getenv("BIOSW")) cps3_dbg_bioswatch = (unsigned)strtoul(getenv("BIOSW"), NULL, 16); }
    if (!cps3_load_zip(argv[1], argv[2], (int)strtol(argv[3], NULL, 16))) { fprintf(stderr, "caricamento fallito\n"); return 1; }
    if (getenv("PCTRAP")) { extern int sh2_pctrap; sh2_pctrap = 1; }
    if (getenv("PCMAP")) { extern unsigned char *sh2_pcmap; sh2_pcmap = calloc(0x100000, 1); }
    if (getenv("HUDDYN")) cps3_set_hud_dynamic(atoi(getenv("HUDDYN")));
    if (getenv("OC")) cps3_set_overclock(atoi(getenv("OC")));
    { extern int cps3_dbg_only; extern unsigned long long cps3_dbg_skip;
      if (getenv("ONLY")) cps3_dbg_only = atoi(getenv("ONLY"));
      if (getenv("SKIP")) cps3_dbg_skip = strtoull(getenv("SKIP"), NULL, 16); }
    { extern int cps3_dbg_ioread; if (getenv("IOREAD")) cps3_dbg_ioread = 1; }
    unsigned W = 384 * kx, H = 224 * ky;
    uint32_t *hd = calloc((size_t)W * H, 4), *ref = calloc(384 * 224, 4);
    cps3_set_hd(hd, W, H);
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
    int frmark = getenv("FRAMEMARK") != NULL;
    for (int fr = 0; fr < frames; ++fr) {
        if (frmark) fprintf(stderr, "FR %d\n", fr);
        int want_shot = 0, want_cmp = 0;
        for (int i = 0; i < ncmd; ++i) {
            cmd_t *c = &cmds[i];
            if (c->frame != fr) continue;
            if (!strcmp(c->cmd, "in")) { joy[c->a & 1] = c->b; sys[c->a & 1] = c->c; }
            else if (!strcmp(c->cmd, "shot")) want_shot = 1;
            else if (!strcmp(c->cmd, "cmp")) want_cmp = 1;
            else if (!strcmp(c->cmd, "w8")) cps3_write8(c->a, (uint8_t)c->b);
            else if (!strcmp(c->cmd, "eew")) cps3_eeprom_wr((int)c->a, (uint16_t)c->b);
            else if (!strcmp(c->cmd, "p16")) { if (!cps3_patch16(c->a, (uint16_t)c->b, (uint16_t)c->c)) fprintf(stderr, "p16 %08x: byte diversi\n", c->a); }
            else if (!strcmp(c->cmd, "load")) {
                FILE *f = fopen(c->s, "rb"); if (!f) { fprintf(stderr, "stato %s mancante\n", c->s); continue; }
                size_t n = cps3_state_size(); void *b = malloc(n);
                if (fread(b, 1, n, f) == n) cps3_state_load(b, n);
                fclose(f); free(b);
            }
        }
        static int de = -1, se = -1; static const char *dp;
        if (de < 0) { de = getenv("DUMPEVERY") ? atoi(getenv("DUMPEVERY")) : 0; se = getenv("SHOTEVERY") ? atoi(getenv("SHOTEVERY")) : 0; dp = getenv("DUMPPRE"); }
        if (se > 0 && fr % se == 0) want_shot = 1;
        for (int p = 0; p < 2; ++p) cps3_set_input(p, joy[p], sys[p]);
        cps3_frame(!fast || want_shot || want_cmp);
        { size_t na; const int16_t *ab = cps3_audio(&na); asum += (long)na;
          static FILE *wf; if (!wf && getenv("WAV")) wf = fopen(getenv("WAV"), "wb");
          if (wf) fwrite(ab, 4, na, wf); }
        { static int pe = -1; if (pe < 0) pe = getenv("SSPAL") ? atoi(getenv("SSPAL")) : 0;
          if (pe > 0 && fr % pe == 0) {
              const uint32_t *ss = cps3_ssram(); unsigned bank = cps3_ss_bank(); unsigned char seen[256] = { 0 };
              for (unsigned k = 0; k < 64 * 28; ++k) { uint32_t d = ss[bank + k]; unsigned t = (d >> 16) & 0x1ff; if (t && t != 0x20) seen[(((d & 0x3f) >> 1) + (cps3_ss_palbase() << 5)) & 0xff] = 1; }
              printf("sspal %d:", fr); for (int k = 0; k < 256; ++k) if (seen[k]) printf(" %02x", k); printf("\n");
          } }
        if (de > 0 && dp && fr % de == 0) {
            char p[600]; snprintf(p, sizeof p, "%s_%d.bin", dp, fr);
            FILE *f = fopen(p, "wb");
            for (unsigned k = 0; f && k < 0x80000; ++k) fputc(cps3_read8(0x02000000 + k), f);
            if (f) fclose(f);
        }
        if (want_shot) {
            char p[600];
            snprintf(p, sizeof p, "%s_%d.png", out, fr); save_png(p, hd, W, H);
            cps3_render_reference(ref);
            snprintf(p, sizeof p, "%s_%d_r.png", out, fr); save_png(p, ref, 384, 224);
        }
        if (want_cmp) {
            cps3_render_reference(ref);
            long diff = 0;
            for (unsigned y = 0; y < 224 && kx == 1 && ky == 1; ++y)
                for (unsigned x = 0; x < 384; ++x) if ((hd[y * W + x] & 0xffffff) != (ref[y * 384 + x] & 0xffffff)) diff++;
            printf("cmp %d: zoom %02x diff %ld\n", fr, cps3_fszoom(), diff);
        }
        for (int i = 0; i < ncmd; ++i) {
            cmd_t *c = &cmds[i];
            if (c->frame != fr) continue;
            if (!strcmp(c->cmd, "ram")) {
                printf("ram %d %08x:", fr, c->a);
                for (unsigned k = 0; k < c->b; ++k) printf("%s%02x", (k % 16) ? " " : "\n  ", cps3_read8(c->a + k));
                printf("\n");
            } else if (!strcmp(c->cmd, "ee")) {
                printf("ee %d:", fr);
                for (int k = 0; k < 0x40; ++k) printf("%s%04x", (k % 8) ? " " : "\n  ", cps3_eeprom_rd(k));
                printf("\n");
            } else if (!strcmp(c->cmd, "save")) {
                size_t n = cps3_state_size(); void *b = malloc(n);
                cps3_state_save(b, n);
                FILE *f = fopen(c->s, "wb"); if (f) { fwrite(b, 1, n, f); fclose(f); }
                free(b);
            } else if (!strcmp(c->cmd, "dump")) {
                FILE *f = fopen(c->s, "wb");
                for (unsigned k = 0; f && k < 0x80000; ++k) fputc(cps3_read8(0x02000000 + k), f);
                if (f) fclose(f);
            } else if (!strcmp(c->cmd, "ss")) {
                const uint32_t *ss = cps3_ssram(); unsigned bank = cps3_ss_bank();
                printf("ss %d (palbase %x):\n", fr, cps3_ss_palbase());
                for (unsigned y = c->a; y <= c->b && y < 32; ++y) {
                    printf("  %2u:", y);
                    for (unsigned x = 0; x < 48; ++x) {
                        uint32_t d = ss[bank + y * 64 + x];
                        unsigned t = (d >> 16) & 0x1ff, pal = (d & 0x3f) >> 1;
                        if (t) printf(" %03x/%02x", t, pal); else printf(" ......");
                    }
                    printf("\n");
                }
            } else if (!strcmp(c->cmd, "prog")) {
                extern const uint8_t *cps3_prog(void);
                const uint8_t *pg = cps3_prog(); FILE *f = fopen(c->s, "wb");
                for (unsigned k = 0; f && k < 0x1000000; k += 4) { fputc(pg[k+3], f); fputc(pg[k+2], f); fputc(pg[k+1], f); fputc(pg[k], f); }
                if (f) fclose(f);
            } else if (!strcmp(c->cmd, "pcmap")) {
                // pcmap frame file: salva la mappa dei PC eseguiti dall'inizio (o dall'ultimo pcmap) e la azzera
                extern unsigned char *sh2_pcmap;
                if (sh2_pcmap) { FILE *f = fopen(c->s, "wb"); if (f) { fwrite(sh2_pcmap, 1, 0x100000, f); fclose(f); } memset(sh2_pcmap, 0, 0x100000); }
            } else if (!strcmp(c->cmd, "sprsum")) {
                // riassunto delle voci principali: n tilemap, n sprite, palette min-max, y min-max (grezze)
                const uint32_t *s = cps3_spritelist();
                printf("sprsum %d:", fr);
                for (int i2 = 0; i2 < 0x2000 / 4; i2 += 4) {
                    if (s[i2] & 0x80000000) break;
                    unsigned len = (s[i2] & 0x01ff0000) >> 14, st = ((s[i2] & 0x7ff0) >> 4) * 0x100 / 4;
                    int ntm = 0, nsp = 0; unsigned pmin = 999, pmax = 0;
                    for (unsigned j = 0; j < len && st + j + 2 < 0x20000; j += 4) {
                        uint32_t v1 = s[st + j], v3 = s[st + j + 2];
                        if (!(v3 & 0xc)) continue;
                        if (!(v3 & 3)) ntm++; else { nsp++; unsigned pl = (s[i2+2] & 0x20000000) ? (s[i2+2] >> 16) & 0x1ff : v1 & 0x1ff; if (pl < pmin) pmin = pl; if (pl > pmax) pmax = pl; }
                    }
                    if (!ntm && !nsp) continue;
                    printf(" [%d g%u t%d s%d p%x-%x]", i2 / 4, (s[i2] >> 28) & 7, ntm, nsp, pmin, pmax);
                }
                printf("\n");
            } else if (!strcmp(c->cmd, "spr")) {
                const uint32_t *s = cps3_spritelist();
                printf("spr %d:\n", fr);
                for (int i2 = 0; i2 < 0x2000 / 4; i2 += 4) {
                    if (s[i2] & 0x80000000) break;
                    printf("  %3d: %08x %08x %08x %08x\n", i2 / 4, s[i2], s[i2+1], s[i2+2], s[i2+3]);
                }
            }
        }
    }
    double el = (double)(clock() - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "%d frame in %.2f s (%.2f ms/frame), audio %ld campioni (%.1f/frame), pc %08x\n",
            frames, el, el * 1000.0 / (frames ? frames : 1), asum, (double)asum / (frames ? frames : 1), cps3_pc());
    cps3_unload();
    return 0;
}
