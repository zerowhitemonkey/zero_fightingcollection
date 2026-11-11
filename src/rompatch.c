// SNKHD - patch applicate alla P ROM (big-endian in memoria) dopo emu_load.
// Ogni gruppo si applica solo se TUTTI i byte originali corrispondono.
#include <string.h>
#include "rompatch.h"
#include "emu.h"

typedef struct { unsigned addr; const char *orig, *repl; } rp_t;
typedef struct { const char *id, *desc; const rp_t *p; int n; } rpgroup_t;

// Kizuna Encounter: selezione semplificata dei boss come la release Steam.
// Il gioco ha gia' il codice (SU su Gozu -> Jyazu, SU su King Lion -> King Leo, giocabili
// da soli) ma lo abilita solo con il bit 7 di $10EE7B: i 5 test diventano sempre veri.
static const rp_t kiz_boss[] = {
    { 0xc234, "6a0a", "4e71" },         // ritratto/nome del personaggio sotto il cursore
    { 0xc56c, "6a0a", "4e71" },         // idem (secondo giocatore / conferma)
    { 0xc7b6, "6a24", "4e71" },         // SU nella riga in alto: scambio Gozu<->Jyazu, King Lion<->King Leo
    { 0xb0de, "6a36", "4e71" },         // personaggio scelto (P1)
    { 0xb178, "6a000034", "4e714e71" }, // personaggio scelto (P2)
};

// The Last Blade 2: EX selezionabile come POWER/SPEED (GIU' = POWER -> SPEED -> EX -> POWER,
// SU al contrario). Il gioco ha gia' il modo EX ma solo col codice segreto: routine asm/lb2_exsel.s
// a $62000 (area libera $FF) al posto del controllo del codice ($1B552) e dello stato EX ($1A932).
static const rp_t lb2_ex[] = {
    { 0x1b552, "102c01cc0200", "4ef900062000" },   // controllo del codice -> chk
    { 0x1a8b4, "28bc0001a932", "28bc0006203c" },   // stato EX -> exst
    { 0x1ada4, "28bc0001adaa", "28bc000620b6" },   // insegna EX -> exsign
    { 0x62000, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
      "102c01cc0200007f66144a2c01cc6b0808050000660c600608050001660470004e75197c000301cf197cffff01d0303c00604eb90002725e70014e75266c00184a2b0100660c28bc0001a9724ef90001a95c4eb90001aa4a08050001660c0805000066144ef90001a932197c000001cc197c000101ce600c197c008001cc197c000201ce422c01cd28bc0001a88a41f90001ab3a303c68104eb8164e41f90001ac3e303c68204eb8164e4eb90001aa4a4ef90001a908266c00180c2b000301ce66064ef90001adaa4ef8176a" },
};

// The Last Blade: i due boss sbloccati senza codice (C x6, B x3, C x4). All'inizio della selezione
// ($6983A) il gioco imposta lo sblocco ($10EAC0 = $FF, frecce ai lati) solo se il bit 7 di $10EC00
// e' acceso: ora sempre. Sinistra su Kaede / destra su Moriya = casella boss (12 Musashi,
// 13 Kagami, SU/GIU' per cambiare); su MVS ($10FD82 != 0) $6A056 trasformava 13 in 12 (Kagami
// solo su AES): ora entrambi.
static const rp_t lb1_boss[] = {
    { 0x69840, "6706", "4e71" },
    { 0x6a056, "4a2d7d82", "4e757d82" },
};

// The Last Blade: barre POWER/SPEED piene (MAX) lampeggiavano bianco/colore ogni 2 frame (15 Hz):
// ora il cambio segue il bit 4 del contatore frame ($292) invece del bit 1 -> ~2 Hz come LB2 con FLASH OFF.
static const rp_t lb1_gauge[] = {
    { 0x60504, "08000001", "08000004" },   // POWER MAX
    { 0x605d8, "08000001", "08000004" },   // SPEED MAX
};

// Garou: Grant e Kain selezionabili. La selezione ha gia' 14 caselle (tabella $3E0C0: ... 12 = Kain $0D,
// 13 = Grant $0C) ma il massimo del cursore e' 13 solo su AES ($10FD82 = 0); su MVS 11. Ora sempre 13:
// SINISTRA su Rock = Grant, ancora SINISTRA = Kain (o DESTRA dall'ultimo personaggio).
static const rp_t garou_boss[] = {
    { 0x3e71a, "66000008", "4e714e71" },   // P1
    { 0x3e756, "66000008", "4e714e71" },   // P2
};

