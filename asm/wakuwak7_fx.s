| Waku Waku 7: SPECIAL EFFECTS (opzione). Gli oggetti col flag "a frame alterni" (bit 3) che non sono ombre
| arrivano qui (da $3827A, hook ombre di wakuwak7_noflicker.json): scintille e simili sempre disegnati;
| il gruppo $1031FC-$10359F (lettere dei messaggi a tutto schermo) resta come l'originale.
        .org 0
fx:
        cmpa.l  #0x1031fc,%a6
        blt.s   draw
        cmpa.l  #0x1035a0,%a6
        bge.s   draw
        btst    #0,-0x4fcf(%a5)         | parita' originale
        jmp     0x3c9b2
draw:
        bclr    #2,%d4
        jmp     0x3c9f0
