// SNKHD - impostazioni dei giochi IGS PGM: Martial Masters / Xing Yi Quan (v104 USA, set martmast)
// Ricavate con il banco di prova (snkhd_pgm: dump RAM, WATCH con stack, DUMPROM + disassemblato 68000).
#include <string.h>
#include "pgmcfg.h"
#include "emu.h"
#include "../pgm/pgmcore.h"

// Regione (DIP B del driver = byte che l'ASIC27A riporta al 68000): 2 = Japan, 6 = USA (Andamiro).
// Con la patch "allchars" dell'utente il gioco usa comunque USA (testi e dialoghi in inglese): voce REGION nascosta.
int pgmcfg_region(const char *id, int region_jp) { (void)id; (void)region_jp; return 6; }
int pgmcfg_has_region(const char *id) { (void)id; return 0; }

typedef struct { unsigned addr; unsigned short expect, val; } pg_patch_t;
static const pg_patch_t martmast_patch[] = {
    // Ombre senza lampeggio: l'oggetto ombra di ogni lottatore (0x80B67E + n*0x2A, routine 0x12681E) ha un contatore
    // nei bit 3-6 del primo byte: 1 -> 0 = disegna, 0 -> 1 = nasconde (bclr #1) => ogni ombra a frame alterni (P1/P2
    // sfasati). 0x126838 bls -> nop (sempre il ramo "disegna"), 0x12684A subq #8 -> nop (contatore fermo).
    { 0x126838, 0x635e, 0x4e71 },
    { 0x12684a, 0x5100, 0x4e71 },
    // Patch dell'utente (martmast-v104_32m.u9-usa-allchars.xdelta, stessi byte nella ROM decifrata):
    //  - tutti i personaggi selezionabili: la routine 0x13F954 segnava le voci nascoste (Saojin, True Lotus Master)
    //    con ori.b #$28 -> andi.b #0 (nessuna voce bloccata). Saojin = casella sotto Reika (colonna sinistra in basso),
    //    True Lotus Master = casella in basso a destra.
    { 0x13f96c, 0x0028, 0x0228 },
    { 0x13f96e, 0x0028, 0x0000 },
    //  - regione/lingua: la funzione 0x10FC0E restituisce sempre 6 (USA, inglese) invece del valore dell'ASIC
    { 0x10fc24, 0x0138, 0x0130 },
    { 0x10fc64, 0x3039, 0x303c },
    { 0x10fc66, 0x00d0, 0x0006 },
    { 0x10fc68, 0x0138, 0x4e71 },
};

// Impostazioni del gioco (OPERATOR SETTING, salvate nella RAM a batteria; il gioco ne fa una copia di lavoro):
//   DIFFICULTY 0x803895 (1 EASY, 2 NORMAL, 3 HARD, 4 HARDEST), copia 0x81C18A (0-3, letta dall'IA)
//   BLOOD COLOR 0x803894 (0 RED, 1 BLUE = predefinito), copia 0x81C189
// Free play = DIP A bit 3 della BIOS PGM (impostato da pgm_load_zip). LEVEL DISPLAY e' gia' OFF di fabbrica.
static int mm_diff = 1;
void pgmcfg_apply(const char *id, int difficulty) {
    if (strcmp(id, "martmast")) return;
    if (difficulty < 1) difficulty = 1;
    if (difficulty > 8) difficulty = 8;
    mm_diff = (difficulty - 1) / 2;        // livello 1-2 EASY, 3-4 NORMAL, 5-6 HARD, 7-8 HARDEST
    for (size_t i = 0; i < sizeof martmast_patch / sizeof martmast_patch[0]; ++i)
        pgm_patch16(martmast_patch[i].addr, martmast_patch[i].expect, martmast_patch[i].val);
    pgm_set_shadow_sprites(0x760837, 0x761509, 11);   // ellissi delle ombre (12 larghezze, alte 11 righe)
}
void pgmcfg_frame(const char *id) {
    if (strcmp(id, "martmast")) return;
    emu_write8(0x803895, (uint8_t)(mm_diff + 1));
    emu_write8(0x81c18a, (uint8_t)mm_diff);
    emu_write8(0x803894, 0);
    emu_write8(0x81c189, 0);
}

// Stato dei giocatori: riquadro per giocatore a 0x81B528 (P1) / 0x81B546 (P2), bit 5 del primo byte = in partita
// (selezione, incontri); si azzera al game over / quando il perdente esce in VERSUS.
int pgmcfg_player_in(const char *id, int p) {
    (void)id;
    return (emu_read8(0x81b528u + (p ? 0x1eu : 0u)) & 0x20) != 0;
}
