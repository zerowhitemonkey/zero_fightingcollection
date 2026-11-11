| The Last Blade 2: modalita' EX selezionabile come POWER e SPEED (Ability Select).
| Il gioco gestisce gia' EX (modo 3, scritta "EX") ma solo col codice
| SPEED C x6, POWER B x3, SPEED C x4 ($1B552). Ora: GIU' = POWER -> SPEED -> EX -> POWER,
| SU = al contrario. Oggetto a4: $1CC angolo insegna (0 = POWER, $80 = SPEED, a riposo se &$7F = 0),
| $1CD passo rotazione, $1CE modo (1/2/3), $1CF ultimo modo, $1D0 flag cambio.
| d5 = levetta (fronti): bit0 SU, bit1 GIU'.
        .org 0
| --- sostituisce il controllo del codice ($1B552, chiamato con bsr; d0=0 -> nessun EX)
chk:
        move.b  0x1cc(%a4),%d0
        andi.b  #0x7f,%d0
        bne.s   no                      | insegna in rotazione
        tst.b   0x1cc(%a4)
        bmi.s   atspeed
        btst    #0,%d5                  | POWER + SU -> EX
        bne.s   yes
        bra.s   no
atspeed:
        btst    #1,%d5                  | SPEED + GIU' -> EX
        bne.s   yes
no:
        moveq   #0,%d0
        rts
yes:
        move.b  #3,0x1cf(%a4)           | ultimo modo = EX
        move.b  #-1,0x1d0(%a4)
        move.w  #0x60,%d0               | suono di cambio
        jsr     0x2725e
        moveq   #1,%d0                  | il chiamante imposta modo 3 e passa a exst
        rts
| --- stato "EX selezionato" (al posto di $1A932): SU/GIU' tornano a SPEED/POWER
exst:
        movea.l 0x18(%a4),%a3           | tempo scaduto ($100 dell'oggetto padre = timer BCD):
        tst.b   0x100(%a3)              | conferma EX come fa il gioco per POWER/SPEED ($1A89A)
        bne.s   intime                  | (l'originale in EX restava fermo a 00)
        move.l  #0x1a972,(%a4)
        jmp     0x1a95c
intime:
        jsr     0x1aa4a                 | legge gli ingressi in d4-d7
        btst    #1,%d5
        bne.s   topower
        btst    #0,%d5
        bne.s   tospeed
        jmp     0x1a932                 | conferma con A/D come prima
topower:
        move.b  #0x00,0x1cc(%a4)
        move.b  #1,0x1ce(%a4)
        bra.s   back
tospeed:
        move.b  #0x80,0x1cc(%a4)
        move.b  #2,0x1ce(%a4)
back:
        clr.b   0x1cd(%a4)
        move.l  #0x1a88a,(%a4)          | torna allo stato normale
        lea     0x1ab3a,%a0             | ricrea le due facce dell'insegna POWER/SPEED
        move.w  #0x6810,%d0             | (come all'inizio di Ability Select, $1A81C)
        jsr     0x164e
        lea     0x1ac3e,%a0
        move.w  #0x6820,%d0
        jsr     0x164e
        jsr     0x1aa4a                 | ricarica gli ingressi (d5-d7 usati da $1A908)
        jmp     0x1a908                 | suono + aggiornamento, poi coda $1A932
| --- insegna "EX" (stato $1ADAA dell'oggetto insegna): se il giocatore ha lasciato EX, si elimina
exsign:
        movea.l 0x18(%a4),%a3
        cmpi.b  #3,0x1ce(%a3)
        bne.s   kill
        jmp     0x1adaa
kill:
        jmp     0x176a
