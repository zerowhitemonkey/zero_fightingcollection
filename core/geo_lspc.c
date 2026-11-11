/*
Copyright (c) 2022-2026 Rupert Carmichael
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "geo.h"
#include "geo_cd.h"
#include "geo_m68k.h"
#include "geo_lspc.h"
#include "geo_serial.h"
#ifdef SNKHD_HOOK
#include <stdio.h>
#include "m68k/m68k.h"
#endif

#define M68K_CYC_PER_LINE 768

static void (*geo_lspc_fixline)(void);

static lspc_t lspc;
static romdata_t *romdata = NULL;

static uint32_t *vbuf = NULL;

// Hacks
static unsigned sprlimit = 96; // Sprites-per-line limit
static unsigned skip_render; // Skip rendering during CD loading fast-forward

// Line buffering
static uint32_t linebuf[2][LSPC_WIDTH]; // Line buffers for sprite pixels (SNKHD: | peso << 16)
static unsigned lbactive = 0; // Active line buffer

static uint8_t *fixdata = NULL;

static unsigned fixbanksw = 0;
static uint32_t crommask = 0;

// Dynamic output palette with values converted from palette RAM
static uint32_t palette_normal[SIZE_8K];
static uint32_t palette_shadow[SIZE_8K];
static uint32_t *palette = palette_normal;

// Palette lookup tables
static unsigned lut_normal[64];
static unsigned lut_shadow[64];

static unsigned reg_disblspr = 0;
static unsigned reg_disblfix = 0;
static unsigned reg_envideo = 0;

/* SNKHD: rendering HD a risoluzione interna N x N.
   Gli sprite ridotti vengono campionati a griglia fine interpolando tra le
   righe/colonne scelte dall'hardware per due posizioni native consecutive,
   cosi' a N=1 l'uscita e' identica all'originale e a N>1 il dettaglio perso
   dallo shrink viene recuperato. */
static unsigned hd_on = 0;              // HD attivo
static unsigned hd_w = 0, hd_h = 0;     // dimensione dell'immagine visibile (320x224 nativi)
static uint32_t *hd_vbuf = NULL;        // hd_w x hd_h
static uint32_t *hd_lb[2] = { NULL, NULL }; // hd_maxrh righe x hd_w
static unsigned hd_maxrh = 0;
static uint16_t hcx[LSPC_WIDTH + 1];    // prima sotto-colonna di ogni colonna nativa
static uint16_t vry[LSPC_SCANLINES];    // prima riga HD di ogni riga del buffer nativo
static uint8_t vrh[LSPC_SCANLINES];     // righe HD per riga nativa (0 = non visibile)
static uint32_t fixrow[LSPC_WIDTH];     // riga del fix layer (0 = trasparente)
static unsigned hd_hcol[16][17];        // colonne mostrate per ogni hshrink + 16
static uint8_t offt[17][17][16];        // [sotto-colonne][passo][k] = passo*k/sotto-colonne
static uint32_t *xq = NULL;             // centro di ogni colonna HD in 1/256 di pixel nativo
static uint8_t *cdec = NULL;            // C ROM decodificata: 1 byte per pixel
static size_t cdec_tiles = 0;

/* SNKHD: trasparenza delle sprite per tile (tabelle .bld di FBNeo).
   blendtab[tile] = peso della sprite 1-255 (0 = opaca). I pixel di una tile
   trasparente si salvano nel buffer di riga come indice | peso << 16 e il
   colore sottostante (RGB, o BLD_BD = fondale risolto alla visualizzazione)
   nel buffer parallelo "und"; la miscela si calcola quando si compone la riga. */
#define BLD_BD 0x01000000u
static uint8_t *blendtab = NULL;
static size_t blendn = 0;
static uint8_t blendpal[256];           // peso per numero di palette sprite (0 = opaca)
static unsigned blendpal_on = 0;
static uint32_t lbund[2][LSPC_WIDTH];
/* SNKHD (banco di prova): mappa della sorgente di ogni pixel nativo. Bit 63 = fix layer;
   sprite: indice << 40 | palette << 32 | tile; fix: palette << 32 | tile. */
uint64_t *geo_lspc_idmap = NULL;
static uint64_t dbglb[2][LSPC_WIDTH], dbgfix[LSPC_WIDTH];
static uint32_t *hd_und[2] = { NULL, NULL };
static unsigned blendline[2];           // la riga contiene pixel trasparenti

static inline uint32_t bld_mix(uint32_t d, uint32_t s, uint32_t p) {
    uint32_t a = 255 - p;
    return ((((s & 0xff00ff) * p + (d & 0xff00ff) * a) & 0xff00ff00) +
        (((s & 0x00ff00) * p + (d & 0x00ff00) * a) & 0x00ff0000)) >> 8;
}

// Scrive un pixel di sprite (indice palette v, peso p; p = 0 opaco)
static inline void bld_plot(uint32_t *lb, uint32_t *und, unsigned v, unsigned p) {
    if (!p) { *lb = v; return; }
    uint32_t cur = *lb, u;
    if (cur == (v | (p << 16)))
        return; // stesso colore trasparente gia' presente (ombre sovrapposte): niente doppia miscela
    if (!cur)
        u = BLD_BD;
    else if (cur >> 16) {
        uint32_t uc = *und;
        if (uc & BLD_BD) uc = palette[(lspc.palbank * SIZE_4K) + 4095];
        u = bld_mix(uc, palette[cur & 0xffff], cur >> 16);
    }
    else
        u = palette[cur] & 0xffffff;
    *lb = v | (p << 16);
    *und = u;
}

static inline uint32_t bld_res(uint32_t v, uint32_t u, uint32_t bdcol) {
    return bld_mix((u & BLD_BD) ? bdcol : u, palette[v & 0xffff], v >> 16);
}

void geo_lspc_set_blend(const uint8_t *tab, size_t n) {
    blendtab = (uint8_t*)tab;
    blendn = tab ? n : 0;
}

// SNKHD: trasparenza del fix layer per palette fix (16 pesi) o per tile fix (n pesi)
static uint8_t fixpalw[16];
static const uint8_t *fixtilew = NULL;
static size_t fixtilen = 0;
static unsigned fixblend_on = 0;
static uint8_t fixw[LSPC_WIDTH];
void geo_lspc_set_blend_fix(const uint8_t *palw, const uint8_t *tilew, size_t n) {
    fixblend_on = 0;
    for (unsigned i = 0; i < 16; ++i) { fixpalw[i] = palw ? palw[i] : 0; if (fixpalw[i]) fixblend_on = 1; }
    fixtilew = tilew; fixtilen = tilew ? n : 0;
    if (fixtilen) fixblend_on = 1;
}

void geo_lspc_set_blend_pal(const uint8_t *pal) {
    blendpal_on = 0;
    for (unsigned i = 0; i < 256; ++i) {
        blendpal[i] = pal ? pal[i] : 0;
        if (blendpal[i]) blendpal_on = 1;
    }
}

// Horizontal Shrink LUT -- from neogeodev
static unsigned lut_hshrink[0x10][0x10] = {
    { 0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0 }, // (15 pixel skipped, 1 remaining)
    { 0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0 }, // (14 pixels skipped...)
    { 0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0 },
    { 0,0,1,0,1,0,0,0,1,0,0,0,1,0,0,0 },
    { 0,0,1,0,1,0,0,0,1,0,0,0,1,0,1,0 },
    { 0,0,1,0,1,0,1,0,1,0,0,0,1,0,1,0 },
    { 0,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0 },
    { 1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0 },
    { 1,0,1,0,1,0,1,0,1,1,1,0,1,0,1,0 },
    { 1,0,1,1,1,0,1,0,1,1,1,0,1,0,1,0 },
    { 1,0,1,1,1,0,1,0,1,1,1,0,1,0,1,1 },
    { 1,0,1,1,1,0,1,1,1,1,1,0,1,0,1,1 },
    { 1,0,1,1,1,0,1,1,1,1,1,0,1,1,1,1 },
    { 1,1,1,1,1,0,1,1,1,1,1,0,1,1,1,1 },
    { 1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1 }, // (...1 pixel skipped)
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }, // (no pixels skipped, full size)
};

