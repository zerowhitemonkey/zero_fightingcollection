#!/usr/bin/env python3
"""Genera src/neodb.h: per ogni gioco Neo Geo i file del romset MAME (CRC32 + dimensione) nell'ordine in cui
build_neo_mame.py / build_neo_enc.py li usano, piu' le patch anti-flicker (patches/<id>_noflicker.json).
uso: gen_neodb.py <cartella zip MAME> <cartella .neo di riferimento (header: ngh/anno/titolo)> > src/neodb.h"""
import sys, os, re, zipfile, struct, json
ZD, ND = sys.argv[1], sys.argv[2]
HERE = os.path.dirname(os.path.abspath(__file__))
PD = os.path.join(HERE, '..', '..', 'patches')
PLAIN = ['aof', 'aof2', 'aof3', 'savagere', 'fatfury1', 'fatfursp', 'fatfury3', 'rbff1', 'rbffspec', 'rbff2',
         'lastblad', 'kof94', 'kof95', 'kof96', 'kof97', 'wakuwak7', 'galaxyfg']
HDR = {'kof95': (0x084, 1995, "The King of Fighters '95"), 'kof96': (0x214, 1996, "The King of Fighters '96")}
# giochi cifrati: (P, crittografia P, chip CMC, chiave, sfix, M1 cmc50, V crittografia, parametro V)
ENC = {
 'garou':   (['253-sma.kf|kf.neo-sma'] + ['253-ep%d.p%d' % (i, i) for i in range(1, 5)], 'SMA_GAROU', 42, 0x06, 0x80000, 0, 'NONE', 0,
             ['253-c%d.bin|253-c%d.c%d' % (i, i, i) for i in range(1, 9)], '253-m1.bin|253-m1.m1', ['253-v%d.bin|253-v%d.v%d' % (i, i, i) for i in range(1, 5)], None),
 'kof2000': (['neo-sma|257-sma.bin', '257-p1.p1|257-p1.bin', '257-p2.p2|257-p2.bin'], 'SMA_KOF2000', 50, 0x00, 0x80000, 1, 'NONE', 0,
             ['257-c%d.bin|257-c%d.c%d' % (i, i, i) for i in range(1, 9)], '257-m1.m1|257-m1.bin', ['257-v%d.v%d|257-v%d.bin' % (i, i, i) for i in range(1, 5)], None),
 'kof2002': (['265-p1.p1', '265-p2.sp2'], 'KOF2002', 50, 0xec, 0x20000, 1, 'PCM2SWAP', 0,
             ['265-c%d.c%d' % (i, i) for i in range(1, 9)], '265-m1.m1', ['265-v1.v1', '265-v2.v2'], None),
 'kof98':   (['242-p1.p1', '242-p2.sp2'], 'KOF98', 0, 0, 0, 0, 'NONE', 0,
             ['242-c%d.c%d' % (i, i) for i in range(1, 9)], '242-m1.m1', ['242-v%d.v%d' % (i, i) for i in range(1, 5)], '242-s1.s1'),
 'kof99':   (['ka.neo-sma', '251-p1.p1', '251-p2.p2'], 'SMA_KOF99', 42, 0x00, 0x20000, 0, 'NONE', 0,
             ['251-c%d.c%d' % (i, i) for i in range(1, 9)], '251-m1.m1', ['251-v%d.v%d' % (i, i) for i in range(1, 5)], None),
 'kof2001': (['262-p1-08-e0.p1', '262-p2-08-e0.sp2'], 'NONE', 50, 0x1e, 0x20000, 1, 'NONE', 0,
             ['262-c%d-08-e0.c%d' % (i, i) for i in range(1, 9)], '265-262-m1.m1', ['262-v%d-08-e0.v%d' % (i, i) for i in range(1, 5)], None),
 'kof2003': (['271-p1c.p1', '271-p2c.p2', '271-p3c.p3'], 'KOF2003', 50, 0x9d, 0x80000, 1, 'PCM2SWAP', 5,
             ['271-c%dc.c%d' % (i, i) for i in range(1, 9)], '271-m1c.m1', ['271-v1c.v1', '271-v2c.v2'], None),
 'rotd':    (['264-p1.p1'], 'NONE', 50, 0x3f, 0x20000, 1, 'SNK1999', 16,
             ['264-c%d.c%d' % (i, i) for i in range(1, 9)], '264-m1.m1', ['264-v1.v1', '264-v2.v2'], None),
}
# giochi della collection Digital Eclipse / GOG: ricette esplicite sui set MAME, patch = differenze tra la P MAME
# e la P dei .neo dell'app (tutte le modifiche del progetto), C riempita fino alla dimensione del .neo dell'app
SEC = {'SAMSH5SP': (0x000000,0x080000,0x500000,0x480000,0x600000,0x580000,0x700000,0x280000,0x100000,0x680000,0x400000,0x780000,0x200000,0x380000,0x300000,0x180000),
       'SAMSHO5': (0x000000,0x080000,0x700000,0x680000,0x500000,0x180000,0x200000,0x480000,0x300000,0x780000,0x600000,0x280000,0x100000,0x580000,0x400000,0x380000)}
