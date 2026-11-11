// SNKHD - impostazioni dei giochi Sega ST-V: Golden Axe: The Duel (set MAME gaxeduel, core Mednafen)
// Ricavate col banco di prova (snkhd_appst: menu di test della BIOS / del gioco, dump EEPROM e RAM).
#include <string.h>
#include "stvcfg.h"
#include "emu.h"
#include "stvcore.h"

// Regione = BIOS: JAP = epr-20091 (Giappone, testi in giapponese), INT = epr-17952a (USA, testi in inglese)
int stvcfg_region(const char *id, int region_jp) { (void)id; return region_jp ? STV_REGION_JP : STV_REGION_NA; }

// EEPROM 93C46 (64 parole big-endian, la BIOS la legge all'avvio). Byte:
//   0x00 "SEGA", 0x08-0x09 CRC-16 CCITT (iniziale 0x5A81) dei byte 0x0C-0x41, XOR con la parola 0x42
//   0x0E-0x0F contatore delle scritture, 0x14 COIN/CREDIT SETTING (0 = #1 ... 0x18 = #25, 0x19 = FREE PLAY), 0x15 = 1
//   0x1E-0x25 impostazioni del gioco (GAME ASSIGNMENTS di Golden Axe The Duel, default 01 81 01 00 01 03 01 00):
//     0x22 = GAME DIFFICULTY 0-3 (nel menu 1-4, predefinito 2), 0x23 bit 7 = BLOOD (predefinito OFF)
//   0x44-0x7F = copia dei byte 0x08-0x43
// La BIOS scrive la EEPROM solo uscendo dal menu di test: qui la si riscrive prima del primo fotogramma.
static unsigned crc16(const unsigned char *d, int n) {
    unsigned c = 0x5A81;
    for (int i = 0; i < n; ++i) {
        c ^= (unsigned)d[i] << 8;
        for (int k = 0; k < 8; ++k) c = (c & 0x8000) ? ((c << 1) ^ 0x1021) & 0xffff : (c << 1) & 0xffff;
    }
    return c;
}

// G. AXE (boss, personaggio 10) selezionabile (solo VERSUS: in ARCADE, dopo la selezione di G. AXE la mappa del percorso
// manda l'SH-2 master in eccezione, PC 0x0600205A, chiamata da 0x06056E66; uguale con la ROM modificata): patch dell'utente (romhack "gaxeduel_patch") del programma
// epr17766.13, applicata nella ROM della cartuccia prima che la BIOS copi il programma in work RAM
// (offset file X -> work RAM 0x06010000 + X). Nella selezione: SINISTRA dal primo personaggio (Kain Blade) = G. AXE.
//  - 0x18EAC: il controllo "personaggio 10" su P1/P2 diventa un controllo sul numero dell'incontro (0x002FA553 == 10)
//  - 0x1BB3E / 0x1BBEC / 0x1C06F / 0x1E618: dove il gioco indicizza tabelle per personaggio (0-9), il 10 diventa 5
//  - 0x25861 / 0x2586B / 0x258EE: dati compressi della selezione (indice massimo 9 -> 10)
// La ROM e' a 8 bit: byte j del file = byte basso della parola di cartuccia 0x02000000 + 2j (copia a +0x100000).
static const struct { unsigned off, n; const char *old, *neu; } gax_unlock[] = {
    { 0x18eac, 4, "\x89\x2c\xd0\x1f", "\xd0\x1e\x70\xf9" },
    { 0x1bb3e, 10, "\xd3\x14\x63\x30\x63\x3c\x23\x38\x8b\x02", "\x60\x13\x88\x0a\x8b\x01\xe0\x05\x22\x00" },
    { 0x1bbec, 10, "\xd0\x13\x60\x00\x60\x0c\x88\x01\x8b\x02", "\x60\x13\x88\x0a\x8b\x01\xe0\x05\x22\x00" },
    { 0x1c06f, 17, "\x02\xe3\x01\xa0\x01\x00\x09\xe3\x00\xd2\x09\x33\x2c\x63\x30\x28\x30", "\x00\xe0\x01\xd2\x0b\x32\x0c\x60\x20\x88\x0a\x8b\x01\xe0\x05\x22\x00" },
    { 0x1c084, 2, "\x00\x09", "\x28\x00" },
    { 0x1e618, 1, "\x62", "\x64" },
    { 0x1e61c, 2, "\x03\x25", "\xe2\x0a" },
    { 0x1e622, 10, "\x54\xf6\x90\x17\x04\x4d\x74\x04\x64\x4f", "\x34\x22\x8b\x00\xe4\x05\x03\x45\x74\x04" },
    { 0x25861, 1, "\x09", "\x0a" },
    { 0x2586b, 1, "\x09", "\x0a" },
    { 0x258ee, 3, "\x31\xad\x3c", "\xa0\xad\x0b" },
};
// 1 = applicata (o gia' presente, es. romset modificato), 0 = ROM diversa (nessuna modifica)
int stvcfg_unlock(const char *id) {
    if (strcmp(id, "gaxeduel")) return 0;
    const unsigned nb = sizeof gax_unlock / sizeof gax_unlock[0];
    for (int pass = 0; pass < 2; ++pass)          // 0 = verifica di tutti i byte, 1 = scrittura
        for (unsigned i = 0; i < nb; ++i)
            for (unsigned j = 0; j < gax_unlock[i].n; ++j) {
                unsigned off = gax_unlock[i].off + j;
                unsigned char o = (unsigned char)gax_unlock[i].old[j], n = (unsigned char)gax_unlock[i].neu[j];
                for (unsigned m = 0; m < 2; ++m) {
                    uint32_t a = 0x02000000u + m * 0x100000u + off * 2;
                    uint16_t w = stv_cart_read16(a);
                    if (!pass) { if ((w & 0xff) != o && (w & 0xff) != n) return 0; continue; }
                    if ((w & 0xff) != n) stv_cart_patch16(a, w, (uint16_t)((w & 0xff00) | n));
                }
            }
    return 1;
}

