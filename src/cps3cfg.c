// SNKHD - impostazioni dei giochi CPS3: regione, EEPROM (difficolta', free play), stato dei giocatori
// Ricavate con il banco di prova (snkhd_cps3): test menu del gioco, confronto della EEPROM, dump della RAM.
#include <string.h>
#include "cps3cfg.h"
#include "emu.h"
#include "../cps3/cps3.h"

// Red Earth / Warzard (961121)
// EEPROM (parole nell'ordine del gioco, due copie: 0x00-0x17 e 0x18-0x2f, validate al boot):
//   0x00 basso = COIN (0 = 1 coin 1 credit ... 0x12 = FREE PLAY); altri byte = chute, continue, monitor, demo, suono, lingua
//   0x10 alto = GAME DIFFICULTY 0-7 (livello 1-8), basso = DAMAGE LEVEL; 0x11 = TIME COUNT SPEED / MAX ROUND;
//   0x12 = EVENT / HANDICAP; 0x13 alto = VIOLENCE MODE (0 = NORMAL, 1 = OFF)
//   0x14-0x17 = firma; 0x15 basso = regione (00 Giappone, 02 Europa, 03 USA): se non corrisponde il gioco
//   reimposta i valori di fabbrica.
// Valori di fabbrica (letti dal gioco al primo avvio) per regione:
static const uint16_t re_jp[24] = { 0x0000, 0x0101, 0x0001, 0x0000, 0x0100, 0x0002, 0x0200, 0x0000, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0x0301, 0x0101, 0x0001, 0x0000, 0x9505, 0x2300, 0x5332, 0x4420 };
static const uint16_t re_eu[24] = { 0x0000, 0x0101, 0x0001, 0x0000, 0x0100, 0x0002, 0x0200, 0x0000, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0x0501, 0x0101, 0x0001, 0x0000, 0x9505, 0x2302, 0x5332, 0x4420 };

// JoJo's Bizarre Adventure / Mirai e no Isan (jojoba, set dell'utente = jojobar1 Europa 990927)
// EEPROM: due copie 0x00-0x17 / 0x18-0x2F; parola 0x17 basso = regione (01 Giappone, 03 Europa, 04 USA).
//   0x00 alto = COIN (0x12 = FREE PLAY), 0x0B alto = GAME DIFFICULTY 0-7 (livello 1-8), 0x0D = REGULATION
//   (Europa 0x0001 = ON, Giappone 0x0000). REGULATION ON = sangue bianco e scene censurate (es. intro di DIO);
//   i kanji giganti degli scontri ora-muda dipendono invece dalla regione (solo Giappone).
// "Misto XBLA" (sempre attivo, entrambe le regioni): REGULATION OFF (sangue rosso) + niente kanji (patch sotto).
static const uint16_t jj_jp[24] = { 0x0001, 0, 0, 0x0101, 0x0101, 0x0100, 0, 0, 0, 0, 0, 0x0101, 0x0101, 0x0000, 0x0100, 0,
                                    0, 0, 0, 0, 0, 0, 0x4a4a, 0x4d01 };
static const uint16_t jj_eu[24] = { 0x0901, 0, 0, 0x0101, 0x0101, 0x0100, 0, 0, 0, 0, 0, 0x0101, 0x0101, 0x0001, 0x0100, 0,
                                    0, 0, 0, 0, 0, 0, 0x4a4a, 0x4d03 };

int cps3cfg_region(const char *id, int region_jp) {
    if (!strcmp(id, "redearth")) return region_jp ? 0x51 : 0x53;   // Warzard (Giappone) / Red Earth (Europa)
    return region_jp ? 0x01 : 0x03;                                // JoJo: Giappone / Europa
}

void cps3cfg_apply(const char *id, int difficulty, int region_jp) {
    if (difficulty < 1) difficulty = 1;
    if (difficulty > 8) difficulty = 8;
    if (!strcmp(id, "redearth")) {
        uint16_t img[24];
        memcpy(img, region_jp ? re_jp : re_eu, sizeof img);
        img[0x00] = (img[0x00] & 0xff00) | 0x12;                          // FREE PLAY
        img[0x10] = (uint16_t)(((difficulty - 1) << 8) | (img[0x10] & 0xff)); // difficolta'
        img[0x13] = (uint16_t)(img[0x13] & 0x00ff);                       // VIOLENCE MODE: NORMAL (finali con sangue)
        for (int i = 0; i < 24; ++i) { cps3_eeprom_wr(i, img[i]); cps3_eeprom_wr(i + 0x18, img[i]); }
    }
    if (!strcmp(id, "jojoba")) {
        uint16_t img[24];
        memcpy(img, region_jp ? jj_jp : jj_eu, sizeof img);
        img[0x00] = (uint16_t)(0x1200 | (img[0x00] & 0xff));                 // FREE PLAY
        img[0x0b] = (uint16_t)(((difficulty - 1) << 8) | (img[0x0b] & 0xff)); // difficolta'
        img[0x0d] = 0;                                                       // REGULATION OFF: sangue rosso
        for (int i = 0; i < 24; ++i) { cps3_eeprom_wr(i, img[i]); cps3_eeprom_wr(i + 0x18, img[i]); }
        // kanji giganti (オラ / 無駄) negli scontri ora-muda: l'effetto 15 viene creato da 0x060FD08A solo con
        // regione 0/1 (Giappone); fuori dal Giappone la funzione esce senza crearlo. Togliendo il "bt/s" la
        // regione 1 segue il ramo non giapponese (la regione 0 non e' usata). Lo scontro resta in tutte le regioni.
        cps3_patch16(0x060fd0a0, 0x8d07, 0x0009);
    }
}

