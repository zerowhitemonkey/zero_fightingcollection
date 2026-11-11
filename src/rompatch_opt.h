// SNKHD - patch opzionali per gioco (incluso da rompatch.c). Ogni gruppo si applica solo se i byte
// originali corrispondono e si annulla solo se ci sono i byte nuovi.

// ---- Samurai Shodown II
static const rp_t ss2_hud_bars[] = {
    { 0x15474, "6704", "6004" },            // barra vita bassa: niente alternanza palette 3 (rossa) / 0 (bianca)
};
static const rp_t ss2_hud_anim[] = {   // animazioni del POW: ferme solo con INTERFACE = SOLID
    { 0x25df4, "102d89f9", "70004e71" },    // cartello POW al massimo: immagine fissa (era ciclo di 4 ogni 2 frame)
};
static const rp_t ss2_hud_text[] = {
    { 0x10ed0, "67000008", "60000008" },    // WAIT A CHALLENGER fisso (alternava con PUSH START ogni 16 frame)
};

// ---- Samurai Shodown
static const rp_t ss1_hud_bars[] = {
    { 0x33f2, "02410001", "02410000" },     // KO / END: colore fisso (alternava due immagini ogni 3 frame)
};
static const rp_t ss1_hud_anim[] = {   // animazioni del POW: ferme solo con INTERFACE = SOLID
    { 0x389e, "302d89f8", "70004e71" },     // cartello POW al massimo: immagine fissa (era ciclo di 4 ogni 2 frame)
};
static const rp_t ss1_hud_text[] = {
    { 0x2d20, "67000006", "60000006" },     // timer sotto 15: sempre palette 1 (alternava con la 5 ogni 2 frame)
    { 0x2d2ba, "67000006", "60000006" },    // PUSH START sempre visibile (acceso/spento ogni 16 frame)
};

// ---- Samurai Shodown III
static const rp_t ss3_hud_bars[] = {
    { 0x1c8ec, "67000014", "60000014" },    // VS / DIE: niente ciclo di palette (bianco/rosso), resta il rosso standard
};
static const rp_t ss3_hud_anim[] = {   // animazioni del POW: ferme solo con INTERFACE = SOLID
    { 0x1cf40, "526c00f20c6c001800f26500000642", "397c000c00f26000000a4e714e7142" }, // POW al massimo: contatore fisso (fiamme e cartello fermi)
};

// ---- Samurai Shodown IV
static const rp_t ss4_hud_bars[] = {
    { 0x85114, "6704", "6004" },            // pallino del giocatore: sempre palette C (alternava con la 3)
    { 0x28dc0, "6702", "6002" },            // barra vita bassa P1: sempre palette 7 (rossa), alternava con la 0B (bianca)
    { 0x28de8, "6702", "6002" },            // barra vita bassa P2
};
static const rp_t ss4_hud_anim[] = {   // animazioni del POW: ferme solo con INTERFACE = SOLID
    { 0x2917c, "522c00d4", "4e714e71" },    // cartello POW in alto: immagine fissa (ciclo di 3 ogni 4 frame)
    { 0x29cd0, "522c00d0", "4e714e71" },    // fiamme POW in basso P1: immagine fissa (ciclo di 8)
    { 0x29d34, "522c00d2", "4e714e71" },    // fiamme POW in basso P2: immagine fissa
};
static const rp_t ss4_hud_text[] = {
    { 0x124d0, "6724", "4e71" },            // PUSH START / INSERT COIN sempre visibile (acceso/spento ogni 32 frame)
};

