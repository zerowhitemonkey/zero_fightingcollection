# Daraku: ombre di entrambi i giocatori a ogni frame.
# Copia di 0x9C20-0x9CFF a 0xFF000 (stesso allineamento: i letterali PC-relativi restano validi), modificata:
#  - 0x9C30 extu.w r13,r13 -> neg r13,r13 e base 0x0602DE28 -> 0x0602E028: giocatore = parita' ^ 1
#  - dopo il disegno dell'ombra (0x9CAA): r4 = oggetto, epilogo, salto (coda) alla funzione originale 0x9C20
#  - letterale della callback a 0x9C14: 0x9C20 -> 0xFF000
import sys
be = open(sys.argv[1], 'rb').read()
SRC, DST, LEN = 0x9c20, 0xff000, 0xe0
orig = bytearray(be[DST:DST + LEN]); assert all(b == 0xff for b in orig), 'zona non libera'
new = bytearray(be[SRC:SRC + LEN])
def w16(a, v, expect=None):
    o = a - SRC
    if expect is not None: assert int.from_bytes(new[o:o+2], 'big') == expect, hex(a)
    new[o:o+2] = v.to_bytes(2, 'big')
w16(0x9c30, 0x6ddb, 0x6ddd)                       # neg r13,r13
w16(0x9ce8, 0x0602, 0x0602); w16(0x9cea, 0xe028, 0xde28)   # base + 0x200
tail = [0x64f2, 0x7f04, 0x4f26, 0x6df6, 0x6ef6, 0xd301, 0x432b, 0x0009, 0x0009, 0x0000, 0x9c20]
#        r4=@r15 add#4  lds pr  pop r13 pop r14 mov.l lit jmp@r3  nop    pad    literal 0x00009C20 a 0x9CBC
for i, v in enumerate(tail): w16(0x9caa + 2 * i, v)
patches = []
for i in range(0, LEN, 2):
    patches.append((DST + i, int.from_bytes(orig[i:i+2], 'big'), int.from_bytes(new[i:i+2], 'big')))
patches.append((0x9c14, 0x0000, 0x0000)); patches.append((0x9c16, 0x9c20, 0xf000))
patches[-2] = (0x9c14, int.from_bytes(be[0x9c14:0x9c16], 'big'), 0x000f)
if len(sys.argv) > 2 and sys.argv[2] == 'c':
    for a, o, v in patches: print('    { 0x%06x, 0x%04x, 0x%04x },' % (a, o, v))
else:
    for a, o, v in patches: print('p16 0 0x%x 0x%x 0x%x' % (a, o, v))
# verifica: disassemblato della copia
img = bytearray(be); img[DST:DST+LEN] = new
open('/tmp/prog_patched.bin', 'wb').write(bytes(img))