// Ninja Master's (GOG): Ranmaru e Nobunaga selezionabili. La griglia ha gia' le caselle 10 (Ranmaru) e
// 11 (Nobunaga), attive solo col bit 0 di $10C007 (codice segreto, $D994); i bit 1 (caselle 12/13) non
// corrispondono a personaggi validi. Le tre letture di $4007(a6) & 3 diventano 1: SINISTRA su Sasuke =
// Ranmaru, ancora SINISTRA = Nobunaga.
static const rp_t nm_boss[] = {
    { 0xd72c, "102e400702400003", "70014e714e714e71" },   // movimento del cursore
    { 0xd7e6, "102e400702400003", "70014e714e714e71" },   // scelta automatica
    { 0xd93e, "142e400702420003", "74014e714e714e71" },   // ritratti della griglia
};

// Savage Reign: King Leo (id 10, personaggio completo) selezionabile tenendo SU mentre si conferma un
// personaggio (routine asm/savagere_boss.s a $7FF40; il limite a 9 di $7180 diventa 10).
static const rp_t sr_boss[] = {
    { 0x7166, "302c00c21080", "4ef90007ff40" },
    { 0x7180, "0c100009", "0c10000a" },
    { 0x7ff40, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
      "302c00c214390010fd96082c00000037660614390010fd9c080200006702700a10804ef8716c" },
};

// Art of Fighting 2: Geese Howard selezionabile. All'apertura della selezione ($CC50) il gioco attiva Geese
// ($38 = 1 -> id $0D) se SU e' tenuto premuto, ma solo col bit 2 di $108800 acceso: ora sempre.
// Tenere SU mentre compare la schermata SELECT PLAYER (cioe' entrando in gioco con START/SELECT).
static const rp_t aof2_boss[] = {
    { 0xcc58, "670a", "4e71" },
};

// Art of Fighting: Mr. Big e Mr. Karate selezionabili in VERSUS. La selezione VS ha gia' le caselle
// 8 (Mr. Big, a sinistra di Lee) e 9 (Mr. Karate, a destra di John), attive solo dallo stage 9 della
// storia ($A85C: modo = $428 - 9). Ora sempre modo 2. Nella modalita' storia restano Ryo e Robert.
static const rp_t aof_boss[] = {
    { 0xa860, "302d0428044000096b0a", "70024e714e714e714e71" },
};

// Art of Fighting 3: Sinclair e Wyler selezionabili. I limiti del cursore vengono dalla tabella $536F0
// indicizzata dai bit di $10EC06 (codice segreto); col bit 7 di $10EC00 diventano 0-9 (tutti): ora sempre.
// SINISTRA su Robert = Sinclair, ancora SINISTRA = Wyler (o DESTRA da Jin).
static const rp_t aof3_boss[] = {
    { 0x3933a, "6704", "4e71" },    // inizio selezione
    { 0x3a44a, "6704", "4e71" },    // sfidante
};

// Waku Waku 7: Fernandez e Bonus-kun selezionabili in VERSUS. La selezione ha gia' 9 caselle (0 = Fernandez a
// sinistra di Rai, 8 = Bonus-kun a destra di Arina) attive solo se $106BE8 != 0, impostato da $55A2A quando
// entrambi i giocatori sono attivi E $10FD82 = 0 (AES): tolto il controllo AES, vale anche su MVS.
// In storia (1 giocatore) il gioco non supporta i due personaggi (userebbe Rai): restano nascosti.
static const rp_t wk7_boss[] = {
    { 0x55a30, "4a390010fd8266000038", "4e714e714e714e714e71" },
};

// Real Bout 2: Alfred (id $17). Il gioco lo abilita solo se bit 7 di $10E001 (AES): test forzato.
// Si tiene premuti B+C e si preme A (o D) per confermare il personaggio -> Alfred.
static const rp_t rb2_boss[] = {
    { 0x6362e, "6700004a", "4e714e71" },
    { 0x630b2, "6700001e", "4e714e71" },
};

// Samurai Shodown III: niente oggetti lanciati in campo (bombe, cibo). All'inizio dell'incontro ($62134)
// il gioco crea l'oggetto "ITEM MKR" (routine $6ECF4) che genera gli oggetti "ITEM" ($6EDFE dopo un tempo
// accumulato per giocatore, $6EEE4 a ogni secondo del timer secondo la tabella $89CC6). La routine diventa
// un rts: il generatore resta allocato ma inerte (nessun altro codice lo usa).
static const rp_t ss3_noitem[] = {
    { 0x6ecf4, "4a2d049a", "4e75049a" },
};