// Generate the "Raw" palette LUT
static void geo_lspc_palgen_raw(void) {
    // 6 bits means 64 iterations, as we include the "dark" bit as the LSB here
    for (unsigned i = 0; i < (1 << 6); ++i) {
        // Invert the "dark" bit
        unsigned raw = i ^ 0x01;

        /* Scale the 6-bit values to 8-bit values as percentages of a maximum.
           This is an integer equivalent of "((colour * 255.0) / 63.0) + 0.5",
           which results in the same integer value in all possible cases.
        */
        unsigned val = (raw * 259 + 33) >> 6;

        // Populate normal palette entry
        lut_normal[i] = val;

        /* Populate shadow palette entry by dividing original values in half
           to approximate a 150 ohm pulldown.
        */
        lut_shadow[i] = val >> 1;
    }
}

// Generate the "Resistor Network" palette LUT
static void geo_lspc_palgen_resnet(void) {
    /* The Neo Geo uses a resistor ladder DAC to convert digital palette values
       to analog RGB output. Each colour channel has 5 bits driving resistors
       connected to either VCC or GND based on the bit value. The output
       voltage is determined by the voltage divider formed by resistors to VCC
       versus resistors to GND.

       Additionally, a "dark" bit and "shadow" register can activate pulldown
       resistors to GND, reducing the output voltage (darkening the colour).

       Resistor Values (from schematic):
       Bit 0 (LSB): 220 ohms
       Bit 1:       470 ohms
       Bit 2:       1000 ohms
       Bit 3:       2200 ohms
       Bit 4 (MSB): 3900 ohms

       Dark bit pulldown: 8200 ohms (from Palette RAM entry)
       Shadow pulldown:   150 ohms (activated by REG_SHADOW)

       The dark bit signal passes through a 74LS05 open collector inverter
       before reaching the pulldown resistor on each RGB channel.
    */
    double resistance[5] = { 3900.0, 2200.0, 1000.0, 470.0, 220.0 };
    double pd_dark = 8200.0;
    double pd_shadow = 150.0;

    /* First pass: compute raw resistor network voltages for all 32 values.
       For each bit pattern, resistors with their bit set connect to VCC, while
       resistors with their bit clear connect to GND. Resistors in parallel
       combine as: Rtotal = (R1 * R2) / (R1 + R2).

       The output voltage is determined by the voltage divider:
       VOUT = VCC * (r_to_gnd / (r_to_vcc + r_to_gnd))

       When all bits are 1, r_to_gnd is infinite (open circuit), so VOUT = VCC
       (maximum brightness, normalized to 1.0). When all bits are 0, r_to_vcc
       is infinite, so VOUT = 0.
    */
    double v_raw[32];
    for (unsigned i = 0; i < (1 << 5); ++i) {
        double r_to_vcc = 0.0;
        double r_to_gnd = 0.0;

        for (int b = 0; b < 5; ++b) {
            double r = resistance[b];
            if (i & (1 << b)) {
                r_to_vcc =
                    (r_to_vcc == 0.0) ? r : (r_to_vcc * r) / (r_to_vcc + r);
            }
            else {
                r_to_gnd =
                    (r_to_gnd == 0.0) ? r : (r_to_gnd * r) / (r_to_gnd + r);
            }
        }

        if (r_to_vcc == 0.0)
            v_raw[i] = 0.0;
        else if (r_to_gnd == 0.0)
            v_raw[i] = 1.0;
        else
            v_raw[i] = r_to_gnd / (r_to_vcc + r_to_gnd);
    }

    /* Second pass: smooth the raw voltages using weighted neighbour averaging.
       The schematic resistor values do not produce perfectly linear steps,
       which results in visible banding in colour gradients. A weighted average
       with neighbouring values smooths the curve while retaining the overall
       characteristics of the resistor network. Neighbours are weighted more
       heavily than the current value for optimal smoothing.
    */
    double v_smooth[32];
    v_smooth[0] = v_raw[0];
    v_smooth[31] = v_raw[31];
    for (unsigned i = 1; i < 31; ++i) {
        v_smooth[i] =
            (v_raw[i - 1] * 1.6 + v_raw[i] + v_raw[i + 1] * 1.6) / 4.2;
    }

    /* Normalize the smoothed voltages to the 0.0-1.0 range. The endpoints are
       used as the minimum and maximum values for normalization.
    */
    double v_min = v_smooth[0];
    double v_max = v_smooth[31];

    /* Third pass: generate LUT entries using the smoothed base values with
       dark and shadow attenuation factors calculated from the resistor network.
    */
    for (unsigned i = 0; i < (1 << 5); ++i) {
        double r_to_vcc = 0.0;
        double r_to_gnd = 0.0;

        for (unsigned b = 0; b < 5; ++b) {
            double r = resistance[b];
            if (i & (1 << b)) {
                r_to_vcc =
                    (r_to_vcc == 0.0) ? r : (r_to_vcc * r) / (r_to_vcc + r);
            }
            else {
                r_to_gnd =
                    (r_to_gnd == 0.0) ? r : (r_to_gnd * r) / (r_to_gnd + r);
            }
        }

        // Scale the normalized voltage to 8-bit output range (0-255)
        double v_normalized = (v_smooth[i] - v_min) / (v_max - v_min);
        double base = v_normalized * 255.0;

        double factor_dark = 1.0;
        double factor_shad = 1.0;
        double factor_darkshad = 1.0;

        if (r_to_vcc > 0.0) {
            double v_normal =
                (r_to_gnd == 0.0) ? 1.0 : r_to_gnd / (r_to_vcc + r_to_gnd);

            /* The dark pulldown resistor is added in parallel with r_to_gnd,
               providing an additional path to ground and reducing VOUT.
            */
            double r_gnd_dark = (r_to_gnd == 0.0) ? pd_dark :
                (r_to_gnd * pd_dark) / (r_to_gnd + pd_dark);
            double v_dark = r_gnd_dark / (r_to_vcc + r_gnd_dark);

            /* The shadow pulldown works the same way as dark, but with a much
               lower resistance (150 ohms), causing a stronger darkening effect.
            */
            double r_gnd_shadow = (r_to_gnd == 0.0) ?
                pd_shadow : (r_to_gnd * pd_shadow) / (r_to_gnd + pd_shadow);
            double v_shadow = r_gnd_shadow / (r_to_vcc + r_gnd_shadow);

            /* When both are active, the two pulldown resistors are in parallel
               with each other and with r_to_gnd.
            */
            double combined = (pd_dark * pd_shadow) / (pd_dark + pd_shadow);
            double r_gnd_both = (r_to_gnd == 0.0) ?
                combined : (r_to_gnd * combined) / (r_to_gnd + combined);
            double v_both = r_gnd_both / (r_to_vcc + r_gnd_both);

            /* The ratio of darkened voltage to normal voltage gives us the
               factor by which the base colour value should be multiplied.
            */
            if (v_normal > 0.0) {
                factor_dark = v_dark / v_normal;
                factor_shad = v_shadow / v_normal;
                factor_darkshad = v_both / v_normal;
            }
        }

        /* The 6-bit LUT index has the 5-bit colour value in bits 5-1 and the
           dark bit in bit 0. When the dark bit is 1, the dark pulldown is
           active (darker output), otherwise we have normal brightness.
        */
        unsigned light = (i << 1) | 0;
        unsigned dark = (i << 1) | 1;

        lut_normal[light] = (uint8_t)(base + 0.5);
        lut_normal[dark] = (uint8_t)(base * factor_dark + 0.5);

        lut_shadow[light] = (uint8_t)(base * factor_shad + 0.5);
        lut_shadow[dark] = (uint8_t)(base * factor_darkshad + 0.5);
    }
}

