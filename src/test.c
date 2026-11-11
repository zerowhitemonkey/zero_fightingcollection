// SNKHD - banco di prova headless
// uso: snkhd_test <rom.neo> <neogeo.zip> <jp 0/1> <frames> <hd N> <outprefix> [script]
// script: righe "frame port joy sys" (joy/sys bitmask, valgono dal frame indicato)
//         righe "shot frame"  -> salva outprefix_<frame>.png (HD) e _n.png (nativo)
//         righe "dip idx val"
//         righe "ram frame addr len" -> stampa RAM
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ext/stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "emu.h"
#include "softdip.h"
#include "rompatch.h"
#include "visual.h"
#include "geo.h"

typedef struct { int frame, port; unsigned joy, sys; } ev_t;
static ev_t evs[100000]; static int nev;
static int shots[10000], nshots;
static int rams[1000][3], nrams;
static int vdumps[1000], nvd; // SNKHD: "vram frame" -> outprefix_<frame>.vram (VRAM 64K+4K) + .pal (palette RGB)
static int dumps[10000], ndumps;
static int pokes[1000][5], npokes;   // "poke f0 f1 addr val size": scrive in RAM prima dei frame f0..f1
int snkhd_frame;
extern uint64_t *geo_lspc_idmap;   // "dump frame" -> outprefix_<frame>.ram/.pal/.vram
#include "geo_lspc.h"

static void save_png(const char *path, const uint32_t *px, unsigned w, unsigned h, unsigned pitch) {
    unsigned char *rgb = malloc(w * h * 3);
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x) {
            uint32_t c = px[y * pitch + x];
            rgb[(y * w + x) * 3 + 0] = c >> 16;
            rgb[(y * w + x) * 3 + 1] = c >> 8;
            rgb[(y * w + x) * 3 + 2] = c;
        }
    stbi_write_png(path, w, h, 3, rgb, w * 3);
    free(rgb);
}

