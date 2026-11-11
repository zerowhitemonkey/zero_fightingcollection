| Savage Reign: ombre disegnate ogni frame.
| $1ED46 salta a frame alterni gli oggetti con bit 1 di $B(obj) (parita' $551(a5), invertita
| per giocatore): ombre, ma anche effetti (EFCT DST, IMAGE, PILE ACT) che restano come l'originale.
| Qui: oggetti con nome "SHAD..." ($10(obj)) sempre disegnati.
        .org 0
chk:
        btst    #1,0xb(%a4)
        beq.s   draw
        cmpi.l  #0x53484144,0x10(%a4)   | "SHAD"
        beq.s   draw
        jmp     0x1ed50                 | logica originale (parita')
draw:
        jmp     0x1ed74