static inline void geo_lspc_palconv(uint16_t addr, uint16_t data) {
    /* Colour Format
       =================================================
       |D0|R1|G1|B1|R5|R4|R3|R2|G5|G4|G3|G2|B5|B4|B3|B2|
       =================================================
       Colour values are 5 bits, accompanied by a global "dark" bit, which
       enables an 8200 ohm pulldown on each colour channel when set. For the
       "Raw" palette LUT, this bit is simply treated as the least significant
       bit for 6-bit R, G, and B values.

       An important note is that the "dark" bit is inverted, since in hardware
       this line is connected to a 74LS05 per colour channel, which inverts the
       signal -- this inversion is handled inside the LUT generators.

       Similarly, when REG_SHADOW is set, a 150 ohm pulldown is enabled on each
       colour channel. The REG_SHADOW values are stored in a separate LUT for
       simplicity.
    */
    unsigned r = (((data >> 6) & 0x3c) | ((data >> 13) & 0x02) |
        ((data >> 15) & 0x01));
    unsigned g = (((data >> 2) & 0x3c) | ((data >> 12) & 0x02) |
        ((data >> 15) & 0x01));
    unsigned b = (((data << 2) & 0x3c) | ((data >> 11) & 0x02) |
        ((data >> 15) & 0x01));

    palette_normal[addr] = 0xff000000 |
        (lut_normal[r] << 16) | (lut_normal[g] << 8) | lut_normal[b];

    palette_shadow[addr] = 0xff000000 |
        (lut_shadow[r] << 16) | (lut_shadow[g] << 8) | lut_shadow[b];
}

// Set the pointer to the video buffer
void geo_lspc_set_buffer(uint32_t *ptr) {
    vbuf = ptr;
}

// Set the sprites-per-line limit
void geo_lspc_set_sprlimit(unsigned limit) {
    sprlimit = limit;
}

void geo_lspc_set_palette(unsigned p) {
    if (p) // Raw
        geo_lspc_palgen_raw();
    else // Resistor Network
        geo_lspc_palgen_resnet();

    // Set output palette entries to match newly selected palette
    for (unsigned i = 0; i < (SIZE_16K >> 1); ++i)
        geo_lspc_palconv(i, lspc.palram[i]);
}

void geo_lspc_set_skip_render(unsigned s) {
    skip_render = s;
}

// Handle behaviour related to the Neo Geo CD REG_DISBLSPR register
void geo_lspc_disblspr_wr(unsigned e) {
    reg_disblspr = e; // Writing non-zero disables sprite layer rendering
}

// Handle behaviour related to the Neo Geo CD REG_DISBLFIX register
void geo_lspc_disblfix_wr(unsigned e) {
    reg_disblfix = e; // Writing non-zero disables FIX layer rendering
}

// Handle behaviour related to the Neo Geo CD REG_ENVIDEO register
void geo_lspc_envideo_wr(unsigned e) {
    reg_envideo = e; // 0 = Disable, 1 = Enable
}

// Perform post-load operations for C ROM
void geo_lspc_postload(void) {
    crommask = geo_calc_mask(32, romdata->csz >> 7);

    // SNKHD: pre-decodifica delle tile (16x16, ordine di visualizzazione)
    free(cdec);
    cdec = NULL;
    cdec_tiles = 0;
    if (!ngsys.cdmode && romdata->csz) {
        cdec_tiles = romdata->csz >> 7;
        cdec = (uint8_t*)malloc(cdec_tiles * 256);
        for (size_t t = 0; t < cdec_tiles; ++t) {
            const uint8_t *src = romdata->c + (t << 7);
            uint8_t *dst = cdec + (t << 8);
            for (unsigned y = 0; y < 16; ++y) {
                for (unsigned half = 0; half < 2; ++half) {
                    // display x 0-7 from offset 64, x 8-15 from offset 0
                    const uint8_t *b = src + (half ? 0 : 64) + (y << 2);
                    for (unsigned x = 0; x < 8; ++x) {
                        dst[(y << 4) + (half << 3) + x] =
                            ((b[0] >> x) & 1) | (((b[2] >> x) & 1) << 1) |
                            (((b[1] >> x) & 1) << 2) | (((b[3] >> x) & 1) << 3);
                    }
                }
            }
        }
    }
}

/* VRAM Memory Map
   ==================================================================
   | Range           | Words | Zone  | Description                  |
   ==================================================================
   | 0x0000 - 0x6fff |   28K |       | SBC1                         |
   |-------------------------|       |------------------------------|
   | 0x7000 - 0x74ff |       | Lower | Fix map                      |
   |-----------------|    4K |       |------------------------------|
   | 0x7500 - 0x7fff |       |       | Extension                    |
   ------------------------------------------------------------------
   | 0x8000 - 0x81ff |   512 |       | SBC2                         |
   |-----------------|-------|       |------------------------------|
   | 0x8200 - 0x83ff |   512 |       | SBC3                         |
   |-----------------|-------|       |------------------------------|
   | 0x8400 - 0x85ff |   512 | Upper | SBC4                         |
   |-----------------|-------|       |------------------------------|
   | 0x8600 - 0x867f |   128 |       | Sprite list (even scanlines) |
   |-----------------|-------|       |------------------------------|
   | 0x8680 - 0x86ff |   128 |       | Sprite list (odd scanlines)  |
   ------------------------------------------------------------------
   | 0x8700 - 0x87ff |   256 | Unused                               |
   ------------------------------------------------------------------
*/

// Return the backdrop colour -- the last value in the active palette bank
static inline uint32_t geo_lspc_backdrop(void) {
    return palette[(lspc.palbank * SIZE_4K) + 4095];
}

// Draw a line of backdrop and pre-calculated sprite pixels
static inline void geo_lspc_bdsprline(void) {
    uint32_t bdcol = geo_lspc_backdrop();
    uint32_t *ptr = vbuf + (lspc.scanline * LSPC_WIDTH);
    uint32_t *lb = linebuf[lbactive];
    if (geo_lspc_idmap && lspc.scanline >= LSPC_LINE_VISIBLE && lspc.scanline < LSPC_LINE_VISIBLE + 224) {
        memcpy(geo_lspc_idmap + (size_t)(lspc.scanline - LSPC_LINE_VISIBLE) * LSPC_WIDTH, dbglb[lbactive], sizeof dbglb[0]);
        memset(dbglb[lbactive], 0, sizeof dbglb[0]);
    }
    unsigned bl = blendline[lbactive];
    blendline[lbactive] = 0;

    if (!bl)
        for (unsigned p = 0; p < LSPC_WIDTH; ++p) {
            ptr[p] = lb[p] ? palette[lb[p]] : bdcol;
            lb[p] = 0;
        }
    else {
        uint32_t *un = lbund[lbactive];
        for (unsigned p = 0; p < LSPC_WIDTH; ++p) {
            uint32_t v = lb[p];
            ptr[p] = !v ? bdcol : (v >> 16) ? bld_res(v, un[p], bdcol) | 0xff000000 : palette[v];
            lb[p] = 0;
        }
    }

    unsigned rh = hd_on ? vrh[lspc.scanline] : 0;
    if (hd_on) {
        unsigned w = hd_w;
        uint32_t *hl0 = hd_lb[lbactive];
        uint32_t *hu0 = hd_und[lbactive];
        for (unsigned k = 0; k < rh; ++k) {
            uint32_t *hp = hd_vbuf + (size_t)(vry[lspc.scanline] + k) * w;
            uint32_t *hl = hl0 + k * w;
            uint32_t *hu = hu0 + k * w;
            // Sotto-riga uguale alla precedente (caso comune): copia diretta
            if (k && !memcmp(hl, hl - w, w * sizeof(uint32_t)) &&
                (!bl || !memcmp(hu, hu - w, w * sizeof(uint32_t)))) {
                memcpy(hp, hp - w, w * sizeof(uint32_t));
                continue;
            }
            if (!bl)
                for (unsigned p = 0; p < w; ++p) {
                    unsigned v = hl[p];
                    uint32_t c = palette[v];
                    hp[p] = v ? c : bdcol;
                }
            else
                for (unsigned p = 0; p < w; ++p) {
                    uint32_t v = hl[p];
                    hp[p] = !v ? bdcol : (v >> 16) ? bld_res(v, hu[p], bdcol) | 0xff000000 : palette[v];
                }
        }
        memset(hl0, 0, (size_t)w * hd_maxrh * sizeof(uint32_t));
    }
}