void stvcfg_apply(const char *id, int difficulty) {
    if (strcmp(id, "gaxeduel")) return;
    unsigned char e[128];
    for (int w = 0; w < 64; ++w) { unsigned v = stv_eeprom_read(w); e[w * 2] = (unsigned char)(v >> 8); e[w * 2 + 1] = (unsigned char)v; }
    if (memcmp(e, "SEGA", 4)) return;              // EEPROM non inizializzata da Mednafen: lasciata com'e'
    if (difficulty < 1) difficulty = 1;
    if (difficulty > 8) difficulty = 8;
    e[0x14] = 0x19; e[0x15] = 0x01;                 // FREE PLAY
    e[0x22] = (unsigned char)((difficulty - 1) / 2);   // livello app 1-2 / 3-4 / 5-6 / 7-8 -> difficolta' 1 / 2 / 3 / 4
    e[0x23] |= 0x80;                                // BLOOD ON
    unsigned c = crc16(e + 0x0C, 0x42 - 0x0C) ^ ((unsigned)e[0x42] << 8 | e[0x43]);
    e[0x08] = (unsigned char)(c >> 8); e[0x09] = (unsigned char)c;
    memcpy(e + 0x44, e + 0x08, 0x3C);
    for (int w = 0; w < 64; ++w) stv_eeprom_write(w, (uint16_t)(e[w * 2] << 8 | e[w * 2 + 1]));
}

void stvcfg_frame(const char *id) { (void)id; }
// Stato dei giocatori: 0x002E8040 (work RAM bassa) = modalita' del gioco (0x0F = partita in corso: selezione,
// incontri, continue; 0x05-0x0D = attract/demo/classifica, 0x10 = fine partita). La BIOS ST-V registra l'ingresso di
// ogni giocatore (START con credito / FREE PLAY) in una parola della sua area di lavoro: 0x06000830 (P1) e
// 0x06000834 (P2), 0 dall'accensione finche' il giocatore non entra. Bastano per l'avvio veloce (ARCADE / VERSUS).
// I byte 0x002FA55A/B (0 umano, livello CPU) NON servono: cambiano anche fra un incontro VERSUS e l'altro.
// Rientro in VERSUS: l'app preme START per entrambi a impulsi (verificato: START non sceglie il personaggio e non
// disturba gli incontri; chi ha perso rientra dalla schermata del continue).
static unsigned rd32(unsigned a) { return (unsigned)emu_read8(a) << 24 | (unsigned)emu_read8(a + 1) << 16 | (unsigned)emu_read8(a + 2) << 8 | emu_read8(a + 3); }
int stvcfg_player_in(const char *id, int p) {
    if (strcmp(id, "gaxeduel")) return 0;
    return emu_read8(0x002e8040) == 0x0f && rd32(0x06000830u + (p ? 4u : 0u)) != 0;
}
int stvcfg_versus_pulse(const char *id) { return !strcmp(id, "gaxeduel"); }
