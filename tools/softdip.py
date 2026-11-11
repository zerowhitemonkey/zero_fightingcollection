import sys
def dump(p, region=1):  # 0=JP,1=US,2=EU
    ptr = int.from_bytes(p[0x116 + 4*region:0x11a + 4*region], 'big')
    t = p[ptr:]
    name = t[0:16].decode('latin1')
    time1, time2 = t[16:18].hex(), t[18:20].hex()
    c1, c2 = t[20], t[21]
    lists = t[22:32]
    pos = 32
    out = [f'  name={name!r} time1={time1} time2={time2} count1={c1:02x} count2={c2:02x}']
    def s():
        nonlocal pos
        r = t[pos:pos+12].decode('latin1'); pos += 12; return r
    if time1 != 'ffff': out.append('  T1 ' + s())
    if time2 != 'ffff': out.append('  T2 ' + s())
    if c1 != 0xff: out.append('  C1 ' + s())
    if c2 != 0xff: out.append('  C2 ' + s())
    for i, b in enumerate(lists):
        n = b & 0x0f; d = b >> 4
        if n == 0: break
        desc = s(); ch = [s() for _ in range(n)]
        out.append(f'  L{i} ${0x10fd8a+i:06x} def={d} {desc!r}: ' + ' | '.join(ch))
    return '\n'.join(out)
import struct
for f in sys.argv[1:]:
    d = open(f, 'rb').read()
    hdr = d[:4096]; psz = struct.unpack_from('<I', d, 4)[0]
    p = bytearray(d[4096:4096+psz]); p[0::2], p[1::2] = p[1::2], p[0::2]
    print(f, 'NGH', p[0x108:0x10a].hex())
    for r, rn in ((0,'JP'),(1,'US')):
        print(' ', rn); print(dump(p, r))