// SNKHD: copia la riga del fix layer (fixrow) nel buffer nativo e in quello HD
static inline void geo_lspc_fixcompose(void) {
    uint32_t *ptr = vbuf + (lspc.scanline * LSPC_WIDTH);
    if (geo_lspc_idmap && lspc.scanline >= LSPC_LINE_VISIBLE && lspc.scanline < LSPC_LINE_VISIBLE + 224) {
        uint64_t *d = geo_lspc_idmap + (size_t)(lspc.scanline - LSPC_LINE_VISIBLE) * LSPC_WIDTH;
        for (unsigned p = 0; p < LSPC_WIDTH; ++p) if (fixrow[p]) d[p] = dbgfix[p];
    }
    if (fixblend_on) { // SNKHD: fix layer con pixel trasparenti
        unsigned rh = hd_on ? vrh[lspc.scanline] : 0;
        for (unsigned p = 0; p < LSPC_WIDTH; ++p) {
            if (!fixrow[p]) continue;
            uint32_t c = fixrow[p], w = fixw[p];
            ptr[p] = w ? bld_mix(ptr[p], c, w) | 0xff000000 : c;
            for (unsigned k = 0; k < rh; ++k) {
                uint32_t *hp = hd_vbuf + (size_t)(vry[lspc.scanline] + k) * hd_w;
                for (unsigned q = hcx[p]; q < hcx[p + 1]; ++q)
                    hp[q] = w ? bld_mix(hp[q], c, w) | 0xff000000 : c;
            }
        }
        return;
    }
    for (unsigned p = 0; p < LSPC_WIDTH; ++p)
        if (fixrow[p]) ptr[p] = fixrow[p];

    unsigned rh = hd_on ? vrh[lspc.scanline] : 0;
    for (unsigned k = 0; k < rh; ++k) {
        uint32_t *hp = hd_vbuf + (size_t)(vry[lspc.scanline] + k) * hd_w;
        for (unsigned p = 0; p < LSPC_WIDTH; ++p) {
            if (fixrow[p]) {
                uint32_t c = fixrow[p];
                for (unsigned q = hcx[p]; q < hcx[p + 1]; ++q)
                    hp[q] = c;
            }
        }
    }
}

/* SNKHD: imposta il buffer HD e la sua dimensione (immagine visibile 320x224
   portata a w x h, scala anche non intera). w o h = 0 disattiva l'HD. */
void geo_lspc_set_hd(uint32_t *buf, unsigned w, unsigned h) {
    for (unsigned i = 0; i < 2; ++i) {
        free(hd_lb[i]);
        hd_lb[i] = NULL;
        free(hd_und[i]);
        hd_und[i] = NULL;
    }
    hd_vbuf = buf;
    hd_on = (buf && w >= LSPC_WIDTH && h >= LSPC_HEIGHT_VISIBLE);
    if (w > LSPC_WIDTH * 16) w = LSPC_WIDTH * 16;
    if (h > LSPC_HEIGHT_VISIBLE * 16) h = LSPC_HEIGHT_VISIBLE * 16;
    hd_w = w; hd_h = h;
    for (unsigned h2 = 0; h2 < 16; ++h2) {
        unsigned n = 0;
        for (unsigned p = 0; p < 16; ++p)
            if (lut_hshrink[h2][p])
                hd_hcol[h2][n++] = p;
        hd_hcol[h2][n] = 16;
    }
    for (unsigned cw = 1; cw <= 16; ++cw)
        for (unsigned dc = 0; dc <= 16; ++dc)
            for (unsigned k = 0; k < cw; ++k)
                offt[cw][dc][k] = (uint8_t)((dc * k) / cw);
    if (!hd_on)
        return;
    for (unsigned x = 0; x <= LSPC_WIDTH; ++x)
        hcx[x] = (uint16_t)(((uint64_t)x * w) / LSPC_WIDTH);
    free(xq);
    xq = (uint32_t*)malloc(sizeof(uint32_t) * w);
    for (unsigned X = 0; X < w; ++X)
        xq[X] = (uint32_t)(((uint64_t)(2 * X + 1) * LSPC_WIDTH * 128) / w);
    hd_maxrh = 0;
    for (unsigned r = 0; r < LSPC_SCANLINES; ++r) {
        vry[r] = 0; vrh[r] = 0;
        if (r >= LSPC_LINE_VISIBLE && r < LSPC_LINE_VISIBLE + LSPC_HEIGHT_VISIBLE) {
            unsigned v = r - LSPC_LINE_VISIBLE;
            unsigned y0 = (unsigned)(((uint64_t)v * h) / LSPC_HEIGHT_VISIBLE);
            unsigned y1 = (unsigned)(((uint64_t)(v + 1) * h) / LSPC_HEIGHT_VISIBLE);
            vry[r] = (uint16_t)y0; vrh[r] = (uint8_t)(y1 - y0);
            if (y1 - y0 > hd_maxrh) hd_maxrh = y1 - y0;
        }
    }
    for (unsigned i = 0; i < 2; ++i) {
        hd_lb[i] = (uint32_t*)calloc((size_t)w * hd_maxrh, sizeof(uint32_t));
        hd_und[i] = (uint32_t*)calloc((size_t)w * hd_maxrh, sizeof(uint32_t));
    }
}

unsigned geo_lspc_get_hd(void) {
    return hd_on;
}

// Read half of a a palette RAM entry from the active bank
uint8_t geo_lspc_palram_rd08(uint32_t addr) {
    uint16_t pval =
        lspc.palram[((addr >> 1) & 0x0fff) + (lspc.palbank * SIZE_4K)];
    return 0xff & (addr & 0x01 ? pval : pval >> 8);
}

// Read a palette RAM entry from the active bank
uint16_t geo_lspc_palram_rd16(uint32_t addr) {
    return lspc.palram[((addr >> 1) & 0x0fff) + (lspc.palbank * SIZE_4K)];
}

// Write half of a value to the active bank of palette RAM
void geo_lspc_palram_wr08(uint32_t addr, uint8_t data) {
    addr >>= 1; // The address should access 16-bit values rather than bytes

    if (addr & 0x01) {
        lspc.palram[(addr & 0x0fff) + (lspc.palbank * SIZE_4K)] &= 0xff00;
        lspc.palram[(addr & 0x0fff) + (lspc.palbank * SIZE_4K)] |= data;
    }
    else {
        lspc.palram[(addr & 0x0fff) + (lspc.palbank * SIZE_4K)] &= 0x00ff;
        lspc.palram[(addr & 0x0fff) + (lspc.palbank * SIZE_4K)] |= (data << 8);
    }

    unsigned idx = (addr & 0x0fff) + (lspc.palbank * SIZE_4K);
    geo_lspc_palconv(idx, lspc.palram[idx]);
}

// Write a value to the active bank of palette RAM
void geo_lspc_palram_wr16(uint32_t addr, uint16_t data) {
    addr >>= 1; // The address should access 16-bit values rather than bytes
    lspc.palram[(addr & 0x0fff) + (lspc.palbank * SIZE_4K)] = data;
    geo_lspc_palconv((addr & 0x0fff) + (lspc.palbank * SIZE_4K), data);
}

// Set the active palette bank
void geo_lspc_palram_bank(unsigned bank) {
#ifdef SNKHD_HOOK
    { static int on = -1; if (on < 0) on = getenv("PBLOG") != NULL;   // debug: cambi del banco palette con PC
      if (on && bank != lspc.palbank) { extern int snkhd_frame;
        fprintf(stderr, "palbank %u f%d pc %06x\n", bank, snkhd_frame, m68k_get_reg(NULL, M68K_REG_PPC)); } }
#endif
    lspc.palbank = bank;
}

// Write the VRAM Address to work with
void geo_lspc_vramaddr_wr(uint16_t addr) {
    lspc.vramaddr = addr;
    lspc.vrambank = addr & 0x8000;
}

// Read from VRAM
uint16_t geo_lspc_vram_rd(void) {
    return lspc.vram[lspc.vramaddr];
}

