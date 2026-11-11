// banco di prova SNES (core da solo): script ingressi, screenshot, savestate, watch RAM, breakpoint
// uso: snkhd_snes rom.zip out_prefix frame[,frame..]
// env: PRESS="frame:mask:len,..." (P1)  PRESS2=... (P2)  LOAD=file.st  SAVE=frame:file  DUMP=frame:file (WRAM 128K)
//      WATCH=lo-hi (scritture WRAM, indirizzi 0-1FFFF)  WATCHV=val (solo scritture di quel valore)  BRK=a24,a24 (registri all'esecuzione)
//      BRKMAX=n  FASTTO=frame (fino a quel frame senza rendering)  MASKS: B=1 Y=2 SEL=4 START=8 UP=16 DOWN=32 LEFT=64 RIGHT=128 A=256 X=512 L=1024 R=2048
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "snes_core.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
static int frame, wlo = -1, whi = -1, wval = -1, nbrk, brkmax = 40, brkn;
static uint32_t brk[64];
static uint32_t lastpc;
static uint32_t ring[512]; static unsigned ringn;
static void on_exec(uint32_t pc) {
    lastpc = pc; ring[ringn++ & 511] = pc;
    for (int i = 0; i < nbrk; i++) if (brk[i] == pc && brkn < brkmax) {
        uint32_t r[9]; snescore_regs(r); brkn++;
        printf("BRK f%d %06X A=%04X X=%04X Y=%04X S=%04X D=%04X B=%02X P=%03X\n", frame, pc, r[0], r[1], r[2], r[3], r[5], r[7], r[8]);
        { const char *dm = getenv("DUMPMEM"); if (dm) { char *q = strdup(dm); printf("  mem:"); for (char *tk = strtok(q, ","); tk; tk = strtok(NULL, ",")) { unsigned a = strtoul(tk, NULL, 16); printf(" %X=%04X", a, snescore_wram8(a) | snescore_wram8(a + 1) << 8); } printf("\n"); free(q); } }
        if (getenv("PCRING")) { int n = atoi(getenv("PCRING")); printf("  pcs:"); for (int i = n; i >= 1; i--) printf(" %06X", ring[(ringn - i) & 511]); printf("\n"); }
        if (!getenv("NOSTACK")) {
        printf("  stack:"); for (int i = 1; i <= 24; i++) printf(" %02X", snescore_wram8((r[3] + i) & 0x1ffff)); printf("\n"); }
    }
}
static void on_write(uint32_t a, uint8_t v) {
    uint32_t b = (a >> 16) & 0xff, p = a & 0xffff, w;
    if (b == 0x7e || b == 0x7f) w = ((b & 1) << 16) | p;
    else if ((b < 0x40 || (b >= 0x80 && b < 0xc0)) && p < 0x2000) w = p;
    else return;
    if ((int)w >= wlo && (int)w <= whi && (wval < 0 || v == wval)) printf("W f%d %05X=%02X pc=%06X\n", frame, w, v, lastpc);
}
static int rlo = -1, rhi = -1, rmax = 200, rn;
static void on_read(uint32_t a, uint8_t v) {
    uint32_t b = (a >> 16) & 0xff, p = a & 0xffff, w;
    if (b == 0x7e || b == 0x7f) w = ((b & 1) << 16) | p;
    else if ((b < 0x40 || (b >= 0x80 && b < 0xc0)) && p < 0x2000) w = p;
    else return;
    if ((int)w >= rlo && (int)w <= rhi && rn < rmax) { rn++; printf("R f%d %05X=%02X pc=%06X\n", frame, w, v, lastpc); }
}
static void parse_press(const char *pe, int (*press)[3], int *np) {
    if (!pe) return;
    char *q = strdup(pe);
    for (char *t = strtok(q, ","); t; t = strtok(NULL, ",")) { sscanf(t, "%d:%d:%d", &press[*np][0], &press[*np][1], &press[*np][2]); (*np)++; }
}
int main(int argc, char **argv) {
    if (argc < 4) return 1;
    if (!snescore_load_zip(argv[1])) { printf("load fail\n"); return 2; }
    int shots[64], ns = 0; char *s = strdup(argv[3]);
    for (char *t = strtok(s, ","); t; t = strtok(NULL, ",")) shots[ns++] = atoi(t);
    int press[256][3], np = 0, press2[256][3], np2 = 0;
    if (getenv("GUARD")) { size_t rl; const uint8_t *rr = snescore_rom(&rl); printf("rom %zx %02x\n", rl, rr[0x2ADE16]); printf("guard patch: %d\n", snescore_patch_weaponlord()); }
    parse_press(getenv("PRESS"), press, &np); parse_press(getenv("PRESS2"), press2, &np2);
    int last = 0; for (int i = 0; i < ns; i++) if (shots[i] > last) last = shots[i];
    const char *e;
    if ((e = getenv("WATCH"))) sscanf(e, "%x-%x", &wlo, &whi);
    if ((e = getenv("RWATCH"))) sscanf(e, "%x-%x", &rlo, &rhi);
    if ((e = getenv("RMAX"))) rmax = atoi(e);
    if ((e = getenv("WATCHV"))) wval = (int)strtol(e, NULL, 16);
    if ((e = getenv("BRK"))) { char *q = strdup(e); for (char *t = strtok(q, ","); t; t = strtok(NULL, ",")) brk[nbrk++] = (uint32_t)strtoul(t, NULL, 16); }
    if ((e = getenv("BRKMAX"))) brkmax = atoi(e);
    int fastto = (e = getenv("FASTTO")) ? atoi(e) : 0;
    if ((e = getenv("LOAD"))) {
        FILE *f = fopen(e, "rb"); if (!f) { printf("no state\n"); return 3; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); void *b = malloc(n);
        if (fread(b, 1, n, f) != (size_t)n) return 3; fclose(f);
        snescore_frame(0); snescore_state_load(b, n); free(b);
    }
    if (wlo >= 0 || nbrk || rlo >= 0) snescore_set_debug((nbrk || rlo >= 0 || wlo >= 0) ? on_exec : NULL, wlo >= 0 ? on_write : NULL, rlo >= 0 ? on_read : NULL);
    int savef = -1; char savef_name[256] = ""; if ((e = getenv("SAVE"))) sscanf(e, "%d:%255s", &savef, savef_name);
    int dumpf = -1; char dump_name[256] = ""; if ((e = getenv("DUMP"))) sscanf(e, "%d:%255s", &dumpf, dump_name);
    int pk[16][3], npk = 0;   // POKE="frame:addr:val,..." (WRAM, esadecimale)
    if ((e = getenv("POKE"))) { char *q = strdup(e); for (char *tk = strtok(q, ","); tk; tk = strtok(NULL, ",")) { sscanf(tk, "%d:%x:%x", &pk[npk][0], &pk[npk][1], &pk[npk][2]); npk++; } }
    for (frame = 0; frame <= last; frame++) {
        for (int i = 0; i < npk; i++) if (pk[i][0] == frame) snescore_wram_write8(pk[i][1], pk[i][2]);
        unsigned m = 0, m2 = 0;
        for (int i = 0; i < np; i++) if (frame >= press[i][0] && frame < press[i][0] + press[i][2]) m |= press[i][1];
        for (int i = 0; i < np2; i++) if (frame >= press2[i][0] && frame < press2[i][0] + press2[i][2]) m2 |= press2[i][1];
        snescore_set_input(0, m); snescore_set_input(1, m2);
        snescore_frame(frame >= fastto);
        if (frame == savef) {
            size_t n = snescore_state_size(); void *b = malloc(n); snescore_state_save(b);
            FILE *f = fopen(savef_name, "wb"); fwrite(b, 1, n, f); fclose(f); free(b); printf("saved state f%d %zu\n", frame, n);
        }
        { static FILE *tf; static uint32_t tlo, thi; static int tinit;
          if (!tinit) { tinit = 1; const char *te = getenv("TRACE"); char tn[256]; if (te && sscanf(te, "%x-%x:%255s", &tlo, &thi, tn) == 3) tf = fopen(tn, "wb"); }
          if (tf) for (uint32_t a = tlo; a <= thi; a++) fputc(snescore_wram8(a), tf); }
        if (frame == dumpf) { FILE *f = fopen(dump_name, "wb"); for (uint32_t a = 0; a < 0x20000; a++) fputc(snescore_wram8(a), f); fclose(f); }
        for (int i = 0; i < ns; i++) if (shots[i] == frame) {
            unsigned w, h, p; const uint32_t *v = snescore_video(&w, &h, &p);
            uint32_t *o = malloc(w * h * 4);
            for (unsigned k = 0; k < w * h; k++) { uint32_t c = v[k]; o[k] = 0xff000000 | (c & 0xff00ff00) | ((c >> 16) & 0xff) | ((c & 0xff) << 16); }
            char nm[256]; snprintf(nm, sizeof nm, "%s_%05d.png", argv[2], frame);
            stbi_write_png(nm, w, h, 4, o, w * 4); free(o);
        }
    }
    return 0;
}
