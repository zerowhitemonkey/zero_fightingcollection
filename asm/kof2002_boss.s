| KOF 2002: dopo il codice dei tasti della selezione ($EA24: ultime 8 direzioni = SU GIU DESTRA SINISTRA GIU SU SINISTRA DESTRA)
| il gioco trasforma Kyo (id 0) in Kusanagi ($28). Esteso: sulla casella in basso a destra (id $26) -> Omega Rugal ($27).
| Chiamato con jmp da $EA6A (al posto di cmpi.b #0,$32(a4) / bne / move.b #$28,$32(a4)); termina con rts.
        .text
        move.b  0x32(%a4),%d0
        bne.s   1f
        move.b  #0x28,0x32(%a4)
        rts
1:      cmpi.b  #0x26,%d0
        bne.s   2f
        move.b  #0x27,0x32(%a4)
2:      rts
