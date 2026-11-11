#!/usr/bin/env python3
"""Decrittazione dei romset Neo Geo protetti (port numpy delle routine MAME 0.264,
src/devices/bus/neogeo/prot_*.cpp, BSD-3-Clause).
Regioni come in MAME: P = parole little-endian (file MAME cosi' come sono, SMA a $C0000),
C = coppie c1/c2 interleaved a byte, S = ricavata dalla fine di C (sfix_decrypt)."""
import numpy as np
from cmc_tables import TABLES

T = {k: np.frombuffer(v, np.uint8).astype(np.int64) for k, v in TABLES.items()}


def bitswap(v, *bits):
    """bitswap<N>(v, bN-1 ... b0) di MAME, vettoriale"""
    v = np.asarray(v, dtype=np.int64)
    out = np.zeros_like(v)
    n = len(bits)
    for i, b in enumerate(bits):
        out |= ((v >> b) & 1) << (n - 1 - i)
    return out


# ------------------------------------------------------------------ SMA (68000)
def _sma(p, data_bits, fixed_src, fixed_bits, bank_len, bank_bits, bank_size=0x800000, fixed_first=False):
    rom = np.frombuffer(bytes(p), '<u2').astype(np.int64).copy()
    b = 0x100000 // 2
    rom[b:b + 0x800000 // 2] = bitswap(rom[b:b + 0x800000 // 2], *data_bits)

    def fixed():
        i = np.arange(0xc0000 // 2)
        rom[:0xc0000 // 2] = rom[fixed_src // 2 + bitswap(i, *fixed_bits)]

    def banked():
        n = bank_len // 2
        j = bitswap(np.arange(n), *bank_bits)
        seg = rom[b:b + bank_size // 2].reshape(-1, n)
        rom[b:b + bank_size // 2] = seg[:, j].reshape(-1)

    if fixed_first:
        fixed(); banked()
    else:
        banked(); fixed()
    return rom.astype('<u2').tobytes()


def sma_garou(p):
    return _sma(p, (13, 12, 14, 10, 8, 2, 3, 1, 5, 9, 11, 4, 15, 0, 6, 7),
                0x710000, (18, 4, 5, 16, 14, 7, 9, 6, 13, 17, 15, 3, 1, 2, 12, 11, 8, 10, 0),
                0x8000, (9, 4, 8, 3, 13, 6, 2, 7, 0, 12, 1, 11, 10, 5), fixed_first=True)


def sma_kof99(p):
    return _sma(p, (13, 7, 3, 0, 9, 4, 5, 6, 1, 12, 8, 14, 10, 11, 2, 15),
                0x700000, (18, 11, 6, 14, 17, 16, 5, 8, 10, 12, 0, 4, 3, 2, 7, 9, 15, 13, 1),
                0x800, (6, 2, 4, 9, 8, 3, 1, 7, 0, 5), bank_size=0x600000)


def sma_kof2000(p):
    return _sma(p, (12, 8, 11, 3, 15, 14, 7, 0, 10, 13, 6, 5, 9, 2, 1, 4),
                0x73a000, (18, 8, 4, 15, 13, 3, 14, 16, 2, 6, 17, 7, 12, 10, 0, 5, 11, 1, 9),
                0x800, (4, 1, 3, 8, 6, 2, 7, 0, 9, 5), bank_size=0x63a000)


# ------------------------------------------------------------------ CMC (grafica)
def cmc_gfx(c, extra_xor, chip='cmc42'):
    pre = 'kof99_' if chip == 'cmc42' else 'kof2000_'
    t0_03, t0_12 = T[pre + 'type0_t03'], T[pre + 'type0_t12']
    t1_03, t1_12 = T[pre + 'type1_t03'], T[pre + 'type1_t12']
    a815_1, a815_2 = T[pre + 'address_8_15_xor1'], T[pre + 'address_8_15_xor2']
    a1623_1, a1623_2 = T[pre + 'address_16_23_xor1'], T[pre + 'address_16_23_xor2']
    a07 = T[pre + 'address_0_7_xor']
    rom = np.frombuffer(bytes(c), np.uint8).reshape(-1, 4).astype(np.int64)
    n = rom.shape[0]
    buf = np.empty_like(rom)
    for s in range(0, n, 1 << 22):            # a blocchi per contenere la memoria
        r = np.arange(s, min(n, s + (1 << 22)), dtype=np.int64)
        blk = rom[s:s + len(r)]

        def dec(c0, c1, thi, tlo, t1, invert):
            tmp = t1[(r & 0xff) ^ a07[(r >> 8) & 0xff]]
            x0 = (thi[(r >> 8) & 0xff] & 0xfe) | (tmp & 1)
            x1 = (tmp & 0xfe) | (tlo[(r >> 8) & 0xff] & 1)
            r0 = np.where(invert, c1 ^ x0, c0 ^ x0)
            r1 = np.where(invert, c0 ^ x1, c1 ^ x1)
            return r0, r1

        b0, b3 = dec(blk[:, 0], blk[:, 3], t0_03, t0_12, t1_03, ((r >> 8) & 1) != 0)
        b1, b2 = dec(blk[:, 1], blk[:, 2], t0_12, t0_03, t1_12, (((r >> 16) ^ a1623_2[(r >> 8) & 0xff]) & 1) != 0)
        buf[s:s + len(r), 0], buf[s:s + len(r), 1], buf[s:s + len(r), 2], buf[s:s + len(r), 3] = b0, b1, b2, b3
    r = np.arange(n, dtype=np.int64)
    br = r ^ extra_xor
    br ^= a815_1[(br >> 16) & 0xff] << 8
    br ^= a815_2[br & 0xff] << 8
    br ^= a1623_1[br & 0xff] << 16
    br ^= a1623_2[(br >> 8) & 0xff] << 16
    br ^= a07[(br >> 8) & 0xff]
    br &= n - 1
    return buf[br].astype(np.uint8).tobytes()


def sfix(c, size=0x80000):
    src = np.frombuffer(bytes(c[-size:]), np.uint8)
    i = np.arange(size)
    return src[(i & ~0x1f) + ((i & 7) << 2) + ((~i & 8) >> 2) + ((i & 0x10) >> 4)].tobytes()


# ------------------------------------------------------------------ CMC50 (M1)
_P1 = [[15, 14, 10, 7, 1, 2, 3, 8, 0, 12, 11, 13, 6, 9, 5, 4],
       [7, 1, 8, 11, 15, 9, 2, 3, 5, 13, 4, 14, 10, 0, 6, 12],
       [8, 6, 14, 3, 10, 7, 15, 1, 4, 0, 2, 5, 13, 11, 12, 9],
       [2, 8, 15, 9, 3, 4, 11, 7, 13, 6, 0, 10, 1, 12, 14, 5],
       [1, 13, 6, 15, 14, 3, 8, 10, 9, 4, 7, 12, 5, 2, 0, 11],
       [11, 15, 3, 4, 7, 0, 9, 2, 6, 14, 12, 1, 8, 5, 10, 13],
       [10, 5, 13, 8, 6, 15, 1, 14, 11, 9, 3, 0, 12, 7, 4, 2],
       [9, 3, 7, 0, 2, 12, 4, 11, 14, 10, 5, 8, 15, 13, 1, 6]]


def cmc50_m1(m):
    """m: M1 cifrata (0x80000). Ritorna la M1 per l'audiocpu (0x90000 come MAME: 64K + 512K)"""
    rom = np.frombuffer(bytes(m[:0x80000]).ljust(0x80000, b'\xff'), np.uint8)
    key = int(rom[:0x10000].astype(np.int64).sum()) & 0xffff
    a = np.arange(0x80000, dtype=np.int64)
    block = (a >> 16) & 7
    aux = (a & 0xffff) ^ int(bitswap(key, 12, 0, 2, 4, 8, 15, 7, 13, 10, 1, 3, 6, 11, 9, 14, 5))
    out = np.zeros_like(aux)
    for blk in range(8):
        sel = block == blk
        p = _P1[blk]
        out[sel] = bitswap(aux[sel], *[p[15 - k] for k in range(16)])
    aux = out
    aux ^= T['m1_address_0_7_xor'][(aux >> 8) & 0xff]
    aux ^= T['m1_address_8_15_xor'][aux & 0xff] << 8
    aux = bitswap(aux, 7, 15, 14, 6, 5, 13, 12, 4, 11, 3, 10, 2, 9, 1, 8, 0)
    dec = rom[(block << 16) | aux]
    return dec.tobytes()


# ------------------------------------------------------------------ KOF 2002 (68000: banchi rimescolati della P2)
def kof2002_68k(p):
    sec = (0x100000, 0x280000, 0x300000, 0x180000, 0x000000, 0x380000, 0x200000, 0x080000)
    p = bytearray(p)
    src = bytes(p[0x100000:0x500000])
    for i, s in enumerate(sec):
        p[0x100000 + i * 0x80000:0x100000 + (i + 1) * 0x80000] = src[s:s + 0x80000]
    return bytes(p)


# ------------------------------------------------------------------ NEO-PCM2 (V ROM, prot_pcm2.cpp swap)
_PCM2_ADDRS = ((0x000000, 0xa5000), (0xffce20, 0x01000), (0xfe2cf6, 0x4e001), (0xffac28, 0xc2000),
               (0xfeb2c0, 0x0a000), (0xff14ea, 0xa7001), (0xffb440, 0x02000))
_PCM2_XOR = ((0xf9, 0xe0, 0x5d, 0xf3, 0xea, 0x92, 0xbe, 0xef), (0xc4, 0x83, 0xa8, 0x5f, 0x21, 0x27, 0x64, 0xaf),
             (0xc3, 0xfd, 0x81, 0xac, 0x6d, 0xe7, 0xbf, 0x9e), (0xc3, 0xfd, 0x81, 0xac, 0x6d, 0xe7, 0xbf, 0x9e),
             (0xcb, 0x29, 0x7d, 0x43, 0xd2, 0x3a, 0xc2, 0xb4), (0x4b, 0xa4, 0x63, 0x46, 0xf0, 0x91, 0xea, 0x62),
             (0x4b, 0xa4, 0x63, 0x46, 0xf0, 0x91, 0xea, 0x62))


def pcm2_swap(v, value):
    buf = np.frombuffer(bytes(v[:0x1000000]), np.uint8)
    i = np.arange(0x1000000, dtype=np.int64)
    j = bitswap(i, 23, 22, 21, 20, 19, 18, 17, 0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 16)
    j ^= _PCM2_ADDRS[value][1]
    d = (i + _PCM2_ADDRS[value][0]) & 0xffffff
    out = np.empty(0x1000000, np.uint8)
    out[j] = buf[d] ^ np.array(_PCM2_XOR[value], np.uint8)[j & 7]
    return out.tobytes()


# ------------------------------------------------------------------ KOF 98 (68000, prot_kof98.cpp decrypt_68k)
def kof98_68k(p):
    """p: regione MAME (p1 2 MB + p2 4 MB, parole little-endian). Ritorna 5 MB decrittati (1 MB fisso + p2)"""
    src = bytearray(p[:0x600000].ljust(0x600000, b'\xff'))
    dst = bytes(src[:0x200000])
    sec = (0x000000, 0x100000, 0x000004, 0x100004, 0x10000a, 0x00000a, 0x10000e, 0x00000e)
    pos = (0x000, 0x004, 0x00a, 0x00e)
    for i in range(0x800, 0x100000, 0x200):
        for j in range(0, 0x100, 0x10):
            b = i + j
            for k in range(0, 16, 2):
                s = sec[k // 2]
                src[b + k:b + k + 2] = dst[b + s + 0x100:b + s + 0x102]
                src[b + k + 0x100:b + k + 0x102] = dst[b + s:b + s + 2]
            if 0x080000 <= i < 0x0c0000:
                for q in pos:
                    src[b + q:b + q + 2] = dst[b + q:b + q + 2]
                    src[b + q + 0x100:b + q + 0x102] = dst[b + q + 0x100:b + q + 0x102]
            elif i >= 0x0c0000:
                for q in pos:
                    src[b + q:b + q + 2] = dst[b + q + 0x100:b + q + 0x102]
                    src[b + q + 0x100:b + q + 0x102] = dst[b + q:b + q + 2]
        src[i:i + 2] = dst[i:i + 2]
        src[i + 2:i + 4] = dst[i + 0x100000:i + 0x100002]
        src[i + 0x100:i + 0x102] = dst[i + 0x100:i + 0x102]
        src[i + 0x102:i + 0x104] = dst[i + 0x100100:i + 0x100102]
    src[0x100000:0x500000] = src[0x200000:0x600000]
    return bytes(src[:0x500000])


# ------------------------------------------------------------------ KOF 2003 (68000, prot_pvc.cpp kof2003_decrypt_68k)
def kof2003_68k(p1, p2, p3):
    rom = np.zeros(0x900000, np.uint8)
    a = np.frombuffer(p1, np.uint8).reshape(-1, 2); b = np.frombuffer(p2, np.uint8).reshape(-1, 2)
    r4 = rom[:0x800000].reshape(-1, 4)
    r4[:, 0:2] = a; r4[:, 2:4] = b                       # ROM_LOAD32_WORD_SWAP
    rom[0x800000:0x900000] = np.frombuffer(p3, np.uint8)
    xor1 = np.array([0x3b, 0x6a, 0xf7, 0xb7, 0xe8, 0xa9, 0x20, 0x99, 0x9f, 0x39, 0x34, 0x0c, 0xc3, 0x9a, 0xa5, 0xc8, 0xb8, 0x18, 0xce, 0x56, 0x94, 0x44, 0xe3, 0x7a, 0xf7, 0xdd, 0x42, 0xf0, 0x18, 0x60, 0x92, 0x9f], np.uint8)
    xor2 = np.array([0x2f, 0x02, 0x60, 0xbb, 0x77, 0x01, 0x30, 0x08, 0xd8, 0x01, 0xa0, 0xdf, 0x37, 0x0a, 0xf0, 0x65, 0x28, 0x03, 0xd0, 0x23, 0xd3, 0x03, 0x70, 0x42, 0xbb, 0x06, 0xf0, 0x28, 0xba, 0x0f, 0xf0, 0x7a], np.uint8)
    i = np.arange(0x100000)
    rom[0x800000 + i] ^= rom[0x100002 | i]
    rom[:0x100000] ^= xor1[i % 0x20]
    j = np.arange(0x100000, 0x800000)
    rom[0x100000:0x800000] ^= xor2[j % 0x20]
    w = rom[0x100000:0x800000].reshape(-1, 4)
    v = w[:, 1].astype(np.int64) | (w[:, 2].astype(np.int64) << 8)
    v = bitswap(v, 15, 14, 13, 12, 5, 4, 7, 6, 9, 8, 11, 10, 3, 2, 1, 0)
    w[:, 1] = v & 0xff; w[:, 2] = v >> 8
    buf = np.zeros(0x900000, np.uint8)
    for k in range(16):
        ofst = (k & 0xf0) + int(bitswap(k & 0x0f, 7, 6, 5, 4, 0, 1, 2, 3))
        buf[k * 0x10000:(k + 1) * 0x10000] = rom[ofst * 0x10000:(ofst + 1) * 0x10000]
    blk = np.arange(0x100000, 0x900000, 0x100)
    ofst = (blk & 0xf000ff) + ((blk & 0x000f00) ^ 0x00800) + (bitswap((blk & 0x0ff000) >> 12, 4, 5, 6, 7, 1, 0, 3, 2) << 12)
    buf[0x100000:0x900000] = rom.reshape(-1, 0x100)[ofst // 0x100].reshape(-1)
    out = np.concatenate([buf[:0x100000], buf[0x800000:0x900000], buf[0x100000:0x800000]])
    return out.tobytes()


# ------------------------------------------------------------------ NEO-PCM2 SNK 1999 (prot_pcm2.cpp decrypt: rotd 16, mslug4/matrim 8...)
def pcm2_snk1999(v, value):
    """scambia le due meta' di ogni blocco da `value` byte (rom[i+j] = buffer[j ^ value/4], parole)"""
    a = np.frombuffer(bytes(v), np.uint8).reshape(-1, 2, value // 2)
    return a[:, ::-1, :].tobytes()
