// SNKHD - prova headless del percorso dell'app per i giochi Psikyo (emu.c + psikyocfg + visual)
// uso: snkhd_apppk <zip> <id> <jp 0/1> <versus 0/1> <difficolta'> <ombre 0-2> <frames> <kx> <ky> <out> [shot_every]
// env: OC=0/1 (overclock, predefinito 1), RAND=1 (ingressi casuali per entrambi), P2IDLE=1 (P2 fermo con RAND)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../ext/stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "emu.h"
#include "psikyocfg.h"
#include "visual.h"

static const char *gid; static int versus; static unsigned frame_count;
static int player_in(int p) { return psikyocfg_player_in(gid, p); }
static void auto_start(unsigned *s0, unsigned *s1, int boot) {
    int m1 = player_in(0), m2 = player_in(1), pulse = (frame_count % 16) < 4;
    if (boot) { if (!m1) { if (pulse) *s0 |= SYS_START; } else if (versus && !m2) { if (pulse) *s1 |= SYS_START; } return; }
    if (versus) { if (!m1 && pulse) *s0 |= SYS_START; if (!m2 && pulse) *s1 |= SYS_START; }
}

int main(int argc, char **argv) {
    if (argc < 11) { fprintf(stderr, "uso: snkhd_apppk zip id jp versus diff ombre frames kx ky out [every]\n"); return 1; }
    gid = argv[2]; int jp = atoi(argv[3]); versus = atoi(argv[4]);
    int diff = atoi(argv[5]), shadows = atoi(argv[6]), frames = atoi(argv[7]);
    unsigned kx = atoi(argv[8]), ky = atoi(argv[9]); const char *out = argv[10];
    int every = argc > 11 ? atoi(argv[11]) : 0;
    emu_prepare(SYS_PSIKYO);
    emu_set_hd(320 * kx, 224 * ky);
    if (!emu_load_psikyo(argv[1], gid, psikyocfg_region(gid, jp))) { fprintf(stderr, "load fallito\n"); return 1; }
    emu_set_overclock(getenv("OC") ? atoi(getenv("OC")) : 1);
    psikyocfg_apply(gid, diff);
    visual_apply(gid, shadows, 0, 0, 0);
    printf("HD %ux%u, nativo %ux%u, fps %.3f, audio %.1f Hz\n", emu_hd_w(), emu_hd_h(), emu_native_w(), emu_native_h(), emu_fps(), emu_audio_rate());
    int boot_frames = 0;
    clock_t t0 = clock();
    for (;;) {
        unsigned s0 = 0, s1 = 0;
        auto_start(&s0, &s1, 1);
        emu_set_input(0, 0, s0); emu_set_input(1, 0, s1);
        emu_frame_fast(); ++frame_count;
        if (++boot_frames > 120 && player_in(0) && (!versus || player_in(1))) break;
        if (boot_frames > 4000) { printf("boot: timeout\n"); break; }
    }
    printf("boot: %d frame (%.2f s), P1 %d P2 %d\n", boot_frames, (double)(clock() - t0) / CLOCKS_PER_SEC, player_in(0), player_in(1));
    unsigned w, h, pitch;
    int rnd = getenv("RAND") != NULL, p2idle = getenv("P2IDLE") != NULL; unsigned rj[2] = { 0, 0 }; unsigned long long seed = 12345;
    double tmax = 0, tsum = 0; int tmaxf = 0; long na_sum = 0; int pin[2] = { -1, -1 };
    for (int f = 0; f < frames; ++f) {
        unsigned j0 = ((f % 90) < 4) ? IN_A : 0, j1 = 0, s0 = 0, s1 = 0;
        if (rnd) {
            if (f % 6 == 0) for (int p = 0; p < 2; ++p) {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                unsigned r = (unsigned)(seed >> 33);
                unsigned d = r & 15; if ((d & 3) == 3) d &= ~3u; if ((d & 12) == 12) d &= ~12u;
                rj[p] = d | (((r >> 4) & 15) << 4 & ((r >> 10) & 1 ? 0xf0 : 0));
            }
            j0 = rj[0]; j1 = p2idle ? 0 : rj[1];
        }
        auto_start(&s0, &s1, 0);
        emu_set_input(0, j0, s0); emu_set_input(1, j1, s1);
        clock_t ft = clock();
        emu_frame(); ++frame_count;
        double fms = (double)(clock() - ft) * 1000.0 / CLOCKS_PER_SEC;
        tsum += fms; if (fms > tmax && f > 10) { tmax = fms; tmaxf = f; }
        visual_frame(gid);
        size_t na; emu_audio(&na); na_sum += (long)na;
        for (int p = 0; p < 2; ++p) { int v = player_in(p); if (v != pin[p]) { printf("f %d: P%d %s\n", f, p + 1, v ? "in" : "fuori"); pin[p] = v; } }
        if (every && f % every == 0) {
            const uint32_t *px = emu_video(&w, &h, &pitch);
            unsigned char *rgb = malloc((size_t)w * h * 3);
            for (size_t y = 0; y < h; ++y) for (size_t x = 0; x < w; ++x) { uint32_t c = px[y * pitch + x]; unsigned char *o = rgb + (y * w + x) * 3; o[0] = c >> 16; o[1] = c >> 8; o[2] = c; }
            char p[600]; snprintf(p, sizeof p, "%s_%05d.png", out, f);
            stbi_write_png(p, w, h, 3, rgb, w * 3); free(rgb);
        }
    }
    printf("tempo frame: medio %.2f ms, massimo %.2f ms (frame %d)\n", tsum / (frames ? frames : 1), tmax, tmaxf);
    printf("fine: %d frame, audio %.1f/frame @ %.1f Hz, fps %.3f, P1 %d P2 %d\n",
           frames, (double)na_sum / (frames ? frames : 1), emu_audio_rate(), emu_fps(), player_in(0), player_in(1));
    emu_unload();
    return 0;
}
