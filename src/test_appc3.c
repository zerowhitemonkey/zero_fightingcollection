// SNKHD - prova headless del percorso dell'app per i giochi CPS3 (emu.c + cps3cfg + visual)
// uso: snkhd_appc3 <zip> <id> <jp 0/1> <versus 0/1> <difficolta'> <hud 0-3> <area 0/1> <frames> <kx> <ky> <out> [shot_every]
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ext/stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "emu.h"
#include "cps3cfg.h"
#include "visual.h"
#include "../cps3/cps3.h"
#include "input.h"

static const char *gid; static int versus; static unsigned frame_count;
static int player_in(int p) { return cps3cfg_player_in(gid, p); }
static void auto_start(unsigned *s0, unsigned *s1, int boot) {
    int m1 = player_in(0), m2 = player_in(1), pulse = (frame_count % 16) < 4;
    if (boot) { if (!m1) { if (pulse) *s0 |= SYS_START; } else if (versus && !m2) { if (pulse) *s1 |= SYS_START; } return; }
    if (versus) { if (!m1 && pulse) *s0 |= SYS_START; if (!m2 && pulse) *s1 |= SYS_START; }
}

int main(int argc, char **argv) {
    if (argc < 12) { fprintf(stderr, "uso\n"); return 1; }
    gid = argv[2]; int jp = atoi(argv[3]); versus = atoi(argv[4]);
    int diff = atoi(argv[5]), hud = atoi(argv[6]), area = atoi(argv[7]), frames = atoi(argv[8]);
    unsigned kx = atoi(argv[9]), ky = atoi(argv[10]); const char *out = argv[11];
    int every = argc > 12 ? atoi(argv[12]) : 0;
    emu_set_hd(384 * kx, 224 * ky);
    if (!emu_load_cps3(argv[1], gid, cps3cfg_region(gid, jp))) { fprintf(stderr, "load fallito\n"); return 1; }
    emu_set_overclock(1);
    cps3cfg_apply(gid, diff, jp);
    if (versus == 2) cps3cfg_vscpu_setup(gid, 1, getenv("VSSTAGE") ? atoi(getenv("VSSTAGE")) : 0);
    // P1SCRIPT: righe "frame joy" (dopo l'avvio) che sostituiscono i comandi del P1 da quel frame
    static unsigned scr_f[4096], scr_j[4096]; int nscr = 0;
    if (getenv("P1SCRIPT")) { FILE *sf = fopen(getenv("P1SCRIPT"), "r"); while (sf && nscr < 4096 && fscanf(sf, "%u %u", &scr_f[nscr], &scr_j[nscr]) == 2) ++nscr; if (sf) fclose(sf); }
    visual_apply(gid, 0, hud, area, 0);
    // avvio veloce come run_boot dell'app
    int boot_frames = 0;
    clock_t t0 = clock();
    for (;;) {
        unsigned s0 = 0, s1 = 0;
        auto_start(&s0, &s1, 1);
        emu_set_input(0, 0, s0); emu_set_input(1, 0, s1);
        emu_frame_fast(); ++frame_count;
        cps3cfg_frame(gid, getenv("NOMAX") ? 0 : 1);
        if (++boot_frames > 120 && player_in(0) && (!versus || player_in(1))) break;
        if (boot_frames > 4000) { printf("boot: timeout\n"); break; }
    }
    printf("boot: %d frame (%.2f s), P1 %d P2 %d\n", boot_frames, (double)(clock() - t0) / CLOCKS_PER_SEC, player_in(0), player_in(1));
    unsigned w, h, pitch; long gated = 0;
    // partita: P1 preme LP ogni tanto (scelta, password NO, testi), P2 nulla
    int rnd = getenv("RAND") != NULL; unsigned rj[2] = { 0, 0 }; unsigned long long seed = 12345;
    double tmax = 0, tsum = 0; int tmaxf = 0;
    for (int f = 0; f < frames; ++f) {
        unsigned j0 = ((f % 90) < 4) ? IN_A : 0, j1 = 0, s0 = 0, s1 = 0;
        if (rnd) {
            if (f % 6 == 0) for (int p = 0; p < 2; ++p) {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                unsigned r = (unsigned)(seed >> 33);
                unsigned d = r & 15; if ((d & 3) == 3) d &= ~3u; if ((d & 12) == 12) d &= ~12u;
                rj[p] = d | (((r >> 4) & 63) << 4 & ((r >> 10) & 1 ? 0x3f0 : 0));
            }
            j0 = rj[0]; j1 = rj[1];
        }
        if (nscr) { unsigned v = 0; int have = 0; for (int k = 0; k < nscr; ++k) if (scr_f[k] <= (unsigned)f) { v = scr_j[k]; have = 1; } if (have) j0 = v; }
        auto_start(&s0, &s1, 0);
        if (versus == 2) cps3cfg_vscpu_input(gid, &j0, &j1);
        emu_set_input(0, j0, s0); emu_set_input(1, j1, s1);
        clock_t ft = clock();
        emu_frame(); ++frame_count;
        if (getenv("TRACEM") && f % 10 == 0) printf("f %d m %02x j0 %x j1 %x cpu %d stage %d c1 %02x c2 %02x tm %d\n", f, emu_read8(0x02033142), j0, j1, emu_read8(0x02034e38), emu_read8(0x02031459), emu_read8(0x02033144), emu_read8(0x02033146), emu_read8(0x02033149));
        cps3cfg_frame(gid, getenv("NOMAX") ? 0 : 1);
        double fms = (double)(clock() - ft) * 1000.0 / CLOCKS_PER_SEC;
        tsum += fms; if (fms > tmax && f > 10) { tmax = fms; tmaxf = f; }
        visual_frame(gid);
        gated += emu_blend_fixpal_present(0x0b) || emu_blend_fixpal_present(0x17);
        if (every && f % every == 0 && getenv("DUMPS")) {
            char p[600]; snprintf(p, sizeof p, "%s_%05d.bin", out, f);
            FILE *fd = fopen(p, "wb"); for (unsigned k = 0; fd && k < 0x80000; ++k) fputc(emu_read8(0x02000000 + k), fd); if (fd) fclose(fd);
        }
        if (every && f % every == 0) {
            const uint32_t *px = emu_video(&w, &h, &pitch);
            unsigned char *rgb = malloc((size_t)w * h * 3);
            for (size_t i = 0; i < (size_t)w * h; ++i) { rgb[i*3] = px[i] >> 16; rgb[i*3+1] = px[i] >> 8; rgb[i*3+2] = px[i]; }
            char p[600]; snprintf(p, sizeof p, "%s_%05d.png", out, f);
            stbi_write_png(p, w, h, 3, rgb, w * 3); free(rgb);
        }
    }
    if (getenv("TRACE")) {}
    printf("tempo frame: medio %.2f ms, massimo %.2f ms (frame %d)\n", tsum / (frames ? frames : 1), tmax, tmaxf);
    size_t na; emu_audio(&na);
    printf("fine: %d frame, incontro (HUD) in %ld frame, audio %zu/frame @ %.1f Hz, fps %.5f, P1 %d P2 %d\n",
           frames, gated, na, emu_audio_rate(), emu_fps(), player_in(0), player_in(1));
    emu_unload();
    return 0;
}