// Write to VRAM
#ifdef SNKHD_HOOK
#include <stdio.h>
unsigned snkhd_vw_lo = 1, snkhd_vw_hi = 0; long snkhd_vw_max = 300; // debug: VWATCH=lo-hi (indirizzi VRAM)
#include "m68k/m68k.h"
#endif
void geo_lspc_vram_wr(uint16_t data) {
#ifdef SNKHD_HOOK
    if (lspc.vramaddr >= snkhd_vw_lo && lspc.vramaddr <= snkhd_vw_hi && snkhd_vw_max > 0) {
        snkhd_vw_max--;
        printf("vwr %04x=%04x pc %06x\n", lspc.vramaddr, data, m68k_get_reg(NULL, M68K_REG_PPC));
    }
#endif
    // Writing beyond the boundary is not a winning endeavour
    if (lspc.vramaddr < 0x8800)
        lspc.vram[lspc.vramaddr] = data; // Perform the write

    // Apply the modulo after the write, wrapping within the correct bank
    lspc.vramaddr = ((lspc.vramaddr + lspc.vrammod) & 0x7fff) | lspc.vrambank;
}

// Return REG_VRAMMOD as a 16-bit unsigned integer
uint16_t geo_lspc_vrammod_rd(void) {
    return lspc.vrammod;
}

// Write a value to REG_VRAMMOD as a 16-bit signed integer
void geo_lspc_vrammod_wr(int16_t mod) {
    lspc.vrammod = mod;
}

// Read REG_LSPCMODE
uint16_t geo_lspc_mode_rd(void) {
    /* Bits 7-15: Raster line counter (with offset of 0xf8)
       Bits 4-6:  000
       Bit 3:     0 = 60Hz, 1 = 50Hz
       Bits 0-2:  Auto animation counter
    */
    // FIXME - support 50Hz
    return ((lspc.scanline + 0xf8) << 7) | lspc.aa_counter;
}

// Write to REG_LSPCMODE
void geo_lspc_mode_wr(uint16_t data) {
    /* Bits 8-15: Auto animation speed
       Bits 5-7:  Timer interrupt mode
       Bit 4:     Timer interrupt enable
       Bit 3:     Disable auto animation
       Bits 0-2:  Unused
    */
    lspc.aa_reload = (data >> 8);
    lspc.aa_disable = data & 0x08;

    /* When the "Timer IRQ Reload at VBLANK" bit is freshly set during the
       VBLANK period, reload the counter immediately.
    */
    uint8_t prev_ctrl = ngsys.irq2_ctrl;
    ngsys.irq2_ctrl = data & 0xf0;

    if (!(prev_ctrl & IRQ_TIMER_RELOAD_VBLANK) &&
        (ngsys.irq2_ctrl & IRQ_TIMER_RELOAD_VBLANK) &&
        lspc.scanline >= LSPC_LINE_BORDER_BOTTOM) {
        ngsys.irq2_counter = ngsys.irq2_reload;
    }
}

// Write to REG_SHADOW
void geo_lspc_shadow_wr(unsigned s) {
    lspc.shadow = s;
    palette = s ? palette_shadow : palette_normal;
}

void geo_lspc_init(void) {
    lspc.palbank = 0;
    lspc.vramaddr = 0;
    lspc.vrammod = 0;
    lspc.aa_counter = 0;
    lspc.aa_disable = 0;
    lspc.aa_reload = 0;
    lspc.aa_timer = 0;
    lspc.scanline = 0;
    lspc.cyc = 0;

    // CD register copies
    reg_disblspr = 0;
    reg_disblfix = 0;
    reg_envideo = 0;

    geo_lspc_shadow_wr(0);

    romdata = geo_romdata_ptr();
}

// No Fix Layer Banking (Default)
static void geo_lspc_fixline_default(void) {
    /* The "fix map" is located in VRAM from 0x7000 to 0x74ff. Each map entry
       is a 16-bit value, where the bottom 12 bits are the tile number and the
       top 4 bits are the palette number. This limits the fix layer to the
       first 16 palettes of whatever palette bank is active. Tiles are mapped
       from top to bottom, 40 columns of 32 rows.

       Map Entry
       =======================================================================
       | 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
       -----------------------------------------------------------------------
       | Palette Number    | Tile Number                                     |
       -----------------------------------------------------------------------

       Fix Map
       =======================================================================
       | 0x7000 | 0x7020 | 0x7040 |   ........    | 0x74a0 | 0x74c0 | 0x74e0 |
       | 0x7001 | 0x7021 | 0x7041 |   ........    | 0x74a1 | 0x74c1 | 0x74e1 |
       |                              ........                               |
       | 0x701f | 0x703f | 0x705f |   ........    | 0x74bf | 0x74df | 0x74ff |
       -----------------------------------------------------------------------
    */
    unsigned line = lspc.scanline - LSPC_LINE_BORDER_TOP;

    for (unsigned x = 0; x < LSPC_FIXTILES_H; ++x) {
        // Addresses increment vertically downwards
        uint16_t entry = lspc.vram[0x7000 + (line >> 3) + (x << 5)];

        /* The 4 bits making up the palette number can be easily used to create
           an offset into palette memory by virtue of palettes being 16 values
           (a power of 2). The offset can be used in conjunction with the tile
           data to determine the exact value in palette memory that defines the
           final output pixel's colour.
        */
        unsigned poffset = ((entry >> 8) & 0xf0) + (lspc.palbank * SIZE_4K);

        /* Tiles are 8x8 pixels, with 4 bits representing a pixel --  in other
           words, representing the entry in a 16-bit palette. The pixel data is
           arranged in columns, with the second vertical half of the tile
           defined first:
           0x10, 0x18, 0x00, 0x08
           0x11, 0x19, 0x01, 0x09
           ....
           0x17, 0x1f, 0x07, 0x0f

           Each tile is 4 bytes wide and 8 bytes tall, for a total of 32 bytes.
        */
        unsigned tnum = entry & 0x0fff;
        uint8_t *tdata = &fixdata[tnum << 5];

        // Offset into video buffer to draw the tile row into
        uint32_t *voffset =
            fixrow + (x << 3);

        // Row in the 8 pixel high tile
        unsigned row = line & 0x07;

        // If the palette entry is non-zero, output a colour
        if (geo_lspc_idmap)
            for (unsigned q = 0; q < 8; ++q) dbgfix[(x << 3) + q] = (1ull << 63) | ((uint64_t)(entry >> 12) << 32) | tnum;
        if (fixblend_on) { // SNKHD: peso di trasparenza della tile fix
            unsigned fw = fixpalw[entry >> 12];
            if (!fw && tnum < fixtilen) fw = fixtilew[tnum];
            memset(fixw + (x << 3), (int)fw, 8);
        }
        uint32_t pentry = 0;
        for (unsigned p = 0, f = 0x10; p < 4; ++p, f = (f + 0x08) & 0x18) {
            pentry = tdata[f + row] & 0x0f;
            if (pentry) voffset[p << 1] = palette[poffset + pentry];

            pentry = (tdata[f + row] >> 4) & 0x0f;
            if (pentry) voffset[(p << 1) + 1] = palette[poffset + pentry];
        }
    }
}

