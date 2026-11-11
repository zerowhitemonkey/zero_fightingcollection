#!/usr/bin/env python3
# Weapon Lord: limita le parate della CPU. Genera il codice 65816 da mettere in ROM estesa (banco F0) e le patch ai punti originali.
# Uso: wl_guard_patch.py  -> stampa il blob esadecimale (usato in src/wlpatch.c)
import sys
THR = [25, 55, 90, 140]        # probabilita' di parata /256 per livello 0..3 (Adventurer..Warlord): 10% 21% 35% 55%
BASE = 0xF00000
code = bytearray(); labels = {}; fix = []
def emit(*b): code.extend(b)
def lab(n): labels[n] = len(code)
def br(op, n): emit(op, 0); fix.append((len(code) - 1, n))
# entrata: X = indice attaccante, Y = indice difensore (A/X/Y a 16 bit), parata melee candidata
emit(0xAD, 0x32, 0x36)                    # LDA $3632 (contatore frame)
emit(0x38)                                # SEC
emit(0xFF, 0xE0, 0xFF, 0x7F)              # SBC $7FFFE0,X   (ultimo frame di contatto)
emit(0xC9, 0x09, 0x00)                    # CMP #9
br(0xB0, 'roll')                          # BCS roll   (pausa lunga = nuovo attacco)
emit(0xBF, 0xE4, 0xFF, 0x7F)              # LDA $7FFFE4,X   (decisione precedente)
br(0xD0, 'keep')                          # BNE keep   (0 = mai decisa)
lab('roll')
emit(0xDA, 0x5A)                          # PHX PHY
emit(0x22, 0x2D, 0x64, 0xEA)              # JSL $EA642D (random)
emit(0x29, 0xFF, 0x00)                    # AND #$00FF
emit(0x8F, 0xE8, 0xFF, 0x7F)              # STA $7FFFE8
emit(0xAD, 0x20, 0x20)                    # LDA $2020 (livello di gioco)
emit(0x29, 0x03, 0x00)                    # AND #3
emit(0x0A, 0xAA)                          # ASL TAX
emit(0xAF, 0xE8, 0xFF, 0x7F)              # LDA $7FFFE8
thr_fix = len(code); emit(0xDF, 0, 0, 0xF0)  # CMP thr,X  (indirizzo da completare)
emit(0xA9, 0x02, 0x00)                    # LDA #2  (nessuna parata)
br(0xB0, 'dec')                           # BCS dec (rand >= soglia)
emit(0xA9, 0x01, 0x00)                    # LDA #1  (parata)
lab('dec')
emit(0x7A, 0xFA)                          # PLY PLX
emit(0x9F, 0xE4, 0xFF, 0x7F)              # STA $7FFFE4,X
lab('keep')
emit(0xAD, 0x32, 0x36)                    # LDA $3632
emit(0x9F, 0xE0, 0xFF, 0x7F)              # STA $7FFFE0,X
emit(0xBF, 0xE4, 0xFF, 0x7F)              # LDA $7FFFE4,X
emit(0xC9, 0x01, 0x00)                    # CMP #1
br(0xD0, 'no')                            # BNE no
emit(0xA9, 0x00, 0x00, 0x38, 0x6B)        # LDA #0 ; SEC ; RTL   (parata, come l'originale)
lab('no')
emit(0xA9, 0x00, 0x00, 0x18, 0x6B)        # LDA #0 ; CLC ; RTL   (nessuna parata)
lab('thr')
for t in THR: emit(t & 0xFF, 0)
for off, n in fix:
    d = labels[n] - (off + 1)
    assert -128 <= d <= 127; code[off] = d & 0xFF
t = BASE + labels['thr']; code[thr_fix + 1:thr_fix + 4] = bytes([t & 0xFF, (t >> 8) & 0xFF, (t >> 16) & 0xFF])
if __name__ == "__main__":
    print(len(code), code.hex())
