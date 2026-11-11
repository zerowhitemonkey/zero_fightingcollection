# istruzioni mov.l @(disp,PC),Rn (0xDnxx) il cui letterale vale VAL
import sys
be = open(sys.argv[1], 'rb').read()
val = int(sys.argv[2], 16)
for off in range(0, len(be), 2):
    op = int.from_bytes(be[off:off+2], 'big')
    if op >> 12 != 0xD: continue
    addr = off if off < 0x100000 else 0x02000000 + off - 0x100000
    ea = (addr & ~3) + 4 + (op & 0xff) * 4
    o = ea if ea < 0x100000 else ea - 0x02000000 + 0x100000
    if 0 <= o < len(be) - 3 and int.from_bytes(be[o:o+4], 'big') == val:
        print('%08x: mov.l @%08x,r%d' % (addr, ea, (op >> 8) & 15))
