| Rage of the Dragons: Johann (id $0F, boss finale) selezionabile. Abubo ($0E, casella centrale nascosta)
| e' sbloccato dalla patch a $6AA2E (casella sempre aperta, come col codice del gioco).
| Al posto di $6B8F0 (bsr $6A9E2 / bsr $6B916 alla conferma di un personaggio, d1 = casella, d0 = colore):
| se la casella e' Abubo e il giocatore tiene SU, d1 = $0F (Johann). Area libera (zeri) a $7F100.
        .org 0
jo:
        cmpi.w  #0x0e,%d1
        bne.s   go
        move.b  0x10fd96,%d2            | P1: ingressi correnti BIOS (1 = premuto)
        btst    #0,0x5d(%a6)            | giocatore = bit 0 di $5c(a6)
        beq.s   chk
        move.b  0x10fd9c,%d2            | P2
chk:
        btst    #0,%d2                  | SU
        beq.s   go
        moveq   #0x0f,%d1               | Johann
go:
        jsr     0x6a9e2
        jmp     0x6b916
