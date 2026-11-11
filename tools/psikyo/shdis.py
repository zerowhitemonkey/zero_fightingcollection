import sys, capstone
be = open(sys.argv[1],'rb').read()
a0 = int(sys.argv[2],16); n = int(sys.argv[3]) if len(sys.argv)>3 else 40
md = capstone.Cs(capstone.CS_ARCH_SH, capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_SH2)
md.skipdata = True
base = 0 if a0 < 0x02000000 else 0x02000000
off = a0 - base + (0 if base == 0 else 0x100000)
for i in md.disasm(be[off:off+n*2], a0):
    lit = ''
    if i.mnemonic.startswith('mov.l') and '@(' in i.op_str and 'pc' in i.op_str:
        d = int(i.op_str.split('@(')[1].split(',')[0], 0)
        ea = ((i.address & ~3) + 4 + d)
        o = ea - base + (0 if base == 0 else 0x100000)
        lit = '   ; =%08x' % int.from_bytes(be[o:o+4],'big')
    print('%08x: %04x  %-8s %s%s' % (i.address, int.from_bytes(i.bytes,'big'), i.mnemonic, i.op_str, lit))