// Red Earth: livello d'esperienza del personaggio = byte 0x0206A8C2 (P1) / 0x0206AB42 (P2), 0-31 = LV 1-32.
// L'area (0x0206A8C0, 0x30 byte) viene azzerata quando il giocatore entra in partita e riempita dalla password;
// il byte dopo (0x...C3) e vita/mosse il gioco li ricalcola dal livello prima del primo incontro.
// Le password "livello 32" di tutti e quattro i personaggi danno 0x1F (verificato col banco di prova).
int cps3cfg_has_maxlevel(const char *id) { return !strcmp(id, "redearth"); }
int cps3cfg_layout(const char *id) { return !strcmp(id, "jojoba") ? 2 : 1; }

// HUD PRIORITY (come l'opzione della versione XBLA): l'HUD di JoJo sta sullo strato testo, sempre sopra gli sprite.
// Il renderer HD disegna le righe 0-5 (DYNAMIC) e anche 24-27 (DYNAMIC+) sotto gli sprite delle voci della lista
// con gscroll 0 (personaggi, Stand, effetti; ombre escluse), solo con l'HUD dell'incontro a schermo.
int cps3cfg_has_hudprio(const char *id) { return !strcmp(id, "jojoba"); }
void cps3cfg_set_hudprio(const char *id, int mode) { cps3_set_hud_dynamic(cps3cfg_has_hudprio(id) ? mode : 0); }

void cps3cfg_frame(const char *id, int maxlevel) {
    if (!maxlevel || strcmp(id, "redearth")) return;
    for (int p = 0; p < 2; ++p) {
        uint32_t a = 0x0206a8c2u + (p ? 0x280u : 0u);
        if (cps3cfg_player_in(id, p) && emu_read8(a) < 0x1f) cps3_write8(a, 0x1f);
    }
}

// stato del giocatore (RAM SH-2): Red Earth 0x0206a9c5 (P1) / 0x0206ac45 (P2): 0 assente, 2 in gioco, 3 continue
int cps3cfg_player_in(const char *id, int p) {
    if (!strcmp(id, "redearth")) return emu_read8(0x0206a9c5u + (p ? 0x280u : 0u)) == 2;
    // JoJo: 0x020314AA = maschera dei giocatori in partita (bit 0 = P1, bit 1 = P2): 01 / 02 / 03, 00 = nessuno
    if (!strcmp(id, "jojoba")) return (emu_read8(0x020314aau) >> (p & 1)) & 1;
    return 0;
}

// VERSUS CPU (JoJo, come la modalita' della versione XBLA): si entra come in VERSUS (P1 e P2 in partita), il P1
// sceglie il proprio personaggio e poi, con lo stesso controller, quello dell'avversario (colore = tasto con cui
// si conferma); durante l'incontro il P2 e' guidato dalla CPU.
//   0x02033142 = personaggi confermati nella selezione (bit 0 P1, bit 1 P2), 3 dal VS fino alla fine dell'incontro
//   P2 + 0x18C (0x02034E38, oggetto del personaggio P2 a 0x02034CAC) = 1 se il personaggio e' guidato dalla CPU:
//   il gioco lo mette a 1 all'avvio della partita per i giocatori non entrati (0x06180CFC) e lo azzera quando il
//   P2 entra (0x0600BA9E); va impostato prima del caricamento dell'incontro (qui: appena entrambi hanno confermato)
//   e azzerato durante la selezione (altrimenti il cursore del P2 non si muove).
static int vs_forced_stage = -1;
int cps3cfg_has_vscpu(const char *id) { return !strcmp(id, "jojoba"); }

// Personaggi con IA: solo quelli che l'ARCADE usa come avversari (tabella 0x062182F6: 0-8, 10, 11, 12, 14, 15).
// Gli altri selezionabili (0x10 Hol Horse, 0x11 Vanilla Ice, 0x13 Black Polnareff, 0x14 Pet Shop, 0x16 Mariah)
// guidati dalla CPU restano fermi/indietreggiano e Mariah blocca il gioco (l'IA sceglie una mossa che non ha:
// salto a 0x00030000 dalla tabella 0x020068E8). In VERSUS CPU non si possono confermare come avversario.
static int jj_has_ai(unsigned c) { return c <= 8 || c == 10 || c == 11 || c == 12 || c == 14 || c == 15; }
static int vs_hint;   // 1 = cursore del P2 su un personaggio senza IA (messaggio a schermo)

