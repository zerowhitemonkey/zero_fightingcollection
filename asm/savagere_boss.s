| Savage Reign: King Leo (id 10) selezionabile. Il gioco ha il personaggio completo ma limita la scelta a 0-9
| ($7180: cmpi.b #9). Ora: tenendo SU mentre si conferma un personaggio si sceglie King Leo.
| Sostituisce $7166 (move.w $c2(a4),d0 / move.b d0,(a0)); d1 (pulsanti) va preservato.
        .org 0
kl:
        move.w  0xc2(%a4),%d0
        move.b  0x10fd96,%d2            | P1: ingressi correnti BIOS (1 = premuto)
        btst    #0,0x37(%a4)
        bne.s   chk
        move.b  0x10fd9c,%d2            | P2
chk:
        btst    #0,%d2                  | SU
        beq.s   done
        moveq   #10,%d0                 | King Leo
done:
        move.b  %d0,(%a0)
        jmp     0x716c
