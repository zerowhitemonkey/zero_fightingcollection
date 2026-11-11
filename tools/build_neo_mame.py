#!/usr/bin/env python3
"""Costruisce un .neo da un romset MAME (zip) con eventuali patch.
uso: build_neo_mame.py <zip> <out.neo> <ngh_hex> <anno> <titolo> [patch.json]
P = p1 (+ p2/sp2) formato MAME; C = coppie c1/c2 interleaved a byte; M = m1; V = v1..vN."""
import sys, zipfile, re, struct, json, zlib
import numpy as np

zp, out, ngh, year, title = sys.argv[1:6]
patchfile = sys.argv[6] if len(sys.argv) > 6 else None
z = zipfile.ZipFile(zp)
names = z.namelist()
def pick(rx): return sorted([n for n in names if re.search(rx, n, re.I)])
def rd(n): return z.read(n)

pfiles = sorted(pick(r'-s?p\d\.(s?p\d|bin)$'), key=lambda n: re.search(r'-s?p(\d)\.', n).group(1)) or pick(r'-p\d')
p = b''.join(rd(n) for n in pfiles)                    # formato MAME (word-swapped)
if len(pfiles) == 2:
    # MAME carica p2/sp2 a $100000 (zona a banchi): p1 riempita a 1 MB e p2 a multipli di 1 MB (specchiate)
    def mirror(b, size): return (b * (size // len(b) + 1))[:size]
    p1, p2 = rd(pfiles[0]), rd(pfiles[1])
    if len(p1) < 0x100000: p1 = mirror(p1, 0x100000)
    if len(p2) % 0x100000: p2 = mirror(p2, (len(p2) + 0xfffff) // 0x100000 * 0x100000)
    p = p1 + p2
s = rd(pick(r'-s1\.')[0])
# P di 2 MB caricata da MAME con le meta' invertite (es. savagere: offset $100000 + ROM_CONTINUE a 0)
if len(p) == 0x200000 and p[0x100:0x106] != b'EN-OEG' and p[0x100100:0x100106] == b'EN-OEG':
    p = p[0x100000:] + p[:0x100000]
m = rd(pick(r'-m1\.')[0])
v = b''.join(rd(n) for n in pick(r'-v\d+\.'))
cf = pick(r'-c\d+\.')
c = bytearray()
pairs = []
for i in range(0, len(cf), 2):
    a = np.frombuffer(rd(cf[i]), dtype=np.uint8); b = np.frombuffer(rd(cf[i + 1]), dtype=np.uint8)
    pair = np.empty(a.size * 2, dtype=np.uint8); pair[0::2] = a; pair[1::2] = b
    pairs.append(pair.tobytes())
import os
if os.path.basename(zp).lower() in ('aof.zip',):
    # C caricate da MAME come meta' + ROM_CONTINUE: prima meta' di ogni coppia, poi le seconde meta'
    c = bytearray(b''.join(x[:len(x) // 2] for x in pairs) + b''.join(x[len(x) // 2:] for x in pairs))
elif os.path.basename(zp).lower() in ('kof95.zip',):
    # MAME: c1/c2 a 0, c3/c4 a $800000, c5/c6 (2 MB) a $1000000, c7/c8 (1 MB) a $1800000: ogni coppia a multipli di 8 MB
    step = max(len(x) for x in pairs)
    c = bytearray(step * (len(pairs) - 1) + len(pairs[-1]))
    for i, x in enumerate(pairs): c[i * step:i * step + len(x)] = x
else:
    c = bytearray(b''.join(pairs))

if patchfile:
    pb = bytearray(p[0::1])
    # le patch sono indirizzi 68000 (big-endian): lavora sulla copia big-endian
    be = bytearray(len(pb)); be[0::2] = pb[1::2]; be[1::2] = pb[0::2]
    for e in json.load(open(patchfile)):
        a = int(e['addr'], 16); o = bytes.fromhex(e['orig']); n = bytes.fromhex(e['new'])
        assert be[a:a+len(o)] == o, (e['addr'], be[a:a+len(o)].hex())
        be[a:a+len(n)] = n
    pb[0::2] = be[1::2]; pb[1::2] = be[0::2]
    p = bytes(pb)

hdr = bytearray(4096)
hdr[0:4] = b'NEO\x01'
struct.pack_into('<6I', hdr, 4, len(p), len(s), len(m), len(v), 0, len(c))
struct.pack_into('<4I', hdr, 28, int(year), 1, 0, int(ngh, 16))
hdr[44:77] = title.encode()[:33].ljust(33, b'\0')
hdr[77:94] = b'SNK'.ljust(17, b'\0')
with open(out, 'wb') as f:
    for x in (hdr, p, s, m, v, c): f.write(x)
print(out, 'P=%x S=%x M=%x V=%x C=%x crcP=%08x' % (len(p), len(s), len(m), len(v), len(c), zlib.crc32(p)))
