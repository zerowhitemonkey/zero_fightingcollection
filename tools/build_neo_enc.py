#!/usr/bin/env python3
"""Costruisce un .neo da un romset MAME CIFRATO (decrittazione come MAME 0.264, vedi neocrypt.py).
uso: build_neo_enc.py <gioco> <zip> <out.neo> [patch.json]
giochi: garou, kof2000, kof2002, kof98, kof99, kof2001, kof2003, rotd"""
import sys, os, zipfile, struct, json, zlib, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import neocrypt as nc

game, zp, out = sys.argv[1:4]
patchfile = sys.argv[4] if len(sys.argv) > 4 else None
z = zipfile.ZipFile(zp)
names = {n.lower(): n for n in z.namelist()}
def rd(*cands):
    for c in cands:
        if c.lower() in names: return z.read(names[c.lower()])
    raise SystemExit('manca ' + '/'.join(cands))

def cpairs(fmt, n):
    c = bytearray()
    for i in range(1, n + 1, 2):
        a = np.frombuffer(rd(*[(f % i) % i if '%' in f % i else f % i for f in fmt]), np.uint8)
        b = np.frombuffer(rd(*[(f % (i + 1)) % (i + 1) if '%' in f % (i + 1) else f % (i + 1) for f in fmt]), np.uint8)
        pr = np.empty(a.size * 2, np.uint8); pr[0::2] = a; pr[1::2] = b; c += pr.tobytes()
    return bytes(c)

GAMES = {
    # id: (ngh, anno, titolo)
    'garou': (0x253, 1999, 'Garou - Mark of the Wolves'),
    'kof2000': (0x257, 2000, 'The King of Fighters 2000'),
    'kof2002': (0x265, 2002, 'The King of Fighters 2002'),
    'kof98': (0x242, 1998, "The King of Fighters '98"),
    'kof99': (0x251, 1999, "The King of Fighters '99"),
    'kof2001': (0x262, 2001, 'The King of Fighters 2001'),
    'kof2003': (0x271, 2003, 'The King of Fighters 2003'),
    'rotd': (0x264, 2002, 'Rage of the Dragons'),
}
ngh, year, title = GAMES[game]
if game == 'garou':
    p = bytearray(0x900000); p[0xc0000:0x100000] = rd('253-sma.kf', 'kf.neo-sma')
    for i in range(4):
        p[0x100000 + i * 0x200000:0x300000 + i * 0x200000] = rd('253-ep%d.p%d' % (i + 1, i + 1))
    p = nc.sma_garou(p)
    c = nc.cmc_gfx(cpairs(('253-c%d.bin', '253-c%%d.c%d'), 8), 0x06, 'cmc42')
    s = nc.sfix(c, 0x80000)
    m = rd('253-m1.bin', '253-m1.m1')
    v = b''.join(rd('253-v%d.bin' % i, '253-v%d.v%d' % (i, i)) for i in range(1, 5))
if game == 'kof2000':
    # P: SMA ($C0000) + p1/p2 a $100000/$500000, decrittata come MAME (9 MB: Geolith attiva la SMA kof2000)
    p = bytearray(0x900000); p[0xc0000:0x100000] = rd('neo-sma', '257-sma.bin')
    p[0x100000:0x500000] = rd('257-p1.p1', '257-p1.bin'); p[0x500000:0x900000] = rd('257-p2.p2', '257-p2.bin')
    p = nc.sma_kof2000(p)
    c = nc.cmc_gfx(cpairs(('257-c%d.bin', '257-c%%d.c%d'), 8), 0x00, 'cmc50')
    s = nc.sfix(c, 0x80000)
    m0 = rd('257-m1.m1', '257-m1.bin')
    m = nc.cmc50_m1(m0)[:len(m0)]          # M1 decrittata (CMC50), stessa dimensione dell'originale
    v = b''.join(rd('257-v%d.v%d' % (i, i), '257-v%d.bin' % i) for i in range(1, 5))
if game == 'kof2002':
    # P: p1 + p2 con i banchi da 512 KB rimescolati (kof2002_decrypt_68k), C CMC50 chiave $EC,
    # S dalla fine di C (128 KB), M1 CMC50, V NEO-PCM2 (swap valore 0). Verificato identico ai dump MAME 0.264.
    p = nc.kof2002_68k(rd('265-p1.p1') + rd('265-p2.sp2'))
    c = nc.cmc_gfx(cpairs(('265-c%%d.c%d',), 8), 0xec, 'cmc50')
    s = nc.sfix(c, 0x20000)
    m0 = rd('265-m1.m1')
    m = nc.cmc50_m1(m0)[:len(m0)]
    v = nc.pcm2_swap(rd('265-v1.v1') + rd('265-v2.v2'), 0)