// ---- Samurai Shodown V / V Special / V Perfect (stesse routine di SS4, trovate per firma)
static const rp_t ss5_hud_bars[] = {
    { 0x3c356, "6702", "6002" },  // barra vita bassa P1: colore fisso (niente alternanza col bianco)
    { 0x3c37e, "6702", "6002" },  // barra vita bassa P2
    { 0x9f138, "6704", "6004" },  // pallino del giocatore: colore fisso
};
static const rp_t ss5_hud_anim[] = {   // animazioni del POW: ferme solo con INTERFACE = SOLID
    { 0x3c94e, "522c00d4", "4e714e71" },  // cartello POW in alto: immagine fissa
    { 0x3d488, "522c00d0", "4e714e71" },  // fiamme/cartello rabbia P1: immagine fissa
    { 0x3d4ec, "522c00d2", "4e714e71" },  // fiamme/cartello rabbia P2: immagine fissa
};
static const rp_t ss5_hud_text[] = {
    { 0x15f82, "6724", "4e71" },  // PUSH START / INSERT COIN sempre visibile
};
static const rp_t ss5sp_hud_bars[] = {
    { 0x28afc, "6702", "6002" },  // barra vita bassa P1: colore fisso (niente alternanza col bianco)
    { 0x28b24, "6702", "6002" },  // barra vita bassa P2
    { 0x8c682, "6704", "6004" },  // pallino del giocatore: colore fisso
};
static const rp_t ss5sp_hud_anim[] = {   // animazioni del POW: ferme solo con INTERFACE = SOLID
    { 0x29150, "522c00d4", "4e714e71" },  // cartello POW in alto: immagine fissa
    { 0x29c8a, "522c00d0", "4e714e71" },  // fiamme/cartello rabbia P1: immagine fissa
    { 0x29cee, "522c00d2", "4e714e71" },  // fiamme/cartello rabbia P2: immagine fissa
};
static const rp_t ss5sp_hud_text[] = {
    { 0x160f4, "6724", "4e71" },  // PUSH START / INSERT COIN sempre visibile
};
static const rp_t ss5p_hud_bars[] = {
    { 0x31542, "6702", "6002" },  // barra vita bassa P1: colore fisso (niente alternanza col bianco)
    { 0x3156a, "6702", "6002" },  // barra vita bassa P2
    { 0x952a6, "6704", "6004" },  // pallino del giocatore: colore fisso
};
static const rp_t ss5p_hud_anim[] = {   // animazioni del POW: ferme solo con INTERFACE = SOLID
    { 0x31b96, "522c00d4", "4e714e71" },  // cartello POW in alto: immagine fissa
    { 0x326e4, "522c00d0", "4e714e71" },  // fiamme/cartello rabbia P1: immagine fissa
    { 0x32748, "522c00d2", "4e714e71" },  // fiamme/cartello rabbia P2: immagine fissa
};

static const rp_t kiz_hud_bars[] = {
    { 0x1118a, "303c066e", "303c0005" },  // barra vita bassa: colore fisso (niente alternanza col colore chiaro)
};
static const rp_t kiz_hud_text[] = {
    { 0x12d9e, "6616", "6016" },  // PUSH START / PLEASE WAIT sempre visibile
    { 0x12df2, "6616", "6016" },  // INSERT COIN sempre visibile
    { 0x12eae, "6616", "6016" },  // PUSH START (secondo giocatore) sempre visibile
};

static const rp_t lb1_hud_bars[] = {
    { 0x60508, "670e", "4e71" },          // gauge MAX (SPEED): colori standard, niente lampeggio bianco
    { 0x605dc, "670e", "4e71" },          // gauge MAX (POWER): colori standard, niente lampeggio bianco
    { 0x60678, "02400006", "02400000" },  // gauge: niente ciclo di colori / lampeggio
    { 0x60258, "6706", "6006" },          // barra vita bassa: palette F fissa (niente alternanza con la E)
};
static const rp_t lb1_fx[] = {
    { 0x33edc, "660000c2", "4e714e71" },  // pezzi con flag $2000 (scie, effetti) disegnati a ogni frame
};
static const rp_t lb2_hud_bars[] = {
    { 0x5b3c6, "02440002", "02440000" },  // gauge MAX / barre: palette standard, niente lampeggio
};
static const rp_t lb2_fx[] = {
    { 0xe4da, "660000b8", "4e714e71" },   // pezzi con flag $2000 (scie, effetti) disegnati a ogni frame
};

