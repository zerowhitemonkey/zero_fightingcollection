// SNKHD - soft-DIP automatici dalla tabella della ROM
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "softdip.h"
#include "emu.h"

static void norm(const uint8_t *src, char *dst) {   // 12 caratteri -> maiuscolo senza spazi
    int n = 0;
    for (int i = 0; i < 12; ++i) {
        int c = src[i];
        if (c == ' ' || c == 0) continue;
        dst[n++] = (char)toupper(c);
    }
    dst[n] = 0;
}

void softdip_apply(const uint8_t *p, size_t psz, int difficulty, int region_jp) {
    for (int i = 0; i < 16; ++i) emu_set_softdip(i, -1);
    if (psz < 0x200) return;
    uint32_t ptr = ((uint32_t)p[0x11a] << 24) | (p[0x11b] << 16) | (p[0x11c] << 8) | p[0x11d];
    if (ptr + 32 >= psz) return;
    const uint8_t *t = p + ptr;
    // valori predefiniti dalla tabella della regione in uso (stesse posizioni)
    uint32_t pjp = ((uint32_t)p[0x116] << 24) | (p[0x117] << 16) | (p[0x118] << 8) | p[0x119];
    const uint8_t *tdef = (region_jp && pjp + 32 < psz) ? p + pjp : t;
    size_t pos = 32;
    if (!(t[16] == 0xff && t[17] == 0xff)) pos += 12;
    if (!(t[18] == 0xff && t[19] == 0xff)) pos += 12;
    if (t[20] != 0xff) pos += 12;
    if (t[21] != 0xff) pos += 12;
    for (int i = 0; i < 10; ++i) {
        int n = t[22 + i] & 0x0f, def = t[22 + i] >> 4;
        if ((tdef[22 + i] & 0x0f) == n && (tdef[22 + i] >> 4) < n) def = tdef[22 + i] >> 4;
        if (!n) break;
        char desc[16], ch[16][16];
        norm(t + pos, desc); pos += 12;
        for (int c = 0; c < n; ++c) { norm(t + pos, ch[c]); pos += 12; }
        int v = def;
        if (!strcmp(desc, "DIFFICULTY")) {
            v = difficulty - 1;
            if (v >= n) v = n - 1;
            if (v < 0) v = 0;
        }
        else if (!strncmp(desc, "CREDIT/LEVEL", 12) || !strncmp(desc, "CREDIT", 6)) {
            v = n - 1;
            for (int c = 0; c < n; ++c) if (strstr(ch[c], "OFF/OFF")) v = c;
            for (int c = 0; c < n; ++c) if (!strcmp(ch[c], "WITHOUT")) v = c;   // es. Real Bout 2: "CREDIT" WITHOUT/WITH
        }
        else if (strstr(desc, "CREDIT") || strstr(desc, "DSP/LEVEL") || !strncmp(desc, "LEVELDISP", 9) || !strcmp(desc, "LEVEL")) {
            // varianti separate (es. Ninja Master's "DSP/CREDIT", "DSP/LEVEL": WITH/WITHOUT)
            for (int c = 0; c < n; ++c) if (!strcmp(ch[c], "WITHOUT") || !strcmp(ch[c], "OFF")) v = c;
        }
        else if (!strcmp(desc, "BLOOD/FLASH")) {
            // KOF 2000: sangue ON, lampi dello schermo OFF
            for (int c = 0; c < n; ++c) if (!strcmp(ch[c], "ON/OFF")) v = c;
        }
        else if (!strncmp(desc, "BLOOD", 5) ||!strcmp(desc, "CRUELFIGHT") || !strcmp(desc, "GORE")) {
            for (int c = 0; c < n; ++c) if (!strcmp(ch[c], "ON") || !strcmp(ch[c], "WITH") || !strcmp(ch[c], "RED")) v = c;
        }
        else if (!strcmp(desc, "VIOLENCE")) {
            v = n - 1;
        }
        else if (!strncmp(desc, "VIOLENCE/", 9)) {
            // "LVn /WITH..." : livello massimo mantenendo la seconda parte del default
            const char *suf = strchr(ch[def], '/');
            int best = -1, bestlv = -1;
            for (int c = 0; c < n; ++c) {
                const char *s2 = strchr(ch[c], '/');
                int lv = 0;
                if (!strncmp(ch[c], "LV", 2)) lv = ch[c][2] - '0';
                if (suf && s2 && strcmp(suf, s2)) continue;
                if (lv > bestlv) { bestlv = lv; best = c; }
            }
            if (best >= 0) v = best;
        }
        else if (!strcmp(desc, "INSTRUCTION") && !memcmp(t, "[ SAVAGE REIGN ]", 16)) {
            // Savage Reign: la finestra dei suggerimenti lampeggia a frame alterni (trasparenza)
            for (int c = 0; c < n; ++c) if (!strcmp(ch[c], "OFF")) v = c;
        }
        else if (!strcmp(desc, "FLASH")) {
            for (int c = 0; c < n; ++c) if (!strcmp(ch[c], "OFF") || !strcmp(ch[c], "WITHOUT")) v = c;
        }
        emu_set_softdip(6 + i, v);
    }
}
