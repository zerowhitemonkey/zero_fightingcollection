| The Last Blade: ombre P1/P2 (layer $190/$192) disegnate a frame alterni
| (btst #0,$293). Ora entrambe ogni frame.
| (I riflessi a $f1f6 hanno lo stesso schema ma il codice non e' mai chiamato.)
        .org 0
| --- ombre: chiamata da $efca (jmp) con a4 = oggetto ombre
shadow:
        move.w  0x1d9c(%a5),%d0
        move.w  0x190(%a4),%d1
        bclr    %d1,%d0
        move.w  0x192(%a4),%d1
        bclr    %d1,%d0
        move.w  %d0,0x1d9c(%a5)
        jsr     0xefd4          | parte P1 (posizione + bit $190)
        jmp     0xf072          | parte P2 (posizione + bit $192), rts al chiamante