static const rp_t sav_fx[] = {
    { 0x7fe00, "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", "0cac464c59200010671a0cac45464354001067100cac464e5320001067064ef90001ed504ef90001ed74" },  // asm/savagere_fx.s
    { 0x7ff12, "4ef90001ed50", "4ef90007fe00" },  // routine ombre: oggetti non-ombra -> routine effetti
};

static const rp_t aof_hud_text[] = {
    { 0x64f0, "670e", "4e71" },          // PLAYER 2 / NOW WAITING: niente pausa vuota (blink ogni 32 frame)
};
static const rp_t aof2_hud_text[] = {
    { 0x89de, "670e", "4e71" },          // WAIT A CHALLENGER ecc.: niente pausa vuota
};
static const rp_t nin_hud_text[] = {
    { 0x7450, "67000008", "4e714e71" },  // PUSH START sempre visibile (bit 4 di $207)
};

static const rp_t ww7_hud_bars[] = {
    // animazioni palette dei messaggi (palette fix 4-7: SUPER!, CHARGE!, n HIT COMBO...): colore fisso (primo valore)
    { 0x3eeb4, "00020071000200730000000200700002007200000002007100020075000200740000000100760001007700010078000100790001007a0001007b0001007c0001007d0000",
               "0002007100020071000000020070000200700000000200710002007100020071000000010076000100760001007600010076000100760001007600010076000100760000" },
};
static const rp_t ww7_fx[] = {
    { 0x38290, "0000000000000000000000000000000000000000000000000000000000000000000000000000", "bdfc001031fc6d14bdfc001035a06c0c082d0000b0314ef90003c9b2088400024ef90003c9f0" },  // asm/wakuwak7_fx.s
    { 0x3827a, "082d0000b0314ef90003c9b2", "4ef9000382904e714e714e71" },  // hook ombre: oggetti non-ombra -> routine effetti
};
static const rp_t gf_fx[] = {   // Galaxy Fight: riflessi nell'acqua ($EE000, helper $92) + effetti ($5D70)
    { 0xee008, "30280228e248656a", "4e714e714e714e71" },
    { 0xee030, "6f0000b2", "6f000048" },
    { 0xee038, "6c0000aa", "6c000040" },
    { 0xee058, "6f00008a", "6f000020" },
    { 0xee074, "307c00006064", "4eb800924e71" },
    { 0x92, "95f755fd8cfc77f4d7f51ed5b6ff65f575fd", "307c00004eb9000035f041f9001010044e75" },
    { 0x5d7e, "6512", "6012" },
    { 0x5d90, "65ee", "4e71" },
};
static const rp_t garou_hud_bars[] = {
    { 0x1c7a0, "67000006", "60000006" },  // scritta S power (gauge >= $40): niente alternanza palette 4/5
    { 0x1c7be, "67000006", "60000006" },  // gauge pieno: niente alternanza palette 2/3
};
static const rp_t rbff2_hud_bars[] = {
    { 0x1edc2, "66000006", "60000006" },  // barra vita bassa P1: palette C fissa (niente alternanza con la D scura)
    { 0x1eec2, "66000006", "60000006" },  // barra vita bassa P2
    { 0x25d28, "6718", "6018" }, { 0x25d3c, "6704", "6004" }, { 0x25d5e, "6704", "6004" }, { 0x25da6, "6704", "6004" },  // gauge P1
    { 0x25f0a, "6718", "6018" }, { 0x25f1e, "6704", "6004" }, { 0x25f40, "6704", "6004" }, { 0x25f88, "6704", "6004" },  // gauge P2
};