DIFF = {
 # id: (P, crittografia P, cmc, chiave, sfix, m1 cmc50, V crittografia, parametro, C, M, V, S, flag)
 'samsho':   (['045-p1.p1', '045-pg2.sp2'], 'NONE', 0, 0, 0, 0, 'NONE', 0, ['045-c1.c1','045-c2.c2','045-c3.c3','045-c4.c4','045-c51.c5','045-c61.c6'], '045-m1.m1', ['045-v1.v1','045-v2.v2'], '045-s1.s1', '0'),
 'samsho2':  (['063-p1.p1'], 'NONE', 0, 0, 0, 0, 'NONE', 0, ['063-c%d.c%d' % (i, i) for i in range(1, 9)], '063-m1.m1', ['063-v%d.v%d' % (i, i) for i in range(1, 5)], '063-s1.s1', '0'),
 'samsho3':  (['087-epr.ep1','087-epr.ep2','087-epr.ep3','087-epr.ep4','087-p5.p5'], 'NONE', 0, 0, 0, 0, 'NONE', 0, ['087-c%d.c%d' % (i, i) for i in range(1, 9)], '087-m1.m1', ['087-v1.v1','087-v2.v2'], '087-s1.s1', '0'),
 'samsho4':  (['222-p1.p1','222-p2.sp2'], 'NONE', 0, 0, 0, 0, 'NONE', 0, ['222-c%d.c%d' % (i, i) for i in range(1, 9)], '222-m1.m1', ['222-v1.v1','222-v2.v2','222-v3.v3'], '222-s1.s1', '0'),
 'samsh5sp': (['272-p1.bin|272-p1.p1','272-p2.bin|272-p2.sp2'], 'SAMSH5SP', 50, 0x0d, 0x20000, 1, 'PCM2SWAP', 6, ['272-c%d.bin|272-c%d.c%d' % (i, i, i) for i in range(1, 9)], '272-m1.bin|272-m1.m1', ['272-v1.bin|272-v1.v1','272-v2.bin|272-v2.v2'], None, '0'),
 'kizuna':   (['216-p1.bin|216-p1.p1'], 'NONE', 0, 0, 0, 0, 'NONE', 0, ['059-c1.bin|059-c1.c1','059-c2.bin|059-c2.c2','216-c3.bin|216-c3.c3','216-c4.bin|216-c4.c4','059-c5.bin|059-c5.c5','059-c6.bin|059-c6.c6','059-c7.bin|059-c7.c7','059-c8.bin|059-c8.c8'],
              '216-m1.bin|216-m1.m1', ['059-v1.bin|059-v1.v1','216-v2.bin|216-v2.v2','059-v3.bin|059-v3.v3','216-v4.bin|216-v4.v4'], '216-s1.bin|216-s1.s1', 'NF_STEP8M'),
 'lastbld2': (['243-pg1.p1','243-pg2.sp2'], 'NONE', 0, 0, 0, 0, 'NONE', 0, ['243-c%d.c%d' % (i, i) for i in range(1, 7)], '243-m1.m1', ['243-v%d.v%d' % (i, i) for i in range(1, 5)], '243-s1.s1', '0'),
 'samsho5':  (['p1.bin|270-p1c.p1','p2.bin|270-p2c.sp2'], 'SAMSHO5', 50, 0x0f, 0x20000, 1, 'PCM2SWAP', 4, ['270-c%d.bin|270-c%d.c%d' % (i, i, i) for i in range(1, 9)], '270-m1.bin|270-m1.m1', ['270-v1.bin|270-v1.v1','270-v2.bin|270-v2.v2'], None, '0'),
 'ninjamas': (['217-p1.bin|217-p1.p1','217-p2.bin|217-p2.sp2'], 'NONE', 0, 0, 0, 0, 'NONE', 0, ['217-c%d.bin|217-c%d.c%d' % (i, i, i) for i in range(1, 9)], '217-m1.bin|217-m1.m1', ['217-v1.bin|217-v1.v1','217-v2.bin|217-v2.v2'], '217-s1.bin|217-s1.s1', '0'),
}
EXTRA = {'samsho5': ['samsho5a']}   # la collection usa la revisione samsho5a (P diversa dal set padre)
def neocomps(g):
    b = open(os.path.join(ND, g + '.neo'), 'rb').read(); sz = struct.unpack_from('<6I', b, 4); o = 4096; r = {}
    for n, x in zip('PSMVvC', sz): r[n] = b[o:o + x]; o += x
    return r