// Per-Line Banking (Type 1)
static void geo_lspc_fixline_line(void) {
    /* Most everything is the same as the default fix layer drawing routine,
       with the exception of banking offsets which work for a minimum of two
       rows whenever switched. The documentation on this is less than ideal
       at the time this code was written, and the algorithm was shamelessly
       taken from MAME. This is an area where more reverse engineering would
       be beneficial, but it works well enough for the three games using it to
       run without issue.
    */
    unsigned line = lspc.scanline - LSPC_LINE_BORDER_TOP;
    unsigned trow = line >> 3; // Tile row

    unsigned offsets[34];
    unsigned bank = 0;
    unsigned k = 0;
    unsigned y = 0;
    while (y < 32) {
        /* A value of 0x0200 in the 0x7500-0x753f block indicates a bank switch
           should be done. A corresponding value in 0x7580-0x75bf containing
           all 1s in the upper 8 bits will determine the bank using the
           complement of the lowest 3 bits.
        */
        if (lspc.vram[0x7500 + k] == 0x0200 &&
            (lspc.vram[0x7580 + k] & 0xff00) == 0xff00) {
            // Choose one of 4 banks of 4096 tiles
            bank = (~lspc.vram[0x7580 + k] & 0x03) * SIZE_4K;
            offsets[y++] = bank;
        }
        offsets[y++] = bank;
        k += 2;
    }

    for (unsigned x = 0; x < LSPC_FIXTILES_H; ++x) {
        uint16_t entry = lspc.vram[0x7000 + trow + (x << 5)];
        unsigned poffset = ((entry >> 8) & 0xf0) + (lspc.palbank * SIZE_4K);
        unsigned tnum = (entry & 0x0fff) + offsets[(trow - 2) & 0x1f];
        uint8_t *tdata = &fixdata[tnum << 5];
        uint32_t *voffset =
            fixrow + (x << 3);
        unsigned row = line & 0x07;
        if (geo_lspc_idmap)
            for (unsigned q = 0; q < 8; ++q) dbgfix[(x << 3) + q] = (1ull << 63) | ((uint64_t)(entry >> 12) << 32) | tnum;
        if (fixblend_on) { // SNKHD: peso di trasparenza della tile fix
            unsigned fw = fixpalw[entry >> 12];
            if (!fw && tnum < fixtilen) fw = fixtilew[tnum];
            memset(fixw + (x << 3), (int)fw, 8);
        }
        uint32_t pentry = 0;
        for (unsigned p = 0, f = 0x10; p < 4; ++p, f = (f + 0x08) & 0x18) {
            pentry = tdata[f + row] & 0x0f;
            if (pentry) voffset[p << 1] = palette[poffset + pentry];

            pentry = (tdata[f + row] >> 4) & 0x0f;
            if (pentry) voffset[(p << 1) + 1] = palette[poffset + pentry];
        }
    }
}

// Per-Tile Banking (Type 2)
static void geo_lspc_fixline_tile(void) {
    unsigned line = lspc.scanline - LSPC_LINE_BORDER_TOP;
    unsigned trow = line >> 3; // Tile row

    for (unsigned x = 0; x < LSPC_FIXTILES_H; ++x) {
        uint16_t entry = lspc.vram[0x7000 + (line >> 3) + (x << 5)];
        unsigned poffset = ((entry >> 8) & 0xf0) + (lspc.palbank * SIZE_4K);

        /* The lower 12 bits of 16-bit words stored at 0x7500-0x75df are used
           to control the bank offset for 6 consecutive horizontal tiles -- 2
           bits each, complemented, select one of 4 banks of 4096 tiles. This
           algorithm is shamelessly taken from MAME.
        */
        unsigned offset =
            ~(lspc.vram[0x7500 + ((trow - 1) & 0x1f) + 32 * (x / 6)] >>
            (5 - (x % 6)) * 2) & 0x03;
        unsigned tnum = (entry & 0x0fff) + (offset * SIZE_4K);
        uint8_t *tdata = &fixdata[tnum << 5];

        // Offset into video buffer to draw the tile row into
        uint32_t *voffset =
            fixrow + (x << 3);

        // Row in the 8 pixel high tile
        unsigned row = line & 0x07;

        // If the palette entry is non-zero, output a colour
        if (geo_lspc_idmap)
            for (unsigned q = 0; q < 8; ++q) dbgfix[(x << 3) + q] = (1ull << 63) | ((uint64_t)(entry >> 12) << 32) | tnum;
        if (fixblend_on) { // SNKHD: peso di trasparenza della tile fix
            unsigned fw = fixpalw[entry >> 12];
            if (!fw && tnum < fixtilen) fw = fixtilew[tnum];
            memset(fixw + (x << 3), (int)fw, 8);
        }
        uint32_t pentry = 0;
        for (unsigned p = 0, f = 0x10; p < 4; ++p, f = (f + 0x08) & 0x18) {
            pentry = tdata[f + row] & 0x0f;
            if (pentry) voffset[p << 1] = palette[poffset + pentry];

            pentry = (tdata[f + row] >> 4) & 0x0f;
            if (pentry) voffset[(p << 1) + 1] = palette[poffset + pentry];
        }
    }
}

// Set the active Fix ROM
void geo_lspc_set_fix(unsigned f) {
    switch (f) {
        default: case LSPC_FIX_BOARD: {
            fixdata = romdata->sfix;
            geo_lspc_fixline = &geo_lspc_fixline_default;
            break;
        }
        case LSPC_FIX_CART: {
            fixdata = romdata->s;
            geo_lspc_set_fix_banksw(fixbanksw);
            break;
        }
        case LSPC_FIX_CD: { // CD systems always use .FIX files from the CD
            fixdata = romdata->s;
            geo_lspc_fixline = &geo_lspc_fixline_default;
            break;
        }
    }
}

// Set the Fix Layer bank switching type
void geo_lspc_set_fix_banksw(unsigned f) {
    /* Type 0: None
       Type 1: Per-line banking
       Type 2: Per-tile banking
    */
    fixbanksw = f;

    switch (f) {
        default: {
            geo_lspc_fixline = &geo_lspc_fixline_default; break;
        }
        case FIX_BANKSW_LINE: {
            geo_lspc_fixline = &geo_lspc_fixline_line; break;
        }
        case FIX_BANKSW_TILE: {
            geo_lspc_fixline = &geo_lspc_fixline_tile; break;
        }
    }
}

static inline unsigned geo_lspc_tpix(unsigned tbase, unsigned x, unsigned y) {
    /* Sprite Tile Decoding
       Tiles are 8x8 with palette index values represented in a planar format,
       which spans 4 bytes per horizontal line. Each bit in a byte represents
       one component of a 4-bit palette entry:
       =================================
       | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | Take the tile's base address in C ROM
       ================================= and shift right by the X offset in the
    v0 | 1 | 1 | 0 | 1 | 0 | 0 | 0 | 1 | tile to isolate the correct bit. Do
    v1 | 0 | 1 | 0 | 0 | 0 | 1 | 0 | 1 | this for 4 consecutive bytes, then
    v2 | 1 | 0 | 1 | 1 | 0 | 0 | 1 | 1 | shift and OR the bits together to
    v3 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | create a 4-bit palette entry.
       ---------------------------------

       Bytes 0 and 1 come from odd numbered C ROMs, while bytes 2 and 3 come
       from even numbered C ROMs. Because the data from the C ROM pairs are
       interleaved every byte, the second byte is in the third position, and
       the third byte is in the second position.

       Notes: Since there are 4 bytes per row of pixels, multiply the tile's
              base address by 4 to select the specific row in the tile.
              The pixel data is stored right to left -- this means it is in
              reverse order from how it is displayed unless horizontal flip
              is enabled for the tile.
    */
    unsigned base = tbase + (y << 2);
    unsigned v0, v1, v2, v3;
    if (ngsys.cdmode) { // CD SPR DRAM: Non-interleaved byte order [1, 0, 3, 2]
        v0 = (romdata->c[base + 1]) >> (x);
        v1 = (romdata->c[base + 0]) >> (x);
        v2 = (romdata->c[base + 3]) >> (x);
        v3 = (romdata->c[base + 2]) >> (x);
    }
    else { // Cart C ROM: Interleaved odd/even byte order [0, 2, 1, 3]
        v0 = (romdata->c[base + 0]) >> (x);
        v1 = (romdata->c[base + 2]) >> (x);
        v2 = (romdata->c[base + 1]) >> (x);
        v3 = (romdata->c[base + 3]) >> (x);
    }
    return (v0 & 0x01) | (v1 & 0x01) << 1 | (v2 & 0x01) << 2 | (v3 & 0x01) << 3;
}