static const rp_t rbff1_fx[] = {
    { 0x375c, "6700001c", "4e714e71" },   // oggetti "a frame alterni" ($1E & $41): disegnati a ogni frame
    { 0x55c6, "67000008", "4e714e71" },   // idem (seconda routine)
};
static const rp_t rbffsp_fx[] = {
    { 0x412e, "6700001a", "4e714e71" },   // oggetti "a frame alterni" ($1E & $41): disegnati a ogni frame
    { 0x5bc6, "67000008", "4e714e71" },   // idem (seconda routine)
};
static const rp_t rbff2_fx[] = {
    { 0x543a, "6700001a", "4e714e71" },   // oggetti "a frame alterni" ($1E & $41): disegnati a ogni frame
    { 0x984a, "67000008", "4e714e71" },   // idem (seconda routine)
};
static const rp_t ff1_hud_bars[] = {
    { 0x103fa, "02400018", "02400000" },   // segni dei round: fase d'animazione fissa (niente POINT/pallina alternati) P2
    { 0x10430, "02400018", "02400000" },   // idem P1
};
static const rp_t ff1_hud_text[] = {
    { 0x1016a, "67000006", "60000006" },   // 2P PUSH START fisso (bit 6 del contatore frame = vuoto)
};
static const rp_t ffsp_hud_bars[] = {
    { 0x1a306, "67000006", "60000006" },   // barra vita bassa: palette 6 fissa (niente alternanza con la 7)
    { 0x1a3b8, "02400018", "02400000" },   // segni dei round: fase d'animazione fissa
};
static const rp_t ffsp_hud_text[] = {
    { 0x1a604, "6700000a", "6000000a" },   // PUSH START fisso
};
static const rp_t ffsp_fx[] = {
    { 0x1b944, "67000008", "4e714e71" },   // oggetti "a frame alterni" (polvere, proiettili): disegnati a ogni frame
};
static const rp_t ff3_hud_bars[] = {
    { 0x2d60a, "67000006", "60000006" },   // barra vita bassa: palette 3 fissa (niente alternanza con la 4)
    { 0x2d62c, "67000006", "60000006" },
    { 0x2d74c, "67000006", "60000006" },   // idem P2
    { 0x2d76e, "67000006", "60000006" },
};
static const rp_t ff3_fx[] = {
    { 0x371c, "67000008", "4e714e71" },
    { 0x555e, "67000008", "4e714e71" },
};

static const rp_t rbff1_hud_bars[] = {
    { 0x23f82, "66000006", "60000006" },  // barra vita bassa P1: palette C fissa (niente alternanza con la D scura)
    { 0x24082, "66000006", "60000006" },  // barra vita bassa P2
    { 0x29d54, "6710", "6010" },          // gauge POWER MAX P1: palette standard ($8x), niente alternanza con $Ax
    { 0x29f4c, "6710", "6010" },          // gauge POWER MAX P2
};
static const rp_t rbffsp_hud_bars[] = {   // stesse routine di RBFF2
    { 0x25b24, "66000006", "60000006" }, { 0x25c24, "66000006", "60000006" },  // barra vita bassa P1/P2
    { 0x2b62e, "6718", "6018" }, { 0x2b642, "6704", "6004" }, { 0x2b664, "6704", "6004" }, { 0x2b6ac, "6704", "6004" },  // gauge P1
    { 0x2b810, "6718", "6018" }, { 0x2b824, "6704", "6004" }, { 0x2b846, "6704", "6004" }, { 0x2b88e, "6704", "6004" },  // gauge P2
};

