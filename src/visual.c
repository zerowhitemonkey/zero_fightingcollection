// SNKHD - aspetto per gioco: ombre (SHADOWS), interfaccia (INTERFACE + INTERFACE AREA), effetti (SPECIAL EFFECTS).
// Tabelle nel formato .bld di FBNeo esteso (vedi emu.c): "min-max" tile sprite, "P xx" palette sprite,
// "FP x" palette fix, "FT min-max" tile fix. Le patch SOLID (niente lampeggi) sono in rompatch_opt.h.
// Ricavate con il banco di prova (tools/analysis): mappa delle sorgenti dei pixel (IDMAP), finestre di 64 frame
// per i lampeggi, verifica visiva con e senza tabella.
#include <string.h>
#include "visual.h"
#include "rompatch.h"
#include "fx_tables.h"   // tabelle SPECIAL EFFECTS generate (tools/analysis/fxtable.py)

typedef struct {
    const char *id;
    const char *shadows;   // ombre dei personaggi
    const char *bars;      // INTERFACE AREA = BARS: barre HP / POW / gauge, icone, contatori
    const char *text;      // in piu' con FULL: timer, punteggio, nomi, messaggi
    const char *fx;        // SPECIAL EFFECTS (scie delle armi, proiettili) - NULL = selettore assente
    const char *gate;      // incontro in corso: "T min-max" (tile sprite) o "FT min-max" (tile fix) presenti
} vis_t;

