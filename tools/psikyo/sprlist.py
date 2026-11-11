# elenco sprite da un dump (formato dump.lua MAME big-endian, oppure .vram del banco = little-endian con buffer)
import sys, struct
def load(fn):
    b = open(fn,'rb').read()
    if fn.endswith('.vram'):   # banco: sprram LE 0x10000, sprbuf LE 0x4000, vidregs 8 LE
        spr = list(struct.unpack('<16384I', b[:0x10000])); buf = list(struct.unpack('<4096I', b[0x10000:0x14000]))
        vr = list(struct.unpack('<8I', b[0x14000:0x14020])); return spr, buf, None, vr
    spr = list(struct.unpack('>16384I', b[:0x10000])); pal = list(struct.unpack('>5120I', b[0x10000:0x15000]))
    zoom = list(struct.unpack('>256H', b[0x15000:0x15200])); vr = list(struct.unpack('>8I', b[0x15200:0x15220]))
    return spr, spr[:0x1000], (pal, zoom), vr
prev = load(sys.argv[1]); spr = prev[1]; vr = load(sys.argv[2])[3] if len(sys.argv) > 2 else prev[3]
extra = load(sys.argv[2])[2] if len(sys.argv) > 2 else prev[2]
listw = []
for i in range(0x3800//4, 0x4000//4):
    v = spr[i]; listw += [v >> 16, v & 0xffff]
atab = [(vr[0] >> 24) & 0xff, (vr[0] >> 16) & 0xff, (vr[0] >> 8) & 0xff, vr[0] & 0xff, (vr[1] >> 24) & 0xff, (vr[1] >> 16) & 0xff, (vr[1] >> 8) & 0xff, vr[1] & 0xff]
print('vidregs', ' '.join('%08x' % x for x in vr), ' alpha tab', ' '.join('%02x' % a for a in atab))
for i, ld in enumerate(listw):
    n = (ld & 0x3ff) * 4
    w0, w1, w2 = spr[n], spr[n+1], spr[n+2]
    y = (w0 >> 16) & 0x3ff; x = w0 & 0x3ff
    y -= 0x400 if y & 0x200 else 0; x -= 0x400 if x & 0x200 else 0
    pri = (w1 >> 12) & 3; bgpri = ((vr[2] << (pri * 4)) >> 28) & 0xf
    zy = (w1 >> 16) & 0xff; zx = w1 & 0xff
    zt = extra[1] if extra else None
    a = (w2 >> 20) & 7
    print('%3d n%03x x%4d y%4d %dx%d z(%02x,%02x)%s fl%d%d bpri%d spri%d d%d col%02x tile%05x a%d(%02x)%s' % (
        i, n // 4, x, y, ((w1 >> 8) & 15) + 1, ((w1 >> 24) & 15) + 1, zx, zy,
        ('=%03x,%03x' % (zt[zx], zt[zy])) if zt else '', (w1 >> 15) & 1, w1 >> 31, bgpri, (w1 >> 28) & 3,
        (w2 >> 23) & 1, w2 >> 24, w2 & 0x7ffff, a, atab[a], ' END' if ld & 0x4000 else '', ))
    if ld & 0x4000: break