// KOF '94: oggetto palette $313A alterna ogni 8 frame due serie (fix 6 vita, fix 8 MAXIMUM): sempre la prima.
static const rp_t kof94_hud_bars[] = { { 0x314a, "670003fc", "600003fc" } };
// KOF '94: timer <= 10 palette fix A/9 alternate ogni 4 frame ($34244): sempre A.
static const rp_t kof94_hud_text[] = { { 0x34244, "67000006", "60000006" } };
// KOF '95: oggetto $292A (creato a $3B2A0) alterna ogni 8 frame due liste di palette: fix 5 (barra vita bassa gialla/rossa)
// e fix 7 (POW gialla/variante): seconda lista ($5B18E) = prima, cioe' sempre i colori standard.
static const rp_t kof95_hud_bars[] = { { 0x5b18e, "0005000500070007", "0004000500060007" } };
// KOF '95: timer <= 10 palette fix 9/A alternate ogni 4 frame ($39E40): sempre 9; PUSH START 16 frame si'/16 no ($9B0): sempre.
static const rp_t kof95_hud_text[] = { { 0x39e58, "67000006", "60000006" }, { 0x9c6, "67000024", "4e714e71" } };
// KOF '96: come KOF '95, oggetto creato a $1FAFA con le liste $744F8/$744FE (fix 5 = barra vita bassa gialla/rossa ogni
// 8 frame): seconda lista = prima. Timer <= 10 palette 8/9 ogni 4 frame ($2011A): sempre 8; PUSH START 16/16 ($1986): sempre.
static const rp_t kof96_hud_bars[] = { { 0x744fe, "00050005", "00040005" } };
static const rp_t kof96_hud_text[] = { { 0x2011a, "6704", "6004" }, { 0x1986, "6722", "4e71" } };
// KOF 2001: PRESS START / INSERT COIN 16 frame si' / 16 no (bit 4 del contatore -$7f75 = stringa vuota): sempre visibile.
static const rp_t kof2k1_hud_text[] = { { 0x1350, "672e", "4e71" } };
// KOF '99: icone POW/MAX accanto ai ritratti (fix 9/A) con le palette alternate ogni 8 frame (bit 3 del contatore,
// $2CBCA): sempre quelle standard. PRESS START 16 frame si'/16 no ($C86): sempre visibile.
static const rp_t kof99_hud_bars[] = { { 0x2cbd0, "660c", "600c" } };
static const rp_t kof99_hud_text[] = { { 0xc86, "672e", "4e71" } };
// KOF '98: timer <= 10 palette fix E/0 alternate ogni 4 frame ($3076A): sempre E; PRESS START 16/16 ($1DF2): sempre.
static const rp_t kof98_hud_text[] = { { 0x30770, "6704", "6004" }, { 0x1df2, "672e", "4e71" } };
// KOF '97: timer <= 10 palette E/0 alternate ogni 4 frame ($2643E): sempre E; PRESS START 16/16 ($1A78): sempre.
static const rp_t kof97_hud_text[] = { { 0x26444, "6704", "6004" }, { 0x1a78, "672e", "4e71" } };
// KOF 2003: CHANGE OK (fix 4/5 tabella $6EF34/$6EF74) bianco/rosso ogni 8 frame (bit 3 di $26(obj), $1F598/$1F5C8): sempre bianco.
static const rp_t kof2k3_hud_bars[] = { { 0x1f59e, "6702", "6002" }, { 0x1f5ce, "6702", "6002" } };
static const rp_t kof2k_hud_bars[] = {
    { 0x32b78, "6702", "6002" },          // indicatore delle scorte del POW: palette fix 9 fissa (alternava ROM $09/$1B ogni 4 frame)
    { 0x33292, "6702", "6002" },          // scritta MAX nel POW: sempre le tile standard (alternava con la variante gialla ogni 8 frame)
};
static const rp_t kof2k_hud_text[] = {
    { 0x307ce, "6702", "6002" },          // timer sotto 10 s: palette fix 0 normale (alternava verde/giallo ogni 4 frame)
    { 0xc7c, "672e", "4e71" },            // PRESS START / INSERT COIN sempre visibile (bit 4 del contatore = vuoto)
};

static const rp_t kof2k2_hud_text[] = {
    { 0x1340, "672e", "4e71" },           // PRESS START / INSERT COIN sempre visibile (bit 4 del contatore -$7f75 = stringa vuota $B157A)
};

// Rage of the Dragons: PRESS START del secondo giocatore 24 frame si' / 8 no (contatore $14(a6) & $1F: a $18 la stringa
// vuota $1CFB2, $1CA6E): sempre visibile. (Il controllo della somma del codice e' disattivato nella .neo.)
static const rp_t rotd_hud_text[] = { { 0x1ca72, "6600000a", "6000000a" } };