static const vis_t tabs[] = {
    // SS1: interfaccia sul fix layer. Barre palette fix 3/8, KO palette 0 (END e lampeggio KO tile D18-D2F in palette 1),
    // POW palette 6, icone V tile 366-36B; scritte (nomi, punteggio, timer, PUSH START) palette 1 e 5 (timer evidenziato).
    { "samsho",   "P 20 2", "FP 0\nFP 3\nFP 6\nFP 8\nFT 366-36B\nFT D18-D2F", "FP 1\nFP 5", NULL, "FT 480-4A3" },
    // SS2: interfaccia tutta sul fix layer. Barre/KO/END palette fix 1-3 (0 = bianco del lampeggio vita bassa),
    // cartelli e gauge POW + frecce dei comandi palette 6, icone V tile 366-36B; scritte palette 5, timer palette 7.
    { "samsho2",  "16E1C-16E4B 2", "FP 0-3\nFP 6\nFT 366-36B", "FP 5\nFP 7", NULL, "FT 480-4A7" },
    // SS3: interfaccia sul fix layer. Barre palette fix 8/9, VS/DIE 5 e B (A durante il ciclo di colore), barra POW 6,
    // cartello POW e fiamme 7, icone V tile 366-36B, frecce dei comandi tile 760-779; scritte: timer 2, nomi 3, punteggio 4.
    // La palette fix C e' il bordo nero ai lati dello schermo: esclusa.
    // Ombre: palette $20, piu' le ellissi disegnate nella palette del personaggio (Rimururu tile A2DC-A2ED,
    // Galford/Hanzo A2EE-A2F9 + centro pieno 94AC: con la sola "P 20" restavano piene a 75/50%).
    { "samsho3",  "P 20 2\nA2DC-A2F9 2\n94AC 2", "FP 5-B\nFT 366-36B\nFT 760-779", "FP 2-4", NULL, "FT 800-8E3" },
    // SS4: interfaccia mista. Barre: riempimento sprite tile 10-9F (palette 02/07/0B), cornici tile fix 3F4-3FC,
    // pallini dei giocatori tile 3BA e palette fix C, POW in alto e in basso palette fix 1, barre POW 9-A,
    // riquadri dei round 8, frecce dei comandi 5. Scritte: palette fix 3 (orologio, nomi, timer, PUSH START) e 6 (SLASH).
    // La palette fix 0 e' la fascia nera in alto: esclusa.
    { "samsho4",  "P 20 2", "10-9F\nFP 1\nFP 5\nFP 8-A\nFP C\nFT 3BA\nFT 3F4-3FC", "FP 3\nFP 6", NULL, "FT 3F4-3FC" },
    // SS5 / 5SP / 5PE: barre della vita tile fix 880-8FF (palette 3), barra POW di P1 tile E48-E7B, POW/KO palette 1,
    // barre sotto la vita 5, riquadri dei round 8, barre POW 9-A, pallini C e tile 3BA. Scritte: palette 3 e 6
    // (orologio, nomi, timer, PUSH START). La palette fix 0 e' la fascia nera in alto: esclusa.
    { "samsho5", "7B3A6-7B3E8 2", "FT 880-8FF\nFT E48-E7B\nFP 1\nFP 5\nFP 8-A\nFP C\nFT 3BA", "FP 3\nFP 6", NULL, "FT 880-8FF" },
    { "samsh5sp", "7C9E6-7CA76 2", "FT 880-8FF\nFT E48-E7B\nFP 1\nFP 5\nFP 8-A\nFP C\nFT 3BA", "FP 3\nFP 6", NULL, "FT 880-8FF" },
    { "samsho5p", "7AEA1-7AF31 2", "FT 880-8FF\nFT E48-E7B\nFP 1\nFP 5\nFP 8-A\nFP C\nFT 3BA", "FP 3\nFP 6", NULL, "FT 880-8FF" },
    // Kizuna: interfaccia sul fix layer. Barre/ritratti/CHANGE palette fix 1-5, 8-A, D, gauge tile E41-E55, scritta RUSH
    // tile DE0-DEF + cifre del contatore (sprite 1ED28-1ED3B); scritte (nomi, punteggio, COUNT, PUSH START) palette 6-7
    // e tile D2D-D50. La tile 382 (bordi ai lati, palette B) e' esclusa.
    { "kizuna",   "P D1 2", "FP 1-5\nFP 8-A\nFP D\nFT E41-E55\nFT DE0-DEF\n1ED28-1ED3B", "FP 6\nFP 7\nFT D2D-D50", NULL, "FT 300-331" },
    // The Last Blade: barre palette fix 4-7 (POWER/SPEED, gauge), F (riempimento vita), 8 (segni dei round),
    // cornice e stemma del timer tile E60-E63/EAE-EDF (palette 3), contatore "Hits!" tile fix DEC-DFF + cifre sprite 35324-3533F; scritte: punteggio
    // e nomi palette D, PRESS START palette E, cifre del timer tile EE8-F2F. Palette fix 0 = fascia nera, esclusa.
    // Effetti: tile dei pezzi d'animazione col flag $2000 (scie delle spade, proiettili), raccolte dal gioco.
    { "lastblad", "P D0 2", "FT E60-E63\nFT EAE-EDF\nFP 4-7\nFP 8\nFP F\nFT DEC-DFF\n35324-3533F", "FP D\nFP E\nFT EE8-F2F", fx_lastblad, "FT EAE-EBF" },
    // The Last Blade 2: barre tile fix E30-E6E (vita e cornice, palette 2), palette 4-7 (POWER/SPEED, gauge),
    // A (segni dei round), C e F (vita), 8 "counter!!", 9 contatore "Hits!"; scritte: punteggio/PRESS START
    // palette B, nomi palette E, cifre del timer tile E80-ECF (palette 2).
    // Effetti: tabella FBNeo + tile dei pezzi col flag $2000 raccolte dal gioco.
    { "lastbld2", "56D37-56D3F 2", "FT E30-E6E\nFP 4-7\nFP 8-A\nFP C\nFP F", "FP B\nFP E\nFT E80-ECF", fx_lastbld2, "FT E30-E52" },
    // Art of Fighting: barre tile fix 507-518 (vita, palette 9), riquadro timer 600-61F, palette D/F, cornici dei
    // ritratti EF0-EF8, palline dei round (sprite D1F0-D1FF); scritte: palette 4 (punteggi, PLAYER 2...), 2 (HI, nomi),
    // 3 (messaggi YOU LOST...), cifre del timer F06-F77.
    { "aof",      "3596-35C7 2", "FT 507-518\nFT 600-61F\nFP D\nFP F\nFT EF0-EF8\nD1F0-D1FF", "FP 4\nFP 2\nFP 3\nFT F06-F77", NULL, "FT 507-518" },
    // AOF2: barre tile fix 300-311 e 400-41F (vita, palette 5), palette 4/A (spirito), C (medaglia WIN); scritte: palette 2,
    // cifre del timer F24-F9F. Ombre: palette $20 condivisa, solo le tile delle ellissi.
    { "aof2",     "503A-508E 2", "FT 300-311\nFT 400-41F\nFP 4\nFP A\nFP C", "FP 2\nFT F24-F9F", NULL, "FT 300-311" },
    // AOF3: barre tile fix F00-F4F (palette 3), icona del round D70-D73; scritte: cifre COUNT E68-EB7, palette 4
    // (messaggi), 5 (ENTER THE BATTLE), 6 (orologio), 7 (STAGE), 8 (nomi). Palette F = fascia nera in alto, esclusa.
    // Ombre: ellisse grigia (palette $32 condivisa con l'HUD).
    { "aof3",     "6A8-6C1 2", "FT F00-F4F\nFT D70-D73", "FT E68-EB7\nFP 4-8", NULL, "FT F00-F11" },
    // Savage Reign: barre tile fix 46D-47F (vita, palette 4/5), segni V palette 8; scritte: palette 1 (P1),
    // 7 (P2), 2 (timer, VS), 0/3/9 (messaggi). Tile 480-487 = bordi neri ai lati, esclusi.
    // Effetti: oggetti FLY ARMS / EFCT DST / FNS EFCT (asm/savagere_fx.s), tile raccolte dal gioco.
    { "savagere", "P 21 2", "FP 8\nFT 46D-47F", "FP 1\nFP 7\nFP 2\nFP 0\nFP 3\nFP 9", fx_savagere, "FT 46D-47F" },
    // Ninja Master's: barre palette fix 1 e 3 (vita, nomi), 4 (POWER); scritte: cifre del timer/TIME 500-5FF,
    // PUSH START 630-66F, orologio palette D. Tile 20 in palette 0 = fascia nera in alto, esclusa.
    { "ninjamas", "P AF 2", "FP 1\nFP 3\nFP 4", "FT 500-5FF\nFT 630-66F\nFP D", NULL, "FT 720-7FB" },
    // Garou: barre tile fix 590-59F/5F0-5FF/6A0-6AF (vita, palette 9), palette B (vita), 6 (gauge S power), E (icone
    // dei round), scritta S power 3429-342E; scritte: palette 7 (TOTAL, punteggio, PRESS START), C/D (nomi), 3/4
    // (1P/COM), cifre del timer B00-BFF. Ombre: sagome del lottatore.
    { "garou",    "P FF 2", "FT 590-59F\nFT 5F0-5FF\nFT 6A0-6AF\nFP B\nFP 6\nFP E\nFT 3429-342E", "FP 7\nFP C\nFP D\nFP 3\nFP 4\nFT B00-BFF", NULL, "FT 510-51F" },
    // KOF '94: ombre palette $FE (solo ombre, y 183-214). Barre: vita palette fix 5/6 (tile 800-836), POW 7, MAXIMUM 8,
    // ritratti e icone della squadra sprite tile 1F690-1F77F (palette variabili). Scritte: PUSH START 1, TIME 2,
    // punteggio/nomi 3 (P1) e 4 (P2), cifre del timer 9/A. Palette fix 0 = bordi (tile 38C), esclusa. Gate: barre vita.
    { "kof94",    "P FE 2", "FP 5-8\n1F690-1F77F", "FP 1-4\nFP 9-A", NULL, "FT 800-836" },
    // KOF '95: ombre palette $FE (negli incontri solo ombre, y >= 200; la usano anche le fasce nere delle schermate di vittoria:
    // trasparenza solo negli incontri). Barre: vita palette fix 4/5 (tile 800-856, 5 = barra vita bassa), icone squadra e POW 6,
    // cornici dei ritratti e POW 7, TIME UP e cartelli B; ritratti sprite 1E/4E, icone squadra 2E-3F/5E-6F. Scritte: PUSH START 1,
    // 1P/punteggio/CHALLENGER/nomi 2 (P1) e 3 (P2), TIME 8, cifre del timer 9/A. Palette fix 0 = bordi, esclusa. Gate: barre vita.
    { "kof95",    "P FE 2", "FP 4-7\nFP B\nP 1E\nP 2E\nP 2F\nP 3E\nP 3F\nP 4E\nP 5E\nP 5F\nP 6E\nP 6F", "FP 1-3\nFP 8-A", NULL, "FT 800-808" },
    // KOF '96: ombre palette $FE (negli incontri; fuori la usano anche schermate fisse: solo negli incontri). Barre: vita
    // palette fix 4/5 (tile B00-CFE, 5 = vita bassa), POW 6, MAXIMUM 7, icone della squadra A-D, ritratti sprite 1F/2F.
    // Scritte: PUSH START 1, 1P/punteggio/nomi 2 (P1) e 3 (P2, CHALLENGER), riquadro e cifre del timer 8/9. Gate: barre vita.
    { "kof96",    "P FE 2", "FP 4-7\nFP A-D\nP 1F\nP 2F", "FP 1-3\nFP 8-9", NULL, "FT BEE-BEE" },
    // KOF '98: ombre palette $FE. Barre: cornice gauge POW 5, vita + gauge P1 6 (tile B00-CB7) / P2 8, varianti 7/9, scorte
    // ADV/EXTRA D, F, ritratti sprite tile A520-A5AF (palette 17/37). Scritte: 1P/punteggio/nomi 1-3, nomi della squadra B,
    // cifre del timer E (palette 0 alternata col timer basso). Gate: barra vita di P1.
    { "kof98",    "P FE 2", "FP 5-9\nFP D\nFP F\nA520-A5AF", "FP 1-3\nFP B\nFP E\nFT 800-8B7", NULL, "FT B00-B5F" },
    // KOF '97: ombre palette $FE (solo ombre). Barre: vita + gauge P1 6 (tile B00-EDC) / P2 8, varianti 7/9, cornici del gauge e
    // TIME 5, MAX 4, icone della squadra e COUNTER!! A-D, F, ritratti sprite 1F/3F. Scritte: punteggio/nomi/PRESS START 1-3,
    // cifre del timer E. Palette fix 0 = bordi, esclusa. Gate: barra vita di P1.
    { "kof97",    "P FE 2", "FP 4-D\nFP F\nFT DCC-DDD\nP 1F\nP 3F", "FP 1-3\nFP E\nFT 800-8A7", NULL, "FT B00-B3F" },
    // KOF '99: ombre palette $FE (y >= 199; qualche pixel isolato di uno stage). Barre: vita palette fix 5/6 (tile
    // B00-CB0), icona POW + gauge 9 (P1) / A e 7 (P2), gauge striker 8, icone striker D/E, "n HITS" F, ritratti sprite
    // 1E/3E. Scritte: punteggio B.A., nomi, CHALLENGER e liste della squadra 2/3, ST-n 4, cornice e cifre del timer
    // tile 800-8B1 (palette fix 0 condivisa con i bordi 3DE) e 1. Gate: barra vita di P1.
    { "kof99",    "P FE 2", "FP 5-A\nFP D-F\nP 1E\nP 3E", "FP 1-4\nFT 800-8B1", NULL, "FT B00-CB0" },
    // KOF 2000: ombre = sagome del lottatore schiacciate, palette $FE (usata solo dalle ombre, y 200-229).
    // Interfaccia sul fix layer (tile a banchi) + ritratti grandi sprite palette 1F/3F. Barre: vita palette fix 5/6,
    // POW e scritte MAX/COUNTER MODE 9 (varianti 7), barra degli striker 8, icone degli striker D/E, contatore
    // combo "n HITS" A, messaggio "Counter!" B/7. Scritte: nomi, squadra, punteggio B.A./W., CHALLENGER palette 2/3,
    // PRESS START 4, timer e ST-n tile 3900-3973 (palette 0, condivisa con i bordi neri 23DE: per tile).
    // Gate: barre della vita (VRAM a 12 bit: 800-8BF = tile 3800-38BF del banco 3).
    // Effetti speciali: non trattati (scelta dell'utente).
    { "kof2000",  "P FE 2", "FP 5-9\nFP A\nFP B\nFP D\nFP E\nP 1F\nP 3F", "FP 2-4\nFT 3900-3973", NULL, "FT 800-8BF" },
    // KOF 2001: ombre palette $FE (solo ombre, y >= 200). Barre: vita palette fix 3 (tile B00-CEF), gauge POW 4/5, contatore
    // combo "n HITS" 2, icone della squadra/striker 7-C, ritratti sprite F4/F5, cornice del timer sprite DF. Scritte:
    // PRESS START e ST-n palette 6, punteggio/CHALLENGER/BEAT BY E (P1) e F (P2), cifre del timer sprite tile 7A7C0-7A7DF
    // (palette variabili D0-DE). Palette fix 0 = bordi neri (tile 3DE), esclusa. Gate: barre della vita.
    { "kof2001",  "P FE 2", "FP 2-5\nFP 7-C\nP F4\nP F5\nP DF", "FP 6\nFP E-F\n7A7C0-7A7DF", NULL, "FT B00-CEF" },
    // KOF 2003: ombre = sagome palette $7F (y >= 207). Interfaccia sul fix layer a banchi per tile (VRAM $7500; il gioco
    // alternava i banchi 5555/AAAA a ogni frame per il retino: fissato a 5555 nella .neo). Barre: cornice/retino vita 1, 3,
    // riempimento vita palette 2 tile 2400-25FF/3400-35FF, gauge e scorte 4/5 (con CHANGE OK), RUSH n A-C. Scritte: WIN,
    // punteggio, PRESS START 8/9, timer 6, nomi della squadra palette 2 tile 2B00-2CFF/3B00-3CFF. Gate: retino della vita
    // (VRAM a 12 bit 400-408).
    { "kof2003",  "P 7F 2", "FP 1\nFP 3\nFT 2400-25FF\nFT 3400-35FF\nFP 4-5\nFP A-C", "FP 6\nFP 8-9\nFT 2B00-2CFF\nFT 3B00-3CFF", NULL, "FT 400-408" },
    // KOF 2002: ombre = sagome del lottatore, palette $FE (solo ombre, y 187-223). Interfaccia sul fix layer (niente banchi)
    // + ritratti sprite palette F4/F5 (angoli in alto). Barre: vita palette fix 5/6 (tile B00-BEF / C00-CEF), gauge POW
    // 7/8, messaggi combo/"n HIT" 9, "COUNTER!" A, cornice del timer E. Scritte: PRESS START e ST-n palette 2, WIN/punteggio
    // e nomi 3 (P1) e 4 (P2), cifre del timer D. Palette fix 0 = bordi neri (tile 3DE), esclusa. Gate: barra vita di P1.
    // Effetti speciali: non trattati (scelta dell'utente).
    { "kof2002",  "P FE 2", "FP 5-8\nFP 9\nFP A\nFP E\nP F4\nP F5", "FP 2-4\nFP D", NULL, "FT B00-BEF" },
    // Waku Waku 7: barre palette fix 8/9 (vita), A-D (POW), 4-7 (messaggi e contatore combo: SUPER!, CHARGE!,
    // n HIT COMBO); scritte: punteggio 32B-35F, TIME 4E8-518, palette 1-3 (PUSH START, timer, nomi).
    // Effetti: oggetti a frame alterni non-ombra (asm/wakuwak7_fx.s), tile raccolte. Ombre: sagome del lottatore.
    { "wakuwak7", "P 50 2", "FP 8-D\nFP 4-7", "FT 32B-35F\nFT 4E8-518\nFP 1-3", fx_wakuwak7, "FT 682-707" },
    // Galaxy Fight: ombre palette $20 (ellissi), barre FP 5/6, scritte FP 3/4/F; effetti fx_galaxyfg; gate barre vita.
    { "galaxyfg", "P 20 2", "FP 5\nFP 6", "FP 3\nFP 4\nFP F", fx_galaxyfg, "FT 35E-368" },
    // RBFF2: barre tile fix ED0-EFF (vita, palette B/C/D), palette 4/5 (POWER), 8/9 (gauge), E (stelle), F (ritratti),
    // stella F00-F03; scritte: timer E78-EBF, palette 2 (nomi), 3 (HIT/TOTAL), icone 1P/COM 7D8-7FF.
    // Effetti: oggetti "a frame alterni" ($1E & $41: polvere...), tile raccolte. Ombre: sagome del lottatore.
    // Fatal Fury: barre palette fix 7 (360-379, 487-490), ritratti palette 4, segni dei round FT 301-310; scritte (palette 0
    // condivisa): font 330-35F (TIME, 2P PUSH START), timer 39F-3F2, punteggio e nomi D80-DBF. Niente ombre ne' effetti a flicker.
    { "fatfury1", NULL, "FP 7\nFP 4\nFT 301-310", "FT 330-33F\nFT 341-35F\nFT 39F-3F2\nFT D80-DBF", NULL, "FT 360-379" },
    // Fatal Fury Special: barre FT 360-36F (palette 6, 7 = lampeggio vita bassa, 5 = parte persa), ritratti palette 4, segni dei
    // round FT 300-310; scritte: palette 5 (P1/P2, timer), 8 (PUSH START, nome evidenziato), font F30-F5F (punteggio, nomi,
    // BEAT BY). Effetti: oggetti "a frame alterni" (polvere, proiettili, fiamme, mantello di Krauser). Ombre: tile 7B39-7B5C, 7BE4-7BE5.
    { "fatfursp", "7B39-7B5C 2\n7BE4-7BE5 2", "FT 360-36F\nFP 4\nFT 300-310", "FP 5\nFP 8\nFT F30-F5F", fx_fatfursp, "FT 360-36F" },
    // Fatal Fury 3: barre tile fix ECD-ED9 + 300-30F/370-3BF (palette 3, 4 = lampeggio vita bassa), icone dei lottatori
    // palette E/F, CD8-CDF, WON palette A; scritte: timer E75-ECC, palette 6 (punteggio, nomi), 8 (CHALLENGER), 9 (BEAT BY),
    // icone 1P/2P B48-B49/C73-C74. Effetti: oggetti "a frame alterni" (polvere), tile raccolte. Ombre: palette EA.
    { "fatfury3", "P EA 2", "FT ECD-ED9\nFT 300-30F\nFT 370-3BF\nFP E\nFP F\nFP A\nFT CD8-CDF", "FT E75-ECC\nFP 6\nFP 8\nFP 9\nFT B48-B49\nFT C73-C74", fx_fatfury3, "FT ECD-ED9" },
    // Real Bout Fatal Fury: barre tile fix ED0-EFF (vita, palette B/C/D), palette 6 (segni dei round), 8/A (gauge
    // POWER), E/F (ritratti); scritte: timer E74-E9F, palette 1 (nomi), 2, 3 (TOTAL/HIT), icone 1P/COM 7D8-7FF.
    // Effetti: oggetti "a frame alterni" ($1E & $41: polvere, apparizioni delle super...), tile raccolte. Ombre: palette FF.
    { "rbff1",    "P FF 2", "FT ED0-EFF\nFP 6\nFP 8\nFP A\nFP E\nFP F", "FT E74-E9F\nFP 1\nFP 2\nFP 3\nFT 7D8-7FF", fx_rbff1, "FT ED0-EFF" },
    // Real Bout Special: come RBFF2 (barre ED0-EFF, palette 4/5 POWER, 8/9 gauge, E/F ritratti; scritte timer E74-EBF,
    // palette 2 nomi, 3 HIT/TOTAL). Effetti e ombre: come Real Bout.
    { "rbffspec", "P FF 2", "FT ED0-EFF\nFP 4\nFP 5\nFP 8\nFP 9\nFP E\nFP F", "FT E74-EBF\nFP 2\nFP 3\nFT 7D8-7FF", fx_rbffspec, "FT ED0-EFF" },
    { "rbff2",    "P FF 2", "FT ED0-EFF\nFP 4\nFP 5\nFP 8\nFP 9\nFP E\nFP F\nFT F00-F03", "FT E78-EBF\nFP 2\nFP 3\nFT 7D8-7FF", fx_rbff2, "FT ED0-EFF" },
    // Rage of the Dragons: ombre = sagome del lottatore nella palette $12 (un solo colore, solo y >= 204).
    // Barre: fix 8 (vita, tile 700-79C), 9 (seconda barra / CHANGE), A (barrette e cifre del gauge), 7 (gauge in basso),
    // 5 (cornici); ritratti sprite palette 3B/3C, 4B/4C, 5B/5C, 6B/6C (angoli in alto); contatore HIT, comandi e
    // HERE COMES CHALLENGER sprite palette 23/24. Scritte: fix 0 (punteggio, PRESS START), 1 (1P/2P, CHALLENGER),
    // 6/D (timer), B (CHANGE OK), C (nomi). Gate: barra della vita.
    { "rotd", "P 12 2", "FP 5\nFP 7-A\nP 3B-3C\nP 4B-4C\nP 5B-5C\nP 6B-6C\nP 23-24", "FP 0-1\nFP 6\nFP B-D", NULL, "FT 700-79C" },
    // CPS3 - Red Earth: l'interfaccia e' tutta sullo strato testo (tile 8x8 disegnate sopra gli sprite).
    // "FP" = palette dello strato testo. Barre: vita 0B (17 = lampeggio quando si viene colpiti), ritratti 12-15,
    // icona del boss 16, cornice della barra del boss 05, sfere 0F/01, stemma del timer e riquadri 11.
    // Scritte (FULL): tutto il resto (timer 07, nomi 0C, classi 08, EXP/LV 04, FREE PLAY 06).
    // Ombre: gia' trasparenti sull'hardware (fusione di palette), nessun selettore. Gate: barra della vita presente.
    { "redearth", NULL, "FP 01\nFP 05\nFP 0B\nFP 0F\nFP 11-17", "FP 00-FF", NULL, "FP 0B\nFP 17" },
    // Psikyo PS3-V1 - Daraku Tenshi: ombre = sprite con palette 0xFE (renderer del core, emu.c -> psikyo_set_shadows);
    // il lampeggio (un'ombra per frame) e' tolto dalle patch ROM in psikyocfg.c. Interfaccia ed effetti come l'originale.
    { "daraku", "P FE", NULL, NULL, NULL, NULL },
    // IGS PGM - Martial Masters: ombre = ellissi (boff 0x760837-0x761509, alte 11 righe, criterio in pgmcfg.c), trasparenza
    // nel renderer del core (emu.c -> pgm_set_shadows); il lampeggio (ogni ombra a frame alterni) e' tolto da patch ROM.
    { "martmast", "P 0", NULL, NULL, NULL, NULL },
};