void cps3cfg_vscpu_input(const char *id, unsigned *j0, unsigned *j1) {
    static int wait_release;
    vs_hint = 0;
    if (strcmp(id, "jojoba")) return;
    *j1 = 0;                                         // il controller del P2 non serve
    if (!cps3cfg_player_in(id, 1)) { wait_release = 0; return; }
    unsigned m = emu_read8(0x02033142u) & 3;
    // selezione senza tempo limite: allo scadere il gioco confermerebbe anche un personaggio senza IA
    if (m != 3 && emu_read8(0x02033149u) < 5) cps3_write8(0x02033149u, 5);
    if ((m & 1) && !(m & 2)) {
        // il P1 sceglie l'avversario: comandi al P2, dopo aver rilasciato il tasto di conferma del P1
        if (wait_release) { if (!*j0) wait_release = 0; *j0 = 0; }
        else {
            *j1 = *j0; *j0 = 0;
            if (!jj_has_ai(emu_read8(0x02033146u))) { *j1 &= IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT; vs_hint = 1; }
        }
    } else wait_release = !(m & 1) ? 1 : wait_release;
    cps3_write8(0x02034e38u, m == 3 ? 1 : 0);
    if (vs_forced_stage >= 0) cps3_write8(0x02031459u, (uint8_t)vs_forced_stage);
}

const char *cps3cfg_vscpu_hint(const char *id) {
    return (vs_hint && !strcmp(id, "jojoba")) ? "NO CPU FOR THIS CHARACTER" : NULL;
}

// Stage del VERSUS: il gioco lo sceglie a caso a 0x06181BB6 (tabella per P1/P2 + numero casuale 0-7) e lo scrive
// in 0x02031459 (numero dello stage, 0-41; 32 = mappa, non e' uno stage). Per forzarlo: patch dell'istruzione
// "mov.b @(r0,r8),r3" (038C) in "mov.b @r2,r3" (6320, r2 = 0x02031459): il gioco rilegge il valore gia' presente,
// che l'app scrive a ogni frame. Nomi: stage di HftF (JoJoWiki) e varianti di orario/luce numerate.
typedef struct { unsigned char id; const char *name; } jj_stage_t;
static const jj_stage_t jj_stages[] = {
    { 0, "A LOCKUP" }, { 1, "HEALTH ROOM" }, { 2, "IN AN AIRPLANE" }, { 3, "TIGER BALM GARDEN" },
    { 4, "HOTEL (DEVIL) 1" }, { 34, "HOTEL (DEVIL) 2" }, { 5, "REMAINS 1" }, { 24, "REMAINS 2" }, { 33, "REMAINS 3" },
    { 6, "HOTEL (JUSTICE)" }, { 7, "AMUSEMENT PARK" },
    { 25, "SMALL ISLAND 1" }, { 8, "SMALL ISLAND 2" }, { 35, "SMALL ISLAND 3" },
    { 9, "DESERT 1" }, { 31, "DESERT 2" }, { 22, "DESERT 3" },
    { 19, "HELICOPTER 1" }, { 20, "HELICOPTER 2" }, { 21, "HELICOPTER 3" },
    { 10, "TEMPLE RUINS 1" }, { 36, "TEMPLE RUINS 2" },
    { 26, "BACK STREET 1" }, { 18, "BACK STREET 2" }, { 11, "BACK STREET 3" },
    { 16, "CAIRO STREET 1" }, { 28, "CAIRO STREET 2" }, { 29, "CAIRO STREET 3" },
    { 12, "CORRIDOR 1" }, { 37, "CORRIDOR 2" }, { 13, "HALL 1" }, { 27, "HALL 2" }, { 38, "HALL 3" },
    { 14, "MANSION ROOM 1" }, { 23, "MANSION ROOM 2" }, { 39, "MANSION ROOM 3" },
    { 15, "DIO'S CASTLE 1" }, { 30, "DIO'S CASTLE 2" }, { 40, "DIO'S CASTLE 3" },
    { 17, "BRIDGE 1" }, { 41, "BRIDGE 2" },
};
#define JJ_NSTAGES ((int)(sizeof jj_stages / sizeof jj_stages[0]))

int cps3cfg_vs_stage_count(const char *id) { return !strcmp(id, "jojoba") ? JJ_NSTAGES + 1 : 0; }
const char *cps3cfg_vs_stage_name(const char *id, int idx) {
    if (strcmp(id, "jojoba") || idx <= 0 || idx > JJ_NSTAGES) return "AUTO";
    return jj_stages[idx - 1].name;
}
void cps3cfg_vscpu_setup(const char *id, int enabled, int stage_idx) {
    vs_forced_stage = -1;
    if (strcmp(id, "jojoba")) return;
    int force = enabled && stage_idx > 0 && stage_idx <= JJ_NSTAGES;
    if (force) vs_forced_stage = jj_stages[stage_idx - 1].id;
    cps3_patch16(0x06181bb6, force ? 0x038c : 0x6320, force ? 0x6320 : 0x038c);
}