static const rpopt_t optgroups[] = {
    { "rotd", RP_HUD_TEXT, "hud text solid", rotd_hud_text, 1 },
    { "kof2002", RP_HUD_TEXT, "hud text solid", kof2k2_hud_text, 1 },
    { "kof2001", RP_HUD_TEXT, "hud text solid", kof2k1_hud_text, 1 },
    { "kof99", RP_HUD_BARS, "hud bars solid", kof99_hud_bars, 1 },
    { "kof99", RP_HUD_TEXT, "hud text solid", kof99_hud_text, 1 },
    { "kof98", RP_HUD_TEXT, "hud text solid", kof98_hud_text, 2 },
    { "kof97", RP_HUD_TEXT, "hud text solid", kof97_hud_text, 2 },
    { "kof2003", RP_HUD_BARS, "hud bars solid", kof2k3_hud_bars, 2 },
    { "kof96", RP_HUD_BARS, "hud bars solid", kof96_hud_bars, 1 },
    { "kof96", RP_HUD_TEXT, "hud text solid", kof96_hud_text, 2 },
    { "kof95", RP_HUD_BARS, "hud bars solid", kof95_hud_bars, 1 },
    { "kof95", RP_HUD_TEXT, "hud text solid", kof95_hud_text, 2 },
    { "kof94", RP_HUD_BARS, "hud bars solid", kof94_hud_bars, 1 },
    { "kof94", RP_HUD_TEXT, "hud text solid", kof94_hud_text, 1 },
    { "kof2000", RP_HUD_BARS, "hud bars solid", kof2k_hud_bars, 2 },
    { "kof2000", RP_HUD_TEXT, "hud text solid", kof2k_hud_text, 2 },
    { "rbff1", RP_HUD_BARS, "hud bars solid", rbff1_hud_bars, 4 },
    { "rbffspec", RP_HUD_BARS, "hud bars solid", rbffsp_hud_bars, 10 },
    { "rbff1", RP_FX, "fx solid", rbff1_fx, 2 },
    { "rbffspec", RP_FX, "fx solid", rbffsp_fx, 2 },
    { "rbff2", RP_FX, "fx solid", rbff2_fx, 2 },
    { "fatfury1", RP_HUD_BARS, "hud bars solid", ff1_hud_bars, 2 },
    { "fatfury1", RP_HUD_TEXT, "hud text solid", ff1_hud_text, 1 },
    { "fatfursp", RP_HUD_BARS, "hud bars solid", ffsp_hud_bars, 2 },
    { "fatfursp", RP_HUD_TEXT, "hud text solid", ffsp_hud_text, 1 },
    { "fatfursp", RP_FX, "fx solid", ffsp_fx, 1 },
    { "fatfury3", RP_FX, "fx solid", ff3_fx, 2 },
    { "fatfury3", RP_HUD_BARS, "hud bars solid", ff3_hud_bars, 4 },
    { "samsho5p", RP_HUD_ANIM, "hud anim solid", ss5p_hud_anim, (int)(sizeof ss5p_hud_anim / sizeof ss5p_hud_anim[0]) },
    { "samsh5sp", RP_HUD_ANIM, "hud anim solid", ss5sp_hud_anim, (int)(sizeof ss5sp_hud_anim / sizeof ss5sp_hud_anim[0]) },
    { "samsho5", RP_HUD_ANIM, "hud anim solid", ss5_hud_anim, (int)(sizeof ss5_hud_anim / sizeof ss5_hud_anim[0]) },
    { "samsho4", RP_HUD_ANIM, "hud anim solid", ss4_hud_anim, (int)(sizeof ss4_hud_anim / sizeof ss4_hud_anim[0]) },
    { "samsho3", RP_HUD_ANIM, "hud anim solid", ss3_hud_anim, (int)(sizeof ss3_hud_anim / sizeof ss3_hud_anim[0]) },
    { "samsho", RP_HUD_ANIM, "hud anim solid", ss1_hud_anim, (int)(sizeof ss1_hud_anim / sizeof ss1_hud_anim[0]) },
    { "samsho2", RP_HUD_ANIM, "hud anim solid", ss2_hud_anim, (int)(sizeof ss2_hud_anim / sizeof ss2_hud_anim[0]) },
    { "wakuwak7", RP_HUD_BARS, "hud bars solid", ww7_hud_bars, 1 },
    { "wakuwak7", RP_FX, "fx solid", ww7_fx, 2 },
    { "galaxyfg", RP_FX, "fx solid", gf_fx, 8 },
    { "garou", RP_HUD_BARS, "hud bars solid", garou_hud_bars, 2 },
    { "rbff2", RP_HUD_BARS, "hud bars solid", rbff2_hud_bars, (int)(sizeof rbff2_hud_bars / sizeof rbff2_hud_bars[0]) },
    { "aof", RP_HUD_TEXT, "hud text solid", aof_hud_text, 1 },
    { "aof2", RP_HUD_TEXT, "hud text solid", aof2_hud_text, 1 },
    { "ninjamas", RP_HUD_TEXT, "hud text solid", nin_hud_text, 1 },
    { "savagere", RP_FX, "fx solid", sav_fx, (int)(sizeof sav_fx / sizeof sav_fx[0]) },
    { "lastblad", RP_HUD_BARS, "hud bars solid", lb1_hud_bars, (int)(sizeof lb1_hud_bars / sizeof lb1_hud_bars[0]) },
    { "lastblad", RP_FX, "fx solid", lb1_fx, (int)(sizeof lb1_fx / sizeof lb1_fx[0]) },
    { "lastbld2", RP_HUD_BARS, "hud bars solid", lb2_hud_bars, (int)(sizeof lb2_hud_bars / sizeof lb2_hud_bars[0]) },
    { "lastbld2", RP_FX, "fx solid", lb2_fx, (int)(sizeof lb2_fx / sizeof lb2_fx[0]) },
    { "kizuna", RP_HUD_BARS, "hud bars solid", kiz_hud_bars, (int)(sizeof kiz_hud_bars / sizeof kiz_hud_bars[0]) },
    { "kizuna", RP_HUD_TEXT, "hud text solid", kiz_hud_text, (int)(sizeof kiz_hud_text / sizeof kiz_hud_text[0]) },
    { "samsho5", RP_HUD_BARS, "hud bars solid", ss5_hud_bars, (int)(sizeof ss5_hud_bars / sizeof ss5_hud_bars[0]) },
    { "samsho5", RP_HUD_TEXT, "hud text solid", ss5_hud_text, (int)(sizeof ss5_hud_text / sizeof ss5_hud_text[0]) },
    { "samsh5sp", RP_HUD_BARS, "hud bars solid", ss5sp_hud_bars, (int)(sizeof ss5sp_hud_bars / sizeof ss5sp_hud_bars[0]) },
    { "samsh5sp", RP_HUD_TEXT, "hud text solid", ss5sp_hud_text, (int)(sizeof ss5sp_hud_text / sizeof ss5sp_hud_text[0]) },
    { "samsho5p", RP_HUD_BARS, "hud bars solid", ss5p_hud_bars, (int)(sizeof ss5p_hud_bars / sizeof ss5p_hud_bars[0]) },
    { "samsho4", RP_HUD_BARS, "hud bars solid", ss4_hud_bars, (int)(sizeof ss4_hud_bars / sizeof ss4_hud_bars[0]) },
    { "samsho4", RP_HUD_TEXT, "hud text solid", ss4_hud_text, (int)(sizeof ss4_hud_text / sizeof ss4_hud_text[0]) },
    { "samsho3", RP_HUD_BARS, "hud bars solid", ss3_hud_bars, (int)(sizeof ss3_hud_bars / sizeof ss3_hud_bars[0]) },
    { "samsho", RP_HUD_BARS, "hud bars solid", ss1_hud_bars, (int)(sizeof ss1_hud_bars / sizeof ss1_hud_bars[0]) },
    { "samsho", RP_HUD_TEXT, "hud text solid", ss1_hud_text, (int)(sizeof ss1_hud_text / sizeof ss1_hud_text[0]) },
    { "samsho2", RP_HUD_BARS, "hud bars solid", ss2_hud_bars, (int)(sizeof ss2_hud_bars / sizeof ss2_hud_bars[0]) },
    { "samsho2", RP_HUD_TEXT, "hud text solid", ss2_hud_text, (int)(sizeof ss2_hud_text / sizeof ss2_hud_text[0]) },
};