static const vis_t *find(const char *id) {
    for (size_t i = 0; i < sizeof tabs / sizeof tabs[0]; ++i)
        if (!strcmp(tabs[i].id, id)) return &tabs[i];
    return NULL;
}

const char *shadow_table(const char *id) { const vis_t *v = find(id); return v ? v->shadows : NULL; }

int visual_has_shadows(const char *id) { const vis_t *v = find(id); return v && v->shadows; }
int visual_has_hud(const char *id) {
    const vis_t *v = find(id);
    return (v && (v->bars || v->text)) || rompatch_has_options(id, RP_HUD_BARS | RP_HUD_TEXT | RP_HUD_ANIM);
}
int visual_has_fx(const char *id) { const vis_t *v = find(id); return (v && v->fx) || rompatch_has_options(id, RP_FX) || !strcmp(id, "gaxeduel"); }

static int weight(int mode) { return mode == VIS_75 ? 0xc0 : mode == VIS_50 ? 0x80 : 0; }

// 0 = piene (nessuna trasparenza), 1 = 75%, 2 = 50% (opacita' dell'ombra)
int shadow_apply(const char *id, int mode) {
    const char *t = shadow_table(id);
    if (!t || mode <= 0) return emu_blend_set(EMU_BLEND_SHADOWS, NULL, 0, 0), 0;
    // KOF '98 / '95: la palette $FE delle ombre serve anche ai pannelli neri di CONTINUE / VS / vittoria: ombre trasparenti solo negli incontri
    int gated = !strcmp(id, "kof98") || !strcmp(id, "kof95") || !strcmp(id, "kof96");
    return emu_blend_set(EMU_BLEND_SHADOWS, t, mode == 1 ? 0xc0 : 0x80, gated);
}