if game == 'kof98':
    # P: p1 cifrata (overlay kof98 gestito da Geolith BOARD_KOF98), decrittata come MAME; resto in chiaro
    p = nc.kof98_68k(rd('242-p1.p1') + rd('242-p2.sp2'))
    c = cpairs(('242-c%%d.c%d',), 8)
    s = rd('242-s1.s1'); m = rd('242-m1.m1')
    v = b''.join(rd('242-v%d.v%d' % (i, i)) for i in range(1, 5))
if game == 'kof99':
    # P: SMA ka ($C0000) + p1/p2 a $100000/$500000 (9 MB: Geolith attiva la SMA kof99), C CMC42 chiave $00, S 128 KB
    p = bytearray(0x900000); p[0xc0000:0x100000] = rd('ka.neo-sma')
    p[0x100000:0x500000] = rd('251-p1.p1'); p[0x500000:0x900000] = rd('251-p2.p2')
    p = nc.sma_kof99(p)
    c = nc.cmc_gfx(cpairs(('251-c%%d.c%d',), 8), 0x00, 'cmc42')
    s = nc.sfix(c, 0x20000)
    m = rd('251-m1.m1')
    v = b''.join(rd('251-v%d.v%d' % (i, i)) for i in range(1, 5))
if game == 'kof2001':
    # P in chiaro (p1 + sp2), C CMC50 chiave $1E, S 128 KB, M1 CMC50, V in chiaro
    p = rd('262-p1-08-e0.p1') + rd('262-p2-08-e0.sp2')
    c = nc.cmc_gfx(cpairs(('262-c%%d-08-e0.c%d',), 8), 0x1e, 'cmc50')
    s = nc.sfix(c, 0x20000)
    m0 = rd('265-262-m1.m1')
    m = nc.cmc50_m1(m0)[:len(m0)]
    v = b''.join(rd('262-v%d-08-e0.v%d' % (i, i)) for i in range(1, 5))
if game == 'kof2003':
    # P: p1/p2 interleaved a 32 bit + p3, kof2003_decrypt_68k (9 MB, Geolith BOARD_PVC), C CMC50 chiave $9D,
    # S 512 KB, M1 CMC50 (512 KB), V NEO-PCM2 swap valore 5
    p = nc.kof2003_68k(rd('271-p1c.p1'), rd('271-p2c.p2'), rd('271-p3c.p3'))
    c = nc.cmc_gfx(cpairs(('271-c%%dc.c%d',), 8), 0x9d, 'cmc50')
    s = nc.sfix(c, 0x80000)
    m0 = rd('271-m1c.m1')
    m = nc.cmc50_m1(m0)[:len(m0)]
    v = nc.pcm2_swap(rd('271-v1c.v1') + rd('271-v2c.v2'), 5)
if game == 'rotd':
    # P in chiaro (8 MB, banchi standard), C CMC50 chiave $3F, S 128 KB dalla fine di C, M1 CMC50 (128 KB),
    # V NEO-PCM2 SNK 1999 valore 16 (meta' dei blocchi da 16 byte scambiate). Nessun fix a banchi (S 128 KB)
    p = rd('264-p1.p1')
    c = nc.cmc_gfx(cpairs(('264-c%%d.c%d',), 8), 0x3f, 'cmc50')
    s = nc.sfix(c, 0x20000)
    m0 = rd('264-m1.m1')
    m = nc.cmc50_m1(m0)[:len(m0)]
    v = nc.pcm2_snk1999(rd('264-v1.v1') + rd('264-v2.v2'), 16)

if patchfile:
    be = bytearray(len(p)); be[0::2] = p[1::2]; be[1::2] = p[0::2]
    for e in json.load(open(patchfile)):
        a = int(e['addr'], 16); o = bytes.fromhex(e['orig']); n = bytes.fromhex(e['new'])
        assert be[a:a + len(o)] == o, (e['addr'], be[a:a + len(o)].hex())
        be[a:a + len(n)] = n
    pb = bytearray(len(p)); pb[0::2] = be[1::2]; pb[1::2] = be[0::2]; p = bytes(pb)

hdr = bytearray(4096); hdr[0:4] = b'NEO\x01'
struct.pack_into('<6I', hdr, 4, len(p), len(s), len(m), len(v), 0, len(c))
struct.pack_into('<4I', hdr, 28, year, 1, 0, ngh)
hdr[44:77] = title.encode()[:33].ljust(33, b'\0'); hdr[77:94] = b'SNK'.ljust(17, b'\0')
with open(out, 'wb') as f:
    for x in (hdr, p, s, m, v, c): f.write(x)
print(out, 'P=%x S=%x M=%x V=%x C=%x crcP=%08x' % (len(p), len(s), len(m), len(v), len(c), zlib.crc32(p)))