// SNKHD: riga della sprite (0-511, dopo lo shrink verticale) mostrata su "line"
static inline int geo_lspc_maprow(unsigned line, unsigned ypos,
    unsigned sprsize, unsigned vshrink, unsigned *out) {
    unsigned srow = (line - (0x200 - ypos)) & 0x1ff;
    if ((srow >= (sprsize << 4)) || (sprsize == 0))
        return 0;

    /* L0 ROM: 256 tables of 256 bytes selected by vshrink; the bottom half
       of a 32-tile sprite uses the complemented index (see original notes).
       Size 33 loops the borders when shrinking. */
    unsigned invert = srow > 0xff;
    unsigned zrow = srow & 0xff;
    if (invert)
        zrow ^= 0xff;

    if (sprsize == 33) {
        zrow = zrow % ((vshrink + 1) << 1);
        if (zrow > vshrink) {
            zrow = ((vshrink + 1) << 1) - 1 - zrow;
            invert ^= 1;
        }
    }

    srow = romdata->l0[(vshrink << 8) + zrow];
    if (invert)
        srow ^= 0x1ff;

    *out = srow;
    return 1;
}

/* SNKHD: riga sorgente della sotto-riga k (di rh) della linea "line" con
   campionamento uniforme: la sprite ridotta viene ricampionata come
   un'immagine (niente righe scelte in modo irregolare dalla tabella L0). */
static inline int geo_lspc_maprow_hd(unsigned line, unsigned ypos,
    unsigned sprsize, unsigned vshrink, unsigned k, unsigned rh, unsigned *out) {
    unsigned srow = (line - (0x200 - ypos)) & 0x1ff;
    if ((srow >= (sprsize << 4)) || (sprsize == 0))
        return 0;
    if (sprsize == 33 || vshrink == 0xff)
        return geo_lspc_maprow(line, ypos, sprsize, vshrink, out);
    unsigned invert = srow > 0xff;
    unsigned z = srow & 0xff;
    unsigned den = 2 * (vshrink + 1) * rh;
    unsigned s;
    if (!invert) {
        s = ((2 * (z * rh + k) + 1) * 256) / den;
        if (s > 255) s = 255;
        *out = s;
    }
    else {
        z ^= 0xff;
        s = ((2 * ((z + 1) * rh - k) - 1) * 256) / den;
        if (s > 255) s = 255;
        *out = 0x1ff - s;
    }
    return 1;
}

// SNKHD: decodifica i 16 pixel (ordine di visualizzazione) della riga srow
static unsigned dec_w; // peso di trasparenza della tile decodificata (0 = opaca)
static inline void geo_lspc_decrow(unsigned i, unsigned srow,
    unsigned pix[16], unsigned *poff) {
    // Tile map entry pair for this tile of sprite i
    unsigned tmapoffset = (i << 6) + ((srow >> 3) & 0x3e);
    unsigned attr = lspc.vram[tmapoffset + 1];

    unsigned tnum = (lspc.vram[tmapoffset] | ((attr & 0x00f0) << 12)) &
        crommask;

    unsigned hflip = attr & 0x01;
    unsigned vflip = attr & 0x02;
    unsigned aabits = (attr & 0x0c) >> 2;

    *poff = ((attr >> 4) & 0x0ff0) + (lspc.palbank * SIZE_4K);

    // Auto-animation: counter replaces the bottom 2 or 3 bits of the tile
    if (!lspc.aa_disable) {
        switch (aabits) {
            case 1: tnum &= ~0x03; tnum |= lspc.aa_counter & 0x03; break;
            case 2: case 3: tnum &= ~0x07; tnum |= lspc.aa_counter & 0x07; break;
            default: break;
        }
    }

    unsigned toffset = (tnum << 7) % romdata->csz;
    dec_w = (toffset >> 7) < blendn ? blendtab[toffset >> 7] : 0; // SNKHD: trasparenza per tile
    if (!dec_w && blendpal_on)
        dec_w = blendpal[(attr >> 8) & 0xff];                     // ... o per palette
    unsigned y = vflip ? (0x0f - (srow & 0x0f)) : (srow & 0x0f);
    unsigned ftile = hflip ? 0x00 : 0x08;
    unsigned fpix = hflip ? 0x07 : 0x00;

    if (cdec) {
        const uint8_t *row = cdec + ((size_t)(toffset >> 7) << 8) + (y << 4);
        if (hflip)
            for (unsigned p = 0; p < 16; ++p) pix[p] = row[15 - p];
        else
            for (unsigned p = 0; p < 16; ++p) pix[p] = row[p];
        return;
    }
    for (unsigned p = 0; p < 16; ++p)
        pix[p] = geo_lspc_tpix(toffset + (((0x08 & p) ^ ftile) << 3),
            (p & 0x07) ^ fpix, y);
}

// Calculate a line of sprite data 2 lines in advance
static inline void geo_lspc_sprcalc(void) {
    unsigned line = lspc.scanline - LSPC_LINE_BUFSTART;
    unsigned sprcount = 0; // Counter for sprite limit

    unsigned xpos = 0;
    unsigned ypos = 0;
    unsigned sprsize = 0;
    unsigned hshrink = 0x0f; // Start at full width (no shrinking)
    unsigned vshrink = 0xff; // Start at full height (no shrinking)

    for (unsigned i = 1; i < 382; ++i) {
        if (lspc.vram[0x8200 + i] & 0x40) { // Sticky/Chain bit set
            // Attach to the edge of the previous sprite (its hshrink)
            xpos += (hshrink + 1);
        }
        else {
            xpos = (lspc.vram[0x8400 + i] >> 7) & 0x1ff;
            ypos = (lspc.vram[0x8200 + i] >> 7) & 0x1ff; // 512-Y
            sprsize = lspc.vram[0x8200 + i] & 0x3f; // Height in tiles
            vshrink = lspc.vram[0x8000 + i] & 0xff;
        }

        // Set horizontal shrinking value
        hshrink = (lspc.vram[0x8000 + i] >> 8) & 0x0f;

        unsigned srow;
        if (!geo_lspc_maprow(line, ypos, sprsize, vshrink, &srow))
            continue;

        // The LSPC hardware has a 96 sprite hard limit
        if (sprcount++ == sprlimit)
            break;

        unsigned pix[16];
        unsigned poffset;
        geo_lspc_decrow(i, srow, pix, &poffset);
        unsigned pw = dec_w;
        if (pw) blendline[lbactive] = 1;

        // Native line: draw only the columns kept by the horizontal shrink
        unsigned drawpos = 0;
        for (unsigned p = 0; p < 16; ++p) {
            if (lut_hshrink[hshrink][p]) {
                unsigned xcoord = (xpos + drawpos) & 0x1ff;
                if (pix[p] && (xcoord < LSPC_WIDTH)) {
                    if (pw)
                        bld_plot(&linebuf[lbactive][xcoord], &lbund[lbactive][xcoord], poffset + pix[p], pw);
                    else
                        linebuf[lbactive][xcoord] = poffset + pix[p];
                    if (geo_lspc_idmap) {
                        unsigned tmo = (i << 6) + ((srow >> 4) << 1), at = lspc.vram[tmo + 1];
                        dbglb[lbactive][xcoord] = ((uint64_t)i << 40) | ((uint64_t)((at >> 8) & 0xff) << 32) |
                            ((lspc.vram[tmo] | ((at & 0xf0) << 12)) & crommask);
                    }
                }
                ++drawpos; // Increment for coloured and transparent pixels
            }
        }

        if (!hd_on)
            continue;
        // righe HD della riga di schermo in cui comparira' questa linea (2 dopo)
        unsigned dr = lspc.scanline + 2;
        unsigned rh = dr < LSPC_SCANLINES ? vrh[dr] : 0;
        if (!rh)
            continue;

        /* HD: ricampionamento uniforme della sprite alla risoluzione dello
           schermo (righe e colonne), come un'immagine scalata. Le sprite non
           ridotte restano identiche (pixel replicati). */
        unsigned w = hshrink + 1;
        unsigned wspan = w << 8;                 // larghezza in 1/256 di pixel
        int contig = (xpos + w) <= LSPC_WIDTH;
        uint8_t cmap[16 * 16];                   // colonna sorgente per ogni colonna HD
        unsigned hlen = 0;
        if (contig) {
            unsigned X0 = hcx[xpos], X1 = hcx[xpos + w];
            for (unsigned X = X0; X < X1; ++X) {
                int u = (int)xq[X] - (int)(xpos << 8);
                int col = u <= 0 ? 0 : (int)(((unsigned)u * 16) / wspan);
                cmap[hlen++] = (uint8_t)(col > 15 ? 15 : col);
            }
        }
        uint32_t hrow[16 * 16];
        unsigned hw = pw;
        unsigned lastrow = ~0u;
        unsigned hpix[16];
        unsigned hpoff = poffset;
        const unsigned *pp = pix;

        for (unsigned k = 0; k < rh; ++k) {
            unsigned r;
            if (!geo_lspc_maprow_hd(line, ypos, sprsize, vshrink, k, rh, &r))
                r = srow;
            uint32_t *hl = hd_lb[lbactive] + (size_t)k * hd_w;
            uint32_t *hu = hd_und[lbactive] + (size_t)k * hd_w;
            if (r != lastrow) {
                if (r != srow) {
                    geo_lspc_decrow(i, r, hpix, &hpoff);
                    hw = dec_w;
                    if (hw) blendline[lbactive] = 1;
                    pp = hpix;
                }
                else {
                    pp = pix;
                    hpoff = poffset;
                    hw = pw;
                }
                lastrow = r;
                if (contig)
                    for (unsigned j = 0; j < hlen; ++j) {
                        unsigned pe = pp[cmap[j]];
                        hrow[j] = pe ? (hpoff + pe) | (hw << 16) : 0;
                    }
            }
            if (contig) {
                uint32_t *q = hl + hcx[xpos];
                if (!hw) {
                    for (unsigned j = 0; j < hlen; ++j)
                        if (hrow[j]) q[j] = hrow[j];
                }
                else {
                    uint32_t *qu = hu + hcx[xpos];
                    for (unsigned j = 0; j < hlen; ++j)
                        if (hrow[j]) bld_plot(&q[j], &qu[j], hrow[j] & 0xffff, hrow[j] >> 16);
                }
                continue;
            }
            for (unsigned c = 0; c < w; ++c) {
                unsigned xn = (xpos + c) & 0x1ff;
                if (xn >= LSPC_WIDTH)
                    continue;
                for (unsigned X = hcx[xn]; X < hcx[xn + 1]; ++X) {
                    int u = (int)xq[X] - (int)(xn << 8) + (int)(c << 8);
                    int col = u <= 0 ? 0 : (int)(((unsigned)u * 16) / wspan);
                    unsigned pe = pp[col > 15 ? 15 : col];
                    if (pe)
                    {
                        if (hw) bld_plot(&hl[X], &hu[X], hpoff + pe, hw);
                        else hl[X] = hpoff + pe;
                    }
                }
            }
        }
    }

    // Flip the active line buffer once this one has been filled with new data
    lbactive ^= 1;
}