def diffpatch(a, b):
    """differenze tra P assemblata (MAME) e P del .neo dell'app, in indirizzi 68000 (big-endian)"""
    be = lambda x: bytes(bytearray(x[1::2]) + b'') if False else None
    A = bytearray(len(a)); A[0::2] = a[1::2]; A[1::2] = a[0::2]
    B = bytearray(len(b)); B[0::2] = b[1::2]; B[1::2] = b[0::2]
    out = []; i = 0; n = len(A)
    while i < n:
        if A[i] != B[i]:
            j = i
            while j < n and any(A[k] != B[k] for k in range(j, min(n, j + 4))): j += 1
            out.append('%x:%s:%s' % (i, A[i:j].hex(), B[i:j].hex())); i = j
        else: i += 1
    return ';'.join(out)
def hdr(g):
    if g in HDR: return HDR[g]
    h = open(os.path.join(ND, g + '.neo'), 'rb').read(4096)
    y, _, _, n = struct.unpack_from('<4I', h, 28)
    return n, y, h[44:77].rstrip(b'\0').decode()
out = []
def emit(g, pc, cc, key, sfx, m1c, vc, vp, flags, P, S, M, V, C, pt=None, csize=0):
    n, y, t = hdr(g)
    pj = os.path.join(PD, g + '_noflicker.json')
    if pt is None: pt = ';'.join('%s:%s:%s' % (e['addr'], e['orig'], e['new']) for e in json.load(open(pj))) if os.path.exists(pj) else ''
    f = lambda L: '{' + ','.join('{0x%08x,0x%x,%d}' % x for x in L) + '}'
    out.append('  {"%s",0x%03x,%d,"%s",NP_%s,%d,0x%02x,0x%x,%d,NV_%s,%d,%s,%d,%s,%s,%s,%d,%s,%d,%s,"%s",' % (
        g, n, y, t.replace('"', '\\"'), pc, cc, key, sfx, m1c, vc, vp, flags, len(P), f(P), f([S] if S else [(0, 0, 0)]), f([M]),
        len(V), f(V), len(C), f(C), pt) + ('0x%x,"%s"},' % (csize, ' + '.join([g] + EXTRA.get(g, [])))))