void visual_apply(const char *id, int shadows, int hud, int area, int fx) {
    const vis_t *v = find(id);
    // ST-V (Golden Axe: The Duel): aure dei personaggi riconosciute nella lista dei comandi VDP1 (vdp1.cpp, FX_Prescan):
    // SOLID = colore fisso (niente alternanza bianco/blu), 75/50 = colore fisso + fusione col fondo nel VDP2
    if (!strcmp(id, "gaxeduel")) emu_stv_fx(fx);
    shadow_apply(id, shadows);
    unsigned flags = 0;
    if (hud >= VIS_SOLID) flags |= RP_HUD_BARS | (area == AREA_FULL ? RP_HUD_TEXT : 0);
    if (hud == VIS_SOLID) flags |= RP_HUD_ANIM;   // con 75/50% le parti animate (fiamme del POW) restano animate
    if (fx >= VIS_SOLID) flags |= RP_FX;
    rompatch_options(id, flags);
    int wh = weight(hud), wf = weight(fx);
    emu_blend_set(EMU_BLEND_BARS, v && wh ? v->bars : NULL, wh, 1);
    emu_blend_set(EMU_BLEND_TEXT, v && wh && area == AREA_FULL ? v->text : NULL, wh, 1);
    emu_blend_set(EMU_BLEND_EFFECTS, v && wf ? v->fx : NULL, wf, 1);
}

// Da chiamare dopo ogni frame emulato: trasparenze dell'interfaccia/effetti solo durante gli incontri
void visual_frame(const char *id) {
    const vis_t *v = find(id);
    int on = 1;
    if (v && v->gate) {
        unsigned lo, hi;
        if (!strncmp(v->gate, "FP", 2)) {
            // una o piu' palette del fix layer / strato testo (righe "FP xx"): basta che una sia presente
            on = 0;
            for (const char *q = v->gate; q && *q && !on; q = strchr(q, '\n'), q = q ? q + 1 : NULL)
                if (!strncmp(q, "FP", 2) && sscanf(q + 2, "%x", &lo) == 1) on = emu_blend_fixpal_present(lo);
        }
        else if (!strncmp(v->gate, "FT", 2)) on = sscanf(v->gate + 2, "%x-%x", &lo, &hi) == 2 && emu_blend_fix_present(lo, hi);
        else on = sscanf(v->gate + (v->gate[0] == 'T'), "%x-%x", &lo, &hi) == 2 && emu_blend_tile_present(lo, hi);
    }
    emu_blend_gate(on);
}