static inline void geo_lspc_aa(void) {
    /* An internal 8-bit timer is ticked down each frame. When it underflows,
       two things happen:
       - The timer is reloaded with the 8 highest bits of REG_LSPCMODE
       - An internal 3-bit animation counter is incremented
    */
    if (--lspc.aa_timer == 0xff) {
        lspc.aa_counter = (lspc.aa_counter + 1) & 0x07;
        lspc.aa_timer = lspc.aa_reload;
    }
}

static void geo_lspc_scanline(void) {
    if (skip_render || (ngsys.cdmode && !reg_envideo))
        return;

    if (lspc.scanline >= LSPC_LINE_BORDER_TOP &&
        lspc.scanline < LSPC_LINE_BORDER_BOTTOM) {
        geo_lspc_bdsprline();
        if (!reg_disblspr)
            geo_lspc_sprcalc();
        if (!reg_disblfix) {
            memset(fixrow, 0, sizeof(fixrow));
            geo_lspc_fixline();
            geo_lspc_fixcompose();
        }
    }
    else if (lspc.scanline == LSPC_LINE_BUFSTART ||
        lspc.scanline == LSPC_LINE_BUFSTART + 1) {
        if (!reg_disblspr)
            geo_lspc_sprcalc();
    }
}

void geo_lspc_run(unsigned cycs) {
    /* Timing of events which occur during a scanline is not perfect.
       Documentation on the fine details is rather old, and more modern
       materials do not go into full depth about the exact timing of each
       event -- for instance, when the scanline counter increments. There may
       also be missing details about how interrupts are timed, including
       unknown caveats. More research will be required to make everything
       perfect, but the framework for true cycle accuracy in the future is in
       place.
    */
    while (cycs) {
        /* Normally, this loop runs only once. In the case that the cycle per
           scanline barrier is crossed, it runs enough cycles to finish the
           scanline in the first iteration, and then runs the rest in a second
           iteration.
        */
        unsigned start = lspc.cyc;
        unsigned cycsleft = M68K_CYC_PER_LINE - start; // Cycles left for line
        unsigned runcycs = cycs < cycsleft ? cycs : cycsleft;
        unsigned end = start + runcycs;

        if ((start <= 29) && (end > 29)) {
            if (lspc.scanline == LSPC_LINE_BORDER_TOP)
                geo_lspc_aa();
            else if (lspc.scanline == LSPC_LINE_BORDER_BOTTOM + 1) {
                if (ngsys.cdmode && !geo_cd_vbl_enabled()) // Latch VBL
                    geo_cd_set_vbl_pending();
                else
                    geo_m68k_interrupt(irq_vbl_level);
            }
        }

        if ((start <= 573) && (end > 573)) {
            // Arbitrary placement until cycle accuracy is achieved
            geo_lspc_scanline();

            if (lspc.scanline == LSPC_LINE_BORDER_BOTTOM) {
                if (ngsys.irq2_ctrl & IRQ_TIMER_RELOAD_VBLANK)
                    ngsys.irq2_counter = ngsys.irq2_reload;
            }
        }

        if ((start <= 712) && (end > 712)) { // This is an educated guess
            lspc.scanline = (lspc.scanline + 1) % LSPC_SCANLINES;
        }

        lspc.cyc = end;
        cycs -= runcycs;

        lspc.cyc %= M68K_CYC_PER_LINE;
    }
}

void geo_lspc_state_load(uint8_t *st) {
    geo_serial_popblk((uint8_t*)lspc.vram, st, SIZE_64K + SIZE_4K);
    geo_serial_popblk((uint8_t*)lspc.palram, st, SIZE_16K);
    lspc.palbank = geo_serial_pop8(st);
    lspc.vramaddr = geo_serial_pop16(st);
    lspc.vrambank = geo_serial_pop16(st);
    lspc.vrammod = geo_serial_pop16(st);
    lspc.aa_counter = geo_serial_pop8(st);
    lspc.aa_disable = geo_serial_pop8(st);
    lspc.aa_reload = geo_serial_pop8(st);
    lspc.aa_timer = geo_serial_pop8(st);
    lspc.shadow = geo_serial_pop8(st);
    lspc.scanline = geo_serial_pop32(st);
    lspc.cyc = geo_serial_pop32(st);

    // Restore the output palette
    for (unsigned i = 0; i < (SIZE_16K >> 1); ++i)
        geo_lspc_palconv(i, lspc.palram[i]);
}

void geo_lspc_state_save(uint8_t *st) {
    geo_serial_pushblk(st, (uint8_t*)lspc.vram, SIZE_64K + SIZE_4K);
    geo_serial_pushblk(st, (uint8_t*)lspc.palram, SIZE_16K);
    geo_serial_push8(st, lspc.palbank);
    geo_serial_push16(st, lspc.vramaddr);
    geo_serial_push16(st, lspc.vrambank);
    geo_serial_push16(st, lspc.vrammod);
    geo_serial_push8(st, lspc.aa_counter);
    geo_serial_push8(st, lspc.aa_disable);
    geo_serial_push8(st, lspc.aa_reload);
    geo_serial_push8(st, lspc.aa_timer);
    geo_serial_push8(st, lspc.shadow);
    geo_serial_push32(st, lspc.scanline);
    geo_serial_push32(st, lspc.cyc);
}

// Return a pointer to Video RAM
const void* geo_lspc_vram_ptr(void) {
    return lspc.vram;
}

// Return a pointer to Palette RAM
const void* geo_lspc_palram_ptr(void) {
    return lspc.palram;
}