for g in PLAIN + list(ENC) + list(DIFF):
    zp = os.path.join(ZD, g + '.zip')
    if not os.path.exists(zp): sys.stderr.write('manca ' + zp + '\n'); continue
    # set "split": i file del clone (samsho5a = solo P) si cercano anche negli zip aggiuntivi
    zl = [zipfile.ZipFile(zp)] + [zipfile.ZipFile(os.path.join(ZD, x + '.zip')) for x in EXTRA.get(g, [])]
    z = zl[0]; info = {}; src = {}
    for si in reversed(range(len(zl))):
        zz = zl[si]
        for i in zz.infolist(): info[i.filename.lower()] = (i.CRC, i.file_size, si); src[i.filename.lower()] = (zz, i.filename)
    names = [i.filename for i in z.infolist()]
    def get(c):
        for x in c.split('|') + [re.sub(r'\.\w+$', '.bin', y) for y in c.split('|')]:
            if x.lower() in info: return info[x.lower()]
        raise SystemExit('%s: manca %s' % (g, c))
    if g in PLAIN:
        pick = lambda rx: sorted([n for n in names if re.search(rx, n, re.I)])
        pf = sorted(pick(r'-s?p\d\.(s?p\d|bin)$'), key=lambda n: re.search(r'-s?p(\d)\.', n).group(1)) or pick(r'-p\d')
        flags = 'NF_AOFC' if g == 'aof' else 'NF_STEP8M' if g == 'kof95' else '0'
        emit(g, 'NONE', 0, 0, 0, 0, 'NONE', 0, flags, [get(x) for x in pf], get(pick(r'-s1\.')[0]), get(pick(r'-m1\.')[0]),
             [get(x) for x in pick(r'-v\d+\.')], [get(x) for x in pick(r'-c\d+\.')])
    elif g in DIFF:
        P, pc, cc, key, sfx, m1c, vc, vp, C, M, V, S, flags = DIFF[g]
        def rdn(c):
            for x in c.split('|'):
                if x.lower() in src: zz, fn = src[x.lower()]; return zz.read(fn)
        pf = [rdn(x) for x in P]
        if len(pf) == 2 and pc == 'NONE':   # come build_neo_mame.py: p1 a 1 MB, p2 a multipli di 1 MB (specchiati)
            mir = lambda b, size: (b * (size // len(b) + 1))[:size]
            p1, p2 = pf
            if len(p1) < 0x100000: p1 = mir(p1, 0x100000)
            if len(p2) % 0x100000: p2 = mir(p2, (len(p2) + 0xfffff) // 0x100000 * 0x100000)
            p = p1 + p2
        else: p = b''.join(pf)
        if pc == 'NONE' and len(p) == 0x200000 and p[0x100:0x106] != b'EN-OEG' and p[0x100100:0x100106] == b'EN-OEG':
            p = p[0x100000:] + p[:0x100000]
        if pc in SEC:
            q = bytearray(len(p))
            for i, o in enumerate(SEC[pc]): q[i * 0x80000:(i + 1) * 0x80000] = p[o:o + 0x80000]
            p = bytes(q)
        r = neocomps(g)
        assert len(p) == len(r['P']), (g, hex(len(p)), hex(len(r['P'])))
        emit(g, pc, cc, key, sfx, m1c, vc, vp, flags, [get(x) for x in P], get(S) if S else None, get(M), [get(x) for x in V], [get(x) for x in C],
             diffpatch(p, r['P']), len(r['C']))
    else:
        P, pc, cc, key, sfx, m1c, vc, vp, C, M, V, S = ENC[g]
        emit(g, pc, cc, key, sfx, m1c, vc, vp, '0', [get(x) for x in P], get(S) if S else None, get(M), [get(x) for x in V], [get(x) for x in C])
print('// generato da tools/dist/gen_neodb.py: file dei romset MAME (CRC32, dimensione) e patch anti-flicker')
print('static const neodb_t neodb[] = {')
print('\n'.join(out))
print('};')