int main(int argc, char **argv) {
    if (argc < 7) { fprintf(stderr, "uso\n"); return 1; }
    int frames = atoi(argv[4]);
    { int n = atoi(argv[5]); if (getenv("HDW")) emu_set_hd(atoi(getenv("HDW")), atoi(getenv("HDH"))); else emu_set_hd(320 * n, 224 * n); }
    if (!emu_load(argv[1], argv[2], atoi(argv[3]), NULL)) { fprintf(stderr, "load fallito\n"); return 1; }
    if (getenv("OC")) emu_set_overclock(atoi(getenv("OC")));
    { extern int snkhd_watchv; if (getenv("WATCHV")) snkhd_watchv = (int)strtol(getenv("WATCHV"), NULL, 16); }
#ifdef SNKHD_HOOK
    { extern int snkhd_watchw; if (getenv("WATCHW")) snkhd_watchw = (int)strtol(getenv("WATCHW"), NULL, 16); }
    { extern unsigned snkhd_watchl; if (getenv("WATCHL")) snkhd_watchl = (unsigned)strtoul(getenv("WATCHL"), NULL, 16); }
#endif
    { extern unsigned snkhd_rw_lo, snkhd_rw_hi; extern long snkhd_rw_max; if (getenv("RWATCH")) sscanf(getenv("RWATCH"), "%x-%x", &snkhd_rw_lo, &snkhd_rw_hi);
      if (getenv("RWMAX")) snkhd_rw_max = atol(getenv("RWMAX")); }
    { extern unsigned snkhd_watch; if (getenv("WATCH")) snkhd_watch = (unsigned)strtoul(getenv("WATCH"), NULL, 16); }
#ifdef SNKHD_HOOK
    { extern unsigned snkhd_vw_lo, snkhd_vw_hi; extern long snkhd_vw_max;
      if (getenv("VWATCH")) sscanf(getenv("VWATCH"), "%x-%x", &snkhd_vw_lo, &snkhd_vw_hi);
      if (getenv("VWMAX")) snkhd_vw_max = atol(getenv("VWMAX")); }
#endif
#ifdef SNKHD_HOOK
    { extern unsigned snkhd_brk[16]; extern int snkhd_nbrk; extern long snkhd_brk_max; char *b = getenv("BRK");
      if (getenv("BRKMAX")) snkhd_brk_max = atol(getenv("BRKMAX"));
      { extern int snkhd_brk_quiet; extern unsigned snkhd_brk_off; snkhd_brk_quiet = getenv("BRKQ") != NULL; if (getenv("BRKOFF")) snkhd_brk_off = (unsigned)strtoul(getenv("BRKOFF"), NULL, 16); }
      while (b && *b && snkhd_nbrk < 16) { snkhd_brk[snkhd_nbrk++] = (unsigned)strtoul(b, &b, 16); if (*b == ',') b++; } }
#endif
    if (getenv("SHADOWS") && getenv("PATCH")) printf("shadows %d\n", shadow_apply(getenv("PATCH"), atoi(getenv("SHADOWS"))));
    int vis_on = getenv("PATCH") && (getenv("HUD") || getenv("FX"));
    if (vis_on) visual_apply(getenv("PATCH"), getenv("SHADOWS") ? atoi(getenv("SHADOWS")) : 0, getenv("HUD") ? atoi(getenv("HUD")) : 0,
                             getenv("AREA") ? atoi(getenv("AREA")) : 0, getenv("FX") ? atoi(getenv("FX")) : 0);
    if (getenv("BLD")) printf("blend %d\n", emu_load_blend(getenv("BLD"), getenv("BW1") ? atoi(getenv("BW1")) : 0, getenv("BW2") ? atoi(getenv("BW2")) : 0));
    if (getenv("PATCH")) printf("rompatch %d\n", rompatch_apply(getenv("PATCH")));
    if (getenv("OPT") && getenv("PATCH")) printf("opt %d\n", rompatch_options(getenv("PATCH"), (unsigned)strtoul(getenv("OPT"), NULL, 0)));
    if (getenv("SOFTDIP")) { size_t psz; const uint8_t *p = emu_prom(&psz); softdip_apply(p, psz, atoi(getenv("SOFTDIP")), atoi(argv[3])); }
    int automode = getenv("AUTO") ? atoi(getenv("AUTO")) : 0;
    unsigned rnd = getenv("SEED") ? (unsigned)atoi(getenv("SEED")) : 12345;
    if (argc > 7) {
        FILE *f = fopen(argv[7], "r"); char l[256];
        while (f && fgets(l, sizeof l, f)) {
            int a, b; unsigned c, d;
            if (sscanf(l, "poke %d %d %x %x %d", &pokes[npokes][0], &pokes[npokes][1], &pokes[npokes][2], &pokes[npokes][3], &pokes[npokes][4]) == 5) npokes++;
            else if (sscanf(l, "shot %d", &a) == 1) shots[nshots++] = a;
            else if (sscanf(l, "vram %d", &a) == 1) vdumps[nvd++] = a;
            else if (sscanf(l, "dump %d", &a) == 1) dumps[ndumps++] = a;
            else if (sscanf(l, "dip %d %d", &a, &b) == 2) emu_set_softdip(a, b);
            else if (sscanf(l, "ram %d %x %d", &rams[nrams][0], &rams[nrams][1], &rams[nrams][2]) == 3) nrams++;
            else if (sscanf(l, "%d %d %u %u", &a, &b, &c, &d) == 4) { evs[nev].frame = a; evs[nev].port = b; evs[nev].joy = c; evs[nev].sys = d; nev++; }
        }
        if (f) fclose(f);
    }
    if (getenv("LOADSTATE")) { if (!geo_state_load(getenv("LOADSTATE"))) fprintf(stderr, "loadstate fallito\n"); }
    int saveat = -1; char savepath[512] = "";
    if (getenv("SAVEAT")) sscanf(getenv("SAVEAT"), "%d:%511s", &saveat, savepath);
    if (getenv("IDMAP")) geo_lspc_idmap = (uint64_t*)calloc(320 * 224, 8);
    int ei = 0; size_t atot = 0;
    clock_t t0 = clock();
    for (int fr = 0; fr < frames; ++fr) { snkhd_frame = fr;
        while (!(automode == 3) && ei < nev && evs[ei].frame <= fr) { emu_set_input(evs[ei].port, evs[ei].joy, evs[ei].sys); ei++; }
        if (automode) {
            uint8_t *ram = emu_ram(); int m1 = ram[0xfdb6], m2 = ram[0xfdb7];
            int pulse = (fr % 16) < 4; unsigned s0 = 0, s1 = 0;
            if (m1 != 1 && pulse) s0 |= 1;
            if (automode == 2 && m1 == 1 && m2 != 1 && pulse) s1 |= 1;
            if (automode == 2 && m2 == 1 && m1 != 1 && pulse) s0 |= 1;
            // P1 (e P2 in VS) casuali
            rnd = rnd * 1103515245 + 12345; unsigned j0 = (rnd >> 16) & 0xff;
            rnd = rnd * 1103515245 + 12345; unsigned j1 = (rnd >> 16) & 0xff;
            if ((fr / 8) % 2 || getenv("NORAND")) { j0 = 0; j1 = 0; }   // NORAND: solo START, levette ferme
            if (automode == 3) {
                static int joined = -1;
                if (m1 == 1 && joined < 0) { joined = fr; printf("joined %d\n", fr); }
                unsigned jj = 0;
                while (ei < nev && evs[ei].frame <= fr - (joined < 0 ? 1000000 : joined)) { ei++; }
                // gli eventi dello script sono relativi al frame di ingresso
                for (int k = 0; k < nev; ++k) if (joined >= 0 && evs[k].frame <= fr - joined) jj = evs[k].joy;
                emu_set_input(0, jj, s0);
                emu_set_input(1, 0, 0);
            } else {
            emu_set_input(0, j0 & ((rnd>>8)&1 ? 0xff : 0xf0), s0);
            emu_set_input(1, automode == 2 ? j1 : 0, s1);
            }
            if (getenv("MODLOG") && fr % 300 == 0) printf("f%d mod %d %d\n", fr, m1, m2);
        }
        for (int k = 0; k < npokes; ++k) if (fr >= pokes[k][0] && fr <= pokes[k][1]) {
            uint8_t *ram = emu_ram(); unsigned a = pokes[k][2] & 0xffff, v = pokes[k][3];
            for (int q = pokes[k][4] - 1; q >= 0; --q) { ram[(a + q) & 0xffff] = v & 0xff; v >>= 8; } }
        if (getenv("FAST")) emu_frame_fast(); else emu_frame();
        if (vis_on) visual_frame(getenv("PATCH"));
        if (getenv("FINDSTR") && fr % 60 == 0) {
            const char *fs = getenv("FINDSTR"); size_t fl = strlen(fs); uint8_t *ram = emu_ram();
            for (size_t i = 0; i + fl < 65536; ++i) if (!memcmp(ram + i, fs, fl)) { printf("f%d %s @%05zx\n", fr, fs, 0x100000 + i); break; }
        }
        size_t n; const int16_t *ab = emu_audio(&n); atot += n;
        { static FILE *wf; if (!wf && getenv("WAV")) wf = fopen(getenv("WAV"), "wb"); if (wf && ab) fwrite(ab, 4, n, wf); } // SNKHD: PCM grezzo stereo s16
        if (fr == saveat) geo_state_save(savepath);
        if (getenv("SAVELIST")) {   // SAVELIST=f1,f2,... : <SAVEPFX>_<frame>.st
            char *q = getenv("SAVELIST");
            while (q && *q) { int v = atoi(q); if (v == fr) { char sp[600]; snprintf(sp, sizeof sp, "%s_%06d.st", getenv("SAVEPFX") ? getenv("SAVEPFX") : "/tmp/st", fr); geo_state_save(sp); break; }
                q = strchr(q, ','); if (q) ++q; } }
        if (getenv("SAVEEVERY") && fr > 0 && fr % atoi(getenv("SAVEEVERY")) == 0) {   // SAVEEVERY=n: <prefix>_<frame>.st
            char sp[600]; snprintf(sp, sizeof sp, "%s_%06d.st", getenv("SAVEPFX") ? getenv("SAVEPFX") : "/tmp/st", fr); geo_state_save(sp); }
        for (int s = 0; s < nshots; ++s) if (shots[s] == fr) {
            char p[512]; unsigned w, h, pitch;
            const uint32_t *v = emu_video(&w, &h, &pitch);
            if (!getenv("NATIVEONLY")) { snprintf(p, sizeof p, "%s_%06d.png", argv[6], fr); save_png(p, v, w, h, pitch); }
            v = emu_video_native(&pitch);
            snprintf(p, sizeof p, "%s_%06d_n.png", argv[6], fr); save_png(p, v, 320, 224, pitch);
            if (geo_lspc_idmap) { snprintf(p, sizeof p, "%s_%06d.id", argv[6], fr); FILE *idf = fopen(p, "wb"); fwrite(geo_lspc_idmap, 8, 320 * 224, idf); fclose(idf); }
        }
        for (int s = 0; s < ndumps; ++s) if (dumps[s] == fr) {
            char p[512]; FILE *f;
            snprintf(p, sizeof p, "%s_%06d.ram", argv[6], fr); f = fopen(p, "wb"); fwrite(emu_ram(), 1, 65536, f); fclose(f);
            snprintf(p, sizeof p, "%s_%06d.pal", argv[6], fr); f = fopen(p, "wb"); fwrite(geo_lspc_palram_ptr(), 1, 16384, f); fclose(f);
            snprintf(p, sizeof p, "%s_%06d.vram", argv[6], fr); f = fopen(p, "wb"); fwrite(geo_lspc_vram_ptr(), 1, 0x11000, f); fclose(f);
        }
        for (int s = 0; s < nvd; ++s) if (vdumps[s] == fr) {
            extern const void* geo_lspc_vram_ptr(void); extern const void* geo_lspc_palram_ptr(void);
            char p[512]; snprintf(p, sizeof p, "%s_%06d.vram", argv[6], fr);
            FILE *vf = fopen(p, "wb"); fwrite(geo_lspc_vram_ptr(), 1, 0x11000, vf); fclose(vf);
            snprintf(p, sizeof p, "%s_%06d.pal", argv[6], fr);
            vf = fopen(p, "wb"); fwrite(geo_lspc_palram_ptr(), 1, 0x4000, vf); fclose(vf);
        }
        for (int r = 0; r < nrams; ++r) if (rams[r][0] == fr) {
            uint8_t *ram = emu_ram(); printf("ram f%d %06x:", fr, 0x100000 + rams[r][1]);
            for (int i = 0; i < rams[r][2]; ++i) printf(" %02x", ram[(rams[r][1] + i) & 0xffff]);
            printf("\n");
        }
    }
    double el = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("%d frame in %.2fs (%.1f fps), audio %zu campioni (%.1f/frame)\n", frames, el, frames / el, atot, (double)atot / frames);
    return 0;
}
