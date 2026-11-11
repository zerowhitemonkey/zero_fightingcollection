# punti in cui un valore 0xFE (letterale mov.w o mov #-2) viene scritto con mov.b Rm,@(R0,Rn) dopo mov #45,r0
import sys
be = open(sys.argv[1], 'rb').read()[:0x100000]
def op(a): return int.from_bytes(be[a:a+2], 'big')
for a in range(0, len(be) - 2, 2):
    o = op(a)
    reg = None
    if o >> 12 == 0x9:                       # mov.w @(disp,PC),Rn
        ea = a + 4 + (o & 0xff) * 2
        if ea + 2 <= len(be) and op(ea) == 0x00fe: reg = (o >> 8) & 15
    elif o >> 12 == 0xE and (o & 0xff) == 0xfe:  # mov #-2,Rn
        reg = (o >> 8) & 15
    if reg is None: continue
    for b in range(a + 2, a + 16, 2):
        if op(b) == 0xe02d:   # mov #45,r0
            for c in range(b + 2, b + 8, 2):
                x = op(c)
                if x >> 12 == 0 and (x & 0xf) == 4 and ((x >> 4) & 15) == reg:
                    print('%06x: valore FE in r%d -> mov.b @(r0,r%d) a %06x' % (a, reg, (x >> 8) & 15, c))
            break