// SS3, POW al massimo: tinta del personaggio. La routine $26114 calcola per il giocatore la palette base $4B(obj) e la
// tinta $4C(obj) (aggiunta alla palette dello sprite da $13EE2: +2 = versione chiara, a frame alterni o fissa con
// l'anti-flicker). Con il POW pieno ($3A = $40) mette $4C = 2: ora 0 (personaggio sempre dei suoi colori).
// Le tinte dei colpi (tabella $26154, campo $61) restano.
static const rp_t ss3_powtint[] = {
    { 0x26148, "7202", "7200" },
};
// SS3, POW al massimo: barra e cartello POW alternavano ogni 2 frame la versione gialla (striscia sfumata) e quella
// arancione piena: contatore $F0/$F2(obj) / 2, bit 0 = variante (P1 $1CDAC, P2 $1CF5E). Ora "and #$FFFE" + bra:
// sempre la variante gialla (come INTERFACE SOLID), le fiamme restano animate (fotogrammi pari).
static const rp_t ss3_powbar[] = {
    { 0x1cdac, "0800000067000008", "0240fffe60000008" },
    { 0x1cf5e, "0800000067000008", "0240fffe60000008" },
};

// KOF 2002: Kusanagi ($28) si ottiene gia' su MVS col codice del gioco: con il cursore su Kyo, SU GIU DESTRA SINISTRA
// GIU SU SINISTRA DESTRA ($EA24 confronta le ultime 8 direzioni con la tabella $EA7A, poi $EA6A: Kyo -> $28).
// Esteso a Omega Rugal ($27, casella gia' prevista nella tabella cursore $B8EE8 sulla posizione dell'id $26, in basso
// a destra): stessa sequenza su quella casella (asm/kof2002_boss.s a $FF800, area libera). La voce $27 della tabella
// cursore aveva i vicini a 0 (ogni direzione portava a Kyo): ora uguali a quelli dell'id $26.
static const rp_t k2k2_boss[] = {
    { 0xea6a, "0c2c000000326606197c00280032", "4ef9000ff8004e714e714e714e71" },
    { 0xb9020, "00000000", "08112521" },
    { 0xff800, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffff", "102c00326608197c002800324e750c0000266606197c002700324e75" },
};

// Rage of the Dragons: Abubo (id $0E) e Johann (id $0F, boss finale) selezionabili.
// Abubo: la casella centrale si apre solo col codice del gioco ($6B656 -> -$1425/-$1424(a5) = $FF): all'inizio
// della selezione ($6AA2E) il flag diventa sempre $FF (griglia allargata con la casella centrale, anche nel "?").
// Johann: non ha una casella; tenendo SU mentre si conferma Abubo si sceglie Johann (asm/rotd_boss.s a $7F100,
// zeri liberi dopo la somma di controllo, al posto di $6B8F0). Il controllo della somma $6E604 e' gia' disattivato
// nella .neo (patches/rotd_noflicker.json), altrimenti queste modifiche bloccherebbero il gioco.
static const rp_t rotd_boss[] = {
    { 0x6aa2e, "1b6debdbebdc", "50edebdc4e71" },     // move.b -$1425,-$1424 -> st.b -$1424(a5); nop
    { 0x6b8f0, "6100f0f061000020", "4eb90007f1004e71" }, // bsr $6A9E2; bsr $6B916 -> jsr $7F100; nop
    { 0x6c7ec, "0e000d0e06000c0e07010d0e", "0e0e0d0e060e0c0e070e0d0e" }, // tabella cursore, casella Abubo: SU (anche in diagonale) resta su Abubo
    { 0x7f100, "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
      "0c41000e661c14390010fd96082e0000005d670614390010fd9c080200006702720f4eb90006a9e24ef90006b916" },
};

static const rpgroup_t groups[] = {
    { "rotd", "boss select", rotd_boss, (int)(sizeof rotd_boss / sizeof rotd_boss[0]) },
    { "kof2002", "boss select", k2k2_boss, (int)(sizeof k2k2_boss / sizeof k2k2_boss[0]) },
    { "samsho3", "no items", ss3_noitem, (int)(sizeof ss3_noitem / sizeof ss3_noitem[0]) },
    { "samsho3", "pow max tint", ss3_powtint, (int)(sizeof ss3_powtint / sizeof ss3_powtint[0]) },
    { "samsho3", "pow max bar", ss3_powbar, (int)(sizeof ss3_powbar / sizeof ss3_powbar[0]) },
    { "rbff2", "boss select", rb2_boss, (int)(sizeof rb2_boss / sizeof rb2_boss[0]) },
    { "wakuwak7", "boss select", wk7_boss, (int)(sizeof wk7_boss / sizeof wk7_boss[0]) },
    { "aof3", "boss select", aof3_boss, (int)(sizeof aof3_boss / sizeof aof3_boss[0]) },
    { "aof", "boss select", aof_boss, (int)(sizeof aof_boss / sizeof aof_boss[0]) },
    { "aof2", "boss select", aof2_boss, (int)(sizeof aof2_boss / sizeof aof2_boss[0]) },
    { "savagere", "boss select", sr_boss, (int)(sizeof sr_boss / sizeof sr_boss[0]) },
    { "ninjamas", "boss select", nm_boss, (int)(sizeof nm_boss / sizeof nm_boss[0]) },
    { "garou", "boss select", garou_boss, (int)(sizeof garou_boss / sizeof garou_boss[0]) },
    { "lastblad", "gauge flash", lb1_gauge, (int)(sizeof lb1_gauge / sizeof lb1_gauge[0]) },
    { "kizuna", "boss select", kiz_boss, (int)(sizeof kiz_boss / sizeof kiz_boss[0]) },
    { "lastblad", "boss unlock", lb1_boss, (int)(sizeof lb1_boss / sizeof lb1_boss[0]) },
    { "lastbld2", "EX select", lb2_ex, (int)(sizeof lb2_ex / sizeof lb2_ex[0]) },
};

// SNKHD: gruppi opzionali (selettori INTERFACE / SPECIAL EFFECTS), applicati o annullati dal vivo
typedef struct { const char *id; unsigned flag; const char *desc; const rp_t *p; int n; } rpopt_t;
#include "rompatch_opt.h"

static int hexbytes(const char *h, unsigned char *out) {
    int n = 0;
    for (; h[0] && h[1]; h += 2) {
        unsigned v = 0;
        for (int k = 0; k < 2; ++k) {
            char c = h[k];
            v = v * 16 + (unsigned)(c <= '9' ? c - '0' : (c | 32) - 'a' + 10);
        }
        out[n++] = (unsigned char)v;
    }
    return n;
}

int rompatch_apply(const char *id) {
    size_t psz; uint8_t *p = (uint8_t*)emu_prom(&psz);
    int applied = 0, failed = 0;
    for (size_t g = 0; g < sizeof groups / sizeof groups[0]; ++g) {
        if (strcmp(groups[g].id, id)) continue;
        unsigned char o[512], r[512]; int ok = 1;
        for (int i = 0; i < groups[g].n && ok; ++i) {
            int n = hexbytes(groups[g].p[i].orig, o);
            if (groups[g].p[i].addr + n > psz || memcmp(p + groups[g].p[i].addr, o, n)) ok = 0;
        }
        if (!ok) { failed++; continue; }     // gruppo non applicabile: gli altri gruppi vanno avanti
        for (int i = 0; i < groups[g].n; ++i) {
            int n = hexbytes(groups[g].p[i].repl, r);
            memcpy(p + groups[g].p[i].addr, r, n);
        }
        applied++;
    }
    return applied ? applied : (failed ? -1 : 0);
}

// Applica i gruppi opzionali del gioco con flag in "flags" e annulla gli altri (byte originali verificati)
int rompatch_options(const char *id, unsigned flags) {
    size_t psz; uint8_t *p = (uint8_t*)emu_prom(&psz);
    int on = 0;
    for (size_t g = 0; g < sizeof optgroups / sizeof optgroups[0]; ++g) {
        if (strcmp(optgroups[g].id, id)) continue;
        int want = (optgroups[g].flag & flags) != 0;
        unsigned char o[512], r[512]; int is_orig = 1, is_repl = 1;
        for (int i = 0; i < optgroups[g].n; ++i) {
            int no = hexbytes(optgroups[g].p[i].orig, o), nr = hexbytes(optgroups[g].p[i].repl, r);
            if (optgroups[g].p[i].addr + (no > nr ? no : nr) > psz) { is_orig = is_repl = 0; break; }
            if (memcmp(p + optgroups[g].p[i].addr, o, no)) is_orig = 0;
            if (memcmp(p + optgroups[g].p[i].addr, r, nr)) is_repl = 0;
        }
        if (want && is_orig) {
            for (int i = 0; i < optgroups[g].n; ++i) { int n = hexbytes(optgroups[g].p[i].repl, r); memcpy(p + optgroups[g].p[i].addr, r, n); }
        } else if (!want && is_repl) {
            for (int i = 0; i < optgroups[g].n; ++i) { int n = hexbytes(optgroups[g].p[i].orig, o); memcpy(p + optgroups[g].p[i].addr, o, n); }
        }
        if (want && (is_orig || is_repl)) on++;
    }
    return on;
}

int rompatch_has_options(const char *id, unsigned flag) {
    for (size_t g = 0; g < sizeof optgroups / sizeof optgroups[0]; ++g)
        if (!strcmp(optgroups[g].id, id) && (optgroups[g].flag & flag)) return 1;
    return 0;
}
