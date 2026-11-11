| Savage Reign: SPECIAL EFFECTS (opzione). Dalla routine ombre ($7FF00), per gli oggetti disegnati a frame
| alterni: quelli di nome "FLY ARMS" (armi lanciate), "EFCT DST" (polvere), "FNS EFCT" (effetto finale)
| sempre disegnati; gli altri (IMAGE = sagome del personaggio, PILE ACT, DEVICE01) come l'originale.
| Attivata da $7FF12: jmp $1ED50 -> jmp $7FE00.
        .org 0
fx:
        cmpi.l  #0x464c5920,0x10(%a4)   | "FLY "
        beq.s   draw
        cmpi.l  #0x45464354,0x10(%a4)   | "EFCT"
        beq.s   draw
        cmpi.l  #0x464e5320,0x10(%a4)   | "FNS "
        beq.s   draw
        jmp     0x1ed50                 | parita' originale
draw:
        jmp     0x1ed74
