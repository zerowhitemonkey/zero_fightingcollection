// Romset MAME Neo Geo -> immagine .neo in memoria (vedi neorom.h).
// Decrittazione: port C delle routine di MAME 0.264 src/devices/bus/neogeo/prot_*.cpp (BSD-3-Clause), passando
// per tools/neocrypt.py (stessi risultati byte per byte, verificati con i .neo dell'app).
#include "neorom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "miniz.h"

enum { NP_NONE, NP_SMA_GAROU, NP_SMA_KOF99, NP_SMA_KOF2000, NP_KOF2002, NP_KOF98, NP_KOF2003, NP_SAMSHO5, NP_SAMSH5SP };
enum { NV_NONE, NV_PCM2SWAP, NV_SNK1999 };
enum { NF_AOFC = 1, NF_STEP8M = 2 };
typedef struct { uint32_t crc, size; int set; } nrf_t;   // set = indice del romset MAME in sets (0 = principale)
typedef struct {
    const char *id; int ngh, year; const char *title;
    int pcrypt, cmc, cmckey, sfix, m1cmc, vcrypt, vparam, flags;
    int np; nrf_t p[6]; nrf_t s[1]; nrf_t m[1];
    int nv; nrf_t v[8];
    int nc; nrf_t c[16];
    const char *patch;            // "addr:orig:new;..." indirizzi 68000 (big-endian)
    uint32_t csize;               // dimensione della C (0 = quella dei file; altrimenti riempita con zeri)
    const char *sets;             // romset MAME necessari ("samsho5 + samsho5a")
} neodb_t;
#include "neodb.h"
#include "neocrypt_tables.h"
#define NDB ((int)(sizeof neodb / sizeof neodb[0]))

// ------------------------------------------------------------------ indice degli zip
typedef struct { uint32_t crc, size; int zip; unsigned idx; } ent_t;
static char **zips; static int nzips;
static ent_t *ents; static int nents, cents;
static char err[256];
const char *neorom_error(void) { return err; }

static void clear_index(void) {
    for (int i = 0; i < nzips; ++i) free(zips[i]);
    free(zips); zips = NULL; nzips = 0;
    free(ents); ents = NULL; nents = cents = 0;
}

void neorom_scan(const char *romsdir) {
    clear_index();
    DIR *d = opendir(romsdir);
    if (!d) return;
    for (struct dirent *e; (e = readdir(d)); ) {
        size_t l = strlen(e->d_name);
        if (l < 5 || strcasecmp(e->d_name + l - 4, ".zip")) continue;
        char path[1200]; snprintf(path, sizeof path, "%s/%s", romsdir, e->d_name);
        mz_zip_archive z; memset(&z, 0, sizeof z);
        if (!mz_zip_reader_init_file(&z, path, 0)) continue;
        zips = (char**)realloc(zips, (nzips + 1) * sizeof *zips);
        zips[nzips] = strdup(path);
        unsigned n = mz_zip_reader_get_num_files(&z);
        for (unsigned i = 0; i < n; ++i) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&z, i, &st) || st.m_is_directory) continue;
            if (nents == cents) { cents = cents ? cents * 2 : 1024; ents = (ent_t*)realloc(ents, cents * sizeof *ents); }
            ents[nents++] = (ent_t){ st.m_crc32, (uint32_t)st.m_uncomp_size, nzips, i };
        }
        mz_zip_reader_end(&z);
        ++nzips;
    }
    closedir(d);
}
int neorom_zip_count(void) { return nzips; }
const char *neorom_zip_path(int i) { return i >= 0 && i < nzips ? zips[i] : NULL; }

static const ent_t *find(uint32_t crc, uint32_t size) {
    for (int i = 0; i < nents; ++i) if (ents[i].crc == crc && ents[i].size == size) return &ents[i];
    return NULL;
}
const char *neorom_zip_with(uint32_t crc, uint32_t size) { const ent_t *e = find(crc, size); return e ? zips[e->zip] : NULL; }

static const neodb_t *db(const char *id) {
    for (int i = 0; i < NDB; ++i) if (!strcmp(neodb[i].id, id)) return &neodb[i];
    return NULL;
}
int neorom_known(const char *id) { return db(id) != NULL; }

// tutti i file del gioco in un unico elenco (per mancanti e cache)
static int all_files(const neodb_t *g, const nrf_t **out) {
    int n = 0;
    for (int i = 0; i < g->np; ++i) out[n++] = &g->p[i];
    if (g->s[0].size) out[n++] = &g->s[0];
    out[n++] = &g->m[0];
    for (int i = 0; i < g->nv; ++i) out[n++] = &g->v[i];
    for (int i = 0; i < g->nc; ++i) out[n++] = &g->c[i];
    return n;
}
int neorom_missing(const char *id) {
    const neodb_t *g = db(id);
    if (!g) return -1;
    const nrf_t *f[40]; int n = all_files(g, f), miss = 0;
    for (int i = 0; i < n; ++i) if (!find(f[i]->crc, f[i]->size)) ++miss;
    return miss;
}
int neorom_total(const char *id) {
    const neodb_t *g = db(id);
    if (!g) return -1;
    const nrf_t *f[40];
    return all_files(g, f);
}
const char *neorom_sets(const char *id) { const neodb_t *g = db(id); return g ? g->sets : NULL; }
// nomi dei romset MAME con file mancanti (separati da " + "); ritorna il numero di set mancanti
int neorom_missing_sets(const char *id, char *out, size_t n) {
    const neodb_t *g = db(id);
    if (n) out[0] = 0;
    if (!g) return -1;
    const nrf_t *f[40]; int nf = all_files(g, f), miss[8] = {0}, cnt = 0;
    for (int i = 0; i < nf; ++i) if (!find(f[i]->crc, f[i]->size) && f[i]->set < 8) miss[f[i]->set] = 1;
    const char *s = g->sets;
    for (int k = 0; *s && k < 8; ++k) {
        const char *e = strstr(s, " + "); size_t l = e ? (size_t)(e - s) : strlen(s);
        if (miss[k]) {
            size_t o = strlen(out);
            if (o + l + 4 < n) { if (o) { memcpy(out + o, " + ", 3); o += 3; } memcpy(out + o, s, l); out[o + l] = 0; }
            ++cnt;
        }
        s = e ? e + 3 : s + l;
    }
    return cnt;
}
int neorom_missing_list(const char *id, char *out, size_t sz) {
    const neodb_t *g = db(id);
    if (!g) return -1;
    const nrf_t *f[40]; int n = all_files(g, f), miss = 0; size_t o = 0;
    if (sz) out[0] = 0;
    for (int i = 0; i < n; ++i) if (!find(f[i]->crc, f[i]->size)) {
        ++miss;
        if (o + 32 < sz) o += snprintf(out + o, sz - o, "%08x %u\n", (unsigned)f[i]->crc, (unsigned)f[i]->size);
    }
    return miss;
}

// ------------------------------------------------------------------ lettura dei file
static mz_zip_archive zopen[64]; static int zopened[64];
static void zclose_all(void) {
    for (int i = 0; i < 64; ++i) if (zopened[i]) { mz_zip_reader_end(&zopen[i]); zopened[i] = 0; }
}
// copia il file in dst (deve essere lungo f->size)
static int rd(const nrf_t *f, uint8_t *dst) {
    const ent_t *e = find(f->crc, f->size);
    if (!e) { snprintf(err, sizeof err, "missing file crc %08x size %u", (unsigned)f->crc, (unsigned)f->size); return 0; }
    int zi = e->zip % 64;
    if (zopened[zi] != e->zip + 1) {
        if (zopened[zi]) mz_zip_reader_end(&zopen[zi]);
        memset(&zopen[zi], 0, sizeof zopen[zi]); zopened[zi] = 0;
        if (!mz_zip_reader_init_file(&zopen[zi], zips[e->zip], 0)) { snprintf(err, sizeof err, "cannot open %s", zips[e->zip]); return 0; }
        zopened[zi] = e->zip + 1;
    }
    if (!mz_zip_reader_extract_to_mem(&zopen[zi], e->idx, dst, f->size, 0)) {
        snprintf(err, sizeof err, "read error in %s", zips[e->zip]); return 0;
    }
    return 1;
}

// ------------------------------------------------------------------ utilita'
static uint32_t bitswap(uint32_t v, int n, const int *bits) {   // come bitswap<N>(v, bits[0] = bit alto ... )
    uint32_t o = 0;
    for (int i = 0; i < n; ++i) o |= ((v >> bits[i]) & 1u) << (n - 1 - i);
    return o;
}
static uint8_t *mirror(const uint8_t *src, size_t len, size_t size) {   // (b * k)[:size]
    uint8_t *d = (uint8_t*)malloc(size);
    for (size_t o = 0; o < size; o += len) memcpy(d + o, src, size - o < len ? size - o : len);
    return d;
}

// ------------------------------------------------------------------ SMA (68000)
static void sma(uint8_t *p, const int *data_bits, uint32_t fixed_src, const int *fixed_bits, uint32_t bank_len,
                int nbank_bits, const int *bank_bits, uint32_t bank_size, int fixed_first) {
    uint16_t *rom = (uint16_t*)p;   // parole little-endian (host x86)
    const uint32_t b = 0x100000 / 2;
    for (uint32_t i = 0; i < 0x800000 / 2; ++i) rom[b + i] = (uint16_t)bitswap(rom[b + i], 16, data_bits);
    for (int pass = 0; pass < 2; ++pass) {
        int do_fixed = fixed_first ? pass == 0 : pass == 1;
        if (do_fixed) {
            for (uint32_t i = 0; i < 0xc0000 / 2; ++i) rom[i] = rom[fixed_src / 2 + bitswap(i, 19, fixed_bits)];
        } else {
            uint32_t n = bank_len / 2;
            uint32_t *j = (uint32_t*)malloc(n * sizeof *j);
            uint16_t *tmp = (uint16_t*)malloc(n * 2);
            for (uint32_t i = 0; i < n; ++i) j[i] = bitswap(i, nbank_bits, bank_bits);
            for (uint32_t s = 0; s < bank_size / 2; s += n) {
                memcpy(tmp, rom + b + s, n * 2);
                for (uint32_t i = 0; i < n; ++i) rom[b + s + i] = tmp[j[i]];
            }
            free(j); free(tmp);
        }
    }
}
static const int SG_D[16] = {13,12,14,10,8,2,3,1,5,9,11,4,15,0,6,7};
static const int SG_F[19] = {18,4,5,16,14,7,9,6,13,17,15,3,1,2,12,11,8,10,0};
static const int SG_B[14] = {9,4,8,3,13,6,2,7,0,12,1,11,10,5};
static const int S9_D[16] = {13,7,3,0,9,4,5,6,1,12,8,14,10,11,2,15};
static const int S9_F[19] = {18,11,6,14,17,16,5,8,10,12,0,4,3,2,7,9,15,13,1};
static const int S9_B[10] = {6,2,4,9,8,3,1,7,0,5};
static const int SK_D[16] = {12,8,11,3,15,14,7,0,10,13,6,5,9,2,1,4};
static const int SK_F[19] = {18,8,4,15,13,3,14,16,2,6,17,7,12,10,0,5,11,1,9};
static const int SK_B[10] = {4,1,3,8,6,2,7,0,9,5};

// ------------------------------------------------------------------ CMC42/CMC50 (grafica)
static uint8_t *cmc_gfx(const uint8_t *c, size_t len, int extra_xor, int chip) {
    const uint8_t *t0_03, *t0_12, *t1_03, *t1_12, *a815_1, *a815_2, *a1623_1, *a1623_2, *a07;
    if (chip == 42) {
        t0_03 = T_kof99_type0_t03; t0_12 = T_kof99_type0_t12; t1_03 = T_kof99_type1_t03; t1_12 = T_kof99_type1_t12;
        a815_1 = T_kof99_address_8_15_xor1; a815_2 = T_kof99_address_8_15_xor2;
        a1623_1 = T_kof99_address_16_23_xor1; a1623_2 = T_kof99_address_16_23_xor2; a07 = T_kof99_address_0_7_xor;
    } else {
        t0_03 = T_kof2000_type0_t03; t0_12 = T_kof2000_type0_t12; t1_03 = T_kof2000_type1_t03; t1_12 = T_kof2000_type1_t12;
        a815_1 = T_kof2000_address_8_15_xor1; a815_2 = T_kof2000_address_8_15_xor2;
        a1623_1 = T_kof2000_address_16_23_xor1; a1623_2 = T_kof2000_address_16_23_xor2; a07 = T_kof2000_address_0_7_xor;
    }
    uint32_t n = (uint32_t)(len / 4);
    uint8_t *buf = (uint8_t*)malloc(len), *out = (uint8_t*)malloc(len);
    if (!buf || !out) { free(buf); free(out); return NULL; }
    for (uint32_t r = 0; r < n; ++r) {
        const uint8_t *s = c + 4 * r; uint8_t *d = buf + 4 * r;
        uint32_t hi = (r >> 8) & 0xff;
        // b0, b3
        {
            uint32_t tmp = t1_03[(r & 0xff) ^ a07[hi]];
            uint8_t x0 = (t0_03[hi] & 0xfe) | (tmp & 1), x1 = (tmp & 0xfe) | (t0_12[hi] & 1);
            int inv = (r >> 8) & 1;
            d[0] = inv ? s[3] ^ x0 : s[0] ^ x0;
            d[3] = inv ? s[0] ^ x1 : s[3] ^ x1;
        }
        // b1, b2
        {
            uint32_t tmp = t1_12[(r & 0xff) ^ a07[hi]];
            uint8_t x0 = (t0_12[hi] & 0xfe) | (tmp & 1), x1 = (tmp & 0xfe) | (t0_03[hi] & 1);
            int inv = ((r >> 16) ^ a1623_2[hi]) & 1;
            d[1] = inv ? s[2] ^ x0 : s[1] ^ x0;
            d[2] = inv ? s[1] ^ x1 : s[2] ^ x1;
        }
    }
    for (uint32_t r = 0; r < n; ++r) {
        uint32_t br = r ^ (uint32_t)extra_xor;
        br ^= (uint32_t)a815_1[(br >> 16) & 0xff] << 8;
        br ^= (uint32_t)a815_2[br & 0xff] << 8;
        br ^= (uint32_t)a1623_1[br & 0xff] << 16;
        br ^= (uint32_t)a1623_2[(br >> 8) & 0xff] << 16;
        br ^= a07[(br >> 8) & 0xff];
        br &= n - 1;
        memcpy(out + 4 * r, buf + 4 * br, 4);
    }
    free(buf);
    return out;
}
static void sfix(const uint8_t *c, size_t clen, uint8_t *dst, uint32_t size) {
    const uint8_t *src = c + clen - size;
    for (uint32_t i = 0; i < size; ++i)
        dst[i] = src[(i & ~0x1fu) + ((i & 7) << 2) + ((~i & 8) >> 2) + ((i & 0x10) >> 4)];
}

// ------------------------------------------------------------------ CMC50 (M1)
static const int P1[8][16] = {
    {15,14,10,7,1,2,3,8,0,12,11,13,6,9,5,4}, {7,1,8,11,15,9,2,3,5,13,4,14,10,0,6,12},
    {8,6,14,3,10,7,15,1,4,0,2,5,13,11,12,9}, {2,8,15,9,3,4,11,7,13,6,0,10,1,12,14,5},
    {1,13,6,15,14,3,8,10,9,4,7,12,5,2,0,11}, {11,15,3,4,7,0,9,2,6,14,12,1,8,5,10,13},
    {10,5,13,8,6,15,1,14,11,9,3,0,12,7,4,2}, {9,3,7,0,2,12,4,11,14,10,5,8,15,13,1,6} };
static void cmc50_m1(uint8_t *m, size_t len) {
    uint8_t *rom = (uint8_t*)malloc(0x80000);
    memset(rom, 0xff, 0x80000); memcpy(rom, m, len < 0x80000 ? len : 0x80000);
    uint32_t key = 0;
    for (int i = 0; i < 0x10000; ++i) key += rom[i];
    key &= 0xffff;
    static const int KB[16] = {12,0,2,4,8,15,7,13,10,1,3,6,11,9,14,5};
    static const int FB[16] = {7,15,14,6,5,13,12,4,11,3,10,2,9,1,8,0};
    uint32_t kx = bitswap(key, 16, KB);
    int pb[8][16];
    for (int b = 0; b < 8; ++b) for (int k = 0; k < 16; ++k) pb[b][k] = P1[b][15 - k];
    for (uint32_t a = 0; a < len && a < 0x80000; ++a) {
        uint32_t block = (a >> 16) & 7;
        uint32_t aux = (a & 0xffff) ^ kx;
        aux = bitswap(aux, 16, pb[block]);
        aux ^= T_m1_address_0_7_xor[(aux >> 8) & 0xff];
        aux ^= (uint32_t)T_m1_address_8_15_xor[aux & 0xff] << 8;
        aux = bitswap(aux, 16, FB);
        m[a] = rom[(block << 16) | aux];
    }
    free(rom);
}

// ------------------------------------------------------------------ KOF 2002 / KOF 98 / KOF 2003 (68000)
static void kof2002_68k(uint8_t *p) {
    static const uint32_t sec[8] = {0x100000,0x280000,0x300000,0x180000,0x000000,0x380000,0x200000,0x080000};
    uint8_t *src = (uint8_t*)malloc(0x400000);
    memcpy(src, p + 0x100000, 0x400000);
    for (int i = 0; i < 8; ++i) memcpy(p + 0x100000 + i * 0x80000, src + sec[i], 0x80000);
    free(src);
}
static uint8_t *kof98_68k(const uint8_t *p, size_t len) {   // ritorna 5 MB
    uint8_t *src = (uint8_t*)malloc(0x600000), *dst = (uint8_t*)malloc(0x200000);
    memset(src, 0xff, 0x600000); memcpy(src, p, len < 0x600000 ? len : 0x600000);
    memcpy(dst, src, 0x200000);
    static const uint32_t sec[8] = {0x000000,0x100000,0x000004,0x100004,0x10000a,0x00000a,0x10000e,0x00000e};
    static const uint32_t pos[4] = {0x000,0x004,0x00a,0x00e};
    for (uint32_t i = 0x800; i < 0x100000; i += 0x200) {
        for (uint32_t j = 0; j < 0x100; j += 0x10) {
            uint32_t b = i + j;
            for (uint32_t k = 0; k < 16; k += 2) {
                uint32_t s = sec[k / 2];
                memcpy(src + b + k, dst + b + s + 0x100, 2);
                memcpy(src + b + k + 0x100, dst + b + s, 2);
            }
            if (i >= 0x080000 && i < 0x0c0000) {
                for (int q = 0; q < 4; ++q) {
                    memcpy(src + b + pos[q], dst + b + pos[q], 2);
                    memcpy(src + b + pos[q] + 0x100, dst + b + pos[q] + 0x100, 2);
                }
            } else if (i >= 0x0c0000) {
                for (int q = 0; q < 4; ++q) {
                    memcpy(src + b + pos[q], dst + b + pos[q] + 0x100, 2);
                    memcpy(src + b + pos[q] + 0x100, dst + b + pos[q], 2);
                }
            }
        }
        memcpy(src + i, dst + i, 2);
        memcpy(src + i + 2, dst + i + 0x100000, 2);
        memcpy(src + i + 0x100, dst + i + 0x100, 2);
        memcpy(src + i + 0x102, dst + i + 0x100100, 2);
    }
    memmove(src + 0x100000, src + 0x200000, 0x400000);
    free(dst);
    return src;   // primi 5 MB validi
}
static uint8_t *kof2003_68k(const uint8_t *p1, const uint8_t *p2, const uint8_t *p3) {   // ritorna 9 MB
    uint8_t *rom = (uint8_t*)calloc(0x900000, 1), *buf = (uint8_t*)calloc(0x900000, 1), *out = (uint8_t*)malloc(0x900000);
    for (uint32_t k = 0; k < 0x200000; ++k) {
        rom[4 * k] = p1[2 * k]; rom[4 * k + 1] = p1[2 * k + 1];
        rom[4 * k + 2] = p2[2 * k]; rom[4 * k + 3] = p2[2 * k + 1];
    }
    memcpy(rom + 0x800000, p3, 0x100000);
    static const uint8_t xor1[32] = {0x3b,0x6a,0xf7,0xb7,0xe8,0xa9,0x20,0x99,0x9f,0x39,0x34,0x0c,0xc3,0x9a,0xa5,0xc8,0xb8,0x18,0xce,0x56,0x94,0x44,0xe3,0x7a,0xf7,0xdd,0x42,0xf0,0x18,0x60,0x92,0x9f};
    static const uint8_t xor2[32] = {0x2f,0x02,0x60,0xbb,0x77,0x01,0x30,0x08,0xd8,0x01,0xa0,0xdf,0x37,0x0a,0xf0,0x65,0x28,0x03,0xd0,0x23,0xd3,0x03,0x70,0x42,0xbb,0x06,0xf0,0x28,0xba,0x0f,0xf0,0x7a};
    for (uint32_t i = 0; i < 0x100000; ++i) rom[0x800000 + i] ^= rom[0x100002 | i];
    for (uint32_t i = 0; i < 0x100000; ++i) rom[i] ^= xor1[i % 32];
    for (uint32_t j = 0x100000; j < 0x800000; ++j) rom[j] ^= xor2[j % 32];
    static const int WB[16] = {15,14,13,12,5,4,7,6,9,8,11,10,3,2,1,0};
    for (uint32_t w = 0x100000; w < 0x800000; w += 4) {
        uint32_t v = rom[w + 1] | ((uint32_t)rom[w + 2] << 8);
        v = bitswap(v, 16, WB);
        rom[w + 1] = v & 0xff; rom[w + 2] = v >> 8;
    }
    static const int KB[8] = {7,6,5,4,0,1,2,3}, BB[8] = {4,5,6,7,1,0,3,2};
    for (uint32_t k = 0; k < 16; ++k) {
        uint32_t ofst = (k & 0xf0) + bitswap(k & 0x0f, 8, KB);
        memcpy(buf + k * 0x10000, rom + ofst * 0x10000, 0x10000);
    }
    for (uint32_t blk = 0x100000; blk < 0x900000; blk += 0x100) {
        uint32_t ofst = (blk & 0xf000ff) + ((blk & 0x000f00) ^ 0x00800) + (bitswap((blk & 0x0ff000) >> 12, 8, BB) << 12);
        memcpy(buf + blk, rom + (ofst & ~0xffu), 0x100);
    }
    memcpy(out, buf, 0x100000);
    memcpy(out + 0x100000, buf + 0x800000, 0x100000);
    memcpy(out + 0x200000, buf + 0x100000, 0x700000);
    free(rom); free(buf);
    return out;
}

// ------------------------------------------------------------------ NEO-PCM2 (V ROM)
static void pcm2_swap(uint8_t *v, size_t len, int value) {
    static const uint32_t addrs[7][2] = {{0x000000,0xa5000},{0xffce20,0x01000},{0xfe2cf6,0x4e001},{0xffac28,0xc2000},
                                          {0xfeb2c0,0x0a000},{0xff14ea,0xa7001},{0xffb440,0x02000}};
    static const uint8_t xr[7][8] = {{0xf9,0xe0,0x5d,0xf3,0xea,0x92,0xbe,0xef},{0xc4,0x83,0xa8,0x5f,0x21,0x27,0x64,0xaf},
        {0xc3,0xfd,0x81,0xac,0x6d,0xe7,0xbf,0x9e},{0xc3,0xfd,0x81,0xac,0x6d,0xe7,0xbf,0x9e},
        {0xcb,0x29,0x7d,0x43,0xd2,0x3a,0xc2,0xb4},{0x4b,0xa4,0x63,0x46,0xf0,0x91,0xea,0x62},{0x4b,0xa4,0x63,0x46,0xf0,0x91,0xea,0x62}};
    (void)len;   // sempre 16 MB (V1 + V2)
    uint8_t *buf = (uint8_t*)malloc(0x1000000);
    memcpy(buf, v, 0x1000000);
    for (uint32_t i = 0; i < 0x1000000; ++i) {
        uint32_t j = (i & 0xfe0000) | ((i & 1) << 16) | (i & 0xfffe & ~1u) | ((i >> 16) & 1);
        j ^= addrs[value][1];
        uint32_t d = (i + addrs[value][0]) & 0xffffff;
        v[j] = buf[d] ^ xr[value][j & 7];
    }
    free(buf);
}
static void pcm2_snk1999(uint8_t *v, size_t len, int value) {
    uint8_t tmp[64]; int h = value / 2;
    for (size_t o = 0; o + value <= len; o += value) {
        memcpy(tmp, v + o, h); memmove(v + o, v + o + h, h); memcpy(v + o + h, tmp, h);
    }
}

// ------------------------------------------------------------------ costruzione
static int readlist(const nrf_t *f, int n, uint8_t *dst) {
    for (int i = 0; i < n; ++i) { if (!rd(&f[i], dst)) return 0; dst += f[i].size; }
    return 1;
}
static size_t listsize(const nrf_t *f, int n) { size_t s = 0; for (int i = 0; i < n; ++i) s += f[i].size; return s; }
static uint8_t *readall(const nrf_t *f, int n, size_t *len) {
    *len = listsize(f, n);
    uint8_t *d = (uint8_t*)malloc(*len ? *len : 1);
    if (d && !readlist(f, n, d)) { free(d); return NULL; }
    return d;
}
static uint32_t hexv(const char **s) { uint32_t v = (uint32_t)strtoul(*s, (char**)s, 16); return v; }
static int apply_patches(const char *pt, uint8_t *p, size_t plen) {
    const char *s = pt;
    while (*s) {
        uint32_t a = hexv(&s); if (*s++ != ':') return 0;
        const char *o = s; while (*s && *s != ':') ++s; size_t ol = (size_t)(s - o) / 2; if (*s++ != ':') return 0;
        const char *nw = s; while (*s && *s != ';') ++s; size_t nl = (size_t)(s - nw) / 2; if (*s == ';') ++s;
        if (a + ol > plen || a + nl > plen) return 0;
        for (size_t k = 0; k < ol; ++k) {
            char hx[3] = { o[2 * k], o[2 * k + 1], 0 };
            if (p[(a + k) ^ 1] != (uint8_t)strtoul(hx, NULL, 16)) {
                snprintf(err, sizeof err, "patch %x: original bytes differ", (unsigned)a); return 0;
            }
        }
        for (size_t k = 0; k < nl; ++k) {
            char hx[3] = { nw[2 * k], nw[2 * k + 1], 0 };
            p[(a + k) ^ 1] = (uint8_t)strtoul(hx, NULL, 16);
        }
    }
    return 1;
}

static uint8_t *build(const neodb_t *g, size_t *outsz) {
    uint8_t *p = NULL, *s = NULL, *m = NULL, *v = NULL, *c = NULL, *img = NULL;
    size_t pl = 0, sl = 0, ml = 0, vl = 0, cl = 0;
    // ---- P
    if (g->pcrypt == NP_SMA_GAROU || g->pcrypt == NP_SMA_KOF99 || g->pcrypt == NP_SMA_KOF2000) {
        pl = 0x900000; p = (uint8_t*)calloc(pl, 1);
        if (!rd(&g->p[0], p + 0xc0000) || !readlist(g->p + 1, g->np - 1, p + 0x100000)) goto fail;
        if (g->pcrypt == NP_SMA_GAROU) sma(p, SG_D, 0x710000, SG_F, 0x8000, 14, SG_B, 0x800000, 1);
        if (g->pcrypt == NP_SMA_KOF99) sma(p, S9_D, 0x700000, S9_F, 0x800, 10, S9_B, 0x600000, 0);
        if (g->pcrypt == NP_SMA_KOF2000) sma(p, SK_D, 0x73a000, SK_F, 0x800, 10, SK_B, 0x63a000, 0);
    } else if (g->pcrypt == NP_KOF98) {
        size_t l; uint8_t *raw = readall(g->p, g->np, &l); if (!raw) goto fail;
        p = kof98_68k(raw, l); pl = 0x500000; free(raw);
    } else if (g->pcrypt == NP_KOF2003) {
        size_t l1, l2, l3;
        uint8_t *a = readall(&g->p[0], 1, &l1), *b = readall(&g->p[1], 1, &l2), *d = readall(&g->p[2], 1, &l3);
        if (a && b && d) { p = kof2003_68k(a, b, d); pl = 0x900000; }
        free(a); free(b); free(d);
        if (!p) goto fail;
    } else if (g->pcrypt == NP_SAMSHO5 || g->pcrypt == NP_SAMSH5SP) {
        // banchi da 512 KB rimescolati (MAME prot_kof2002.cpp samsho5/samsh5sp_decrypt_68k)
        static const uint32_t s5[16] = {0x000000,0x080000,0x700000,0x680000,0x500000,0x180000,0x200000,0x480000,0x300000,0x780000,0x600000,0x280000,0x100000,0x580000,0x400000,0x380000};
        static const uint32_t s5sp[16] = {0x000000,0x080000,0x500000,0x480000,0x600000,0x580000,0x700000,0x280000,0x100000,0x680000,0x400000,0x780000,0x200000,0x380000,0x300000,0x180000};
        uint8_t *raw = readall(g->p, g->np, &pl); if (!raw || pl != 0x800000) { free(raw); snprintf(err, sizeof err, "P size"); goto fail; }
        p = (uint8_t*)malloc(pl);
        for (int i = 0; i < 16; ++i) memcpy(p + i * 0x80000, raw + (g->pcrypt == NP_SAMSHO5 ? s5 : s5sp)[i], 0x80000);
        free(raw);
    } else if (g->pcrypt == NP_KOF2002) {
        p = readall(g->p, g->np, &pl); if (!p) goto fail;
        kof2002_68k(p);
    } else if (g->np == 2 && !g->cmc) {
        // romset non cifrato con p1 + p2/sp2: MAME carica p2 a $100000 (zona a banchi), specchiati come build_neo_mame.py
        size_t l1, l2; uint8_t *a = readall(&g->p[0], 1, &l1), *b = readall(&g->p[1], 1, &l2);
        if (!a || !b) { free(a); free(b); goto fail; }
        size_t n1 = l1 < 0x100000 ? 0x100000 : l1, n2 = l2 % 0x100000 ? (l2 + 0xfffff) / 0x100000 * 0x100000 : l2;
        uint8_t *ma = mirror(a, l1, n1), *mb = mirror(b, l2, n2);
        pl = n1 + n2; p = (uint8_t*)malloc(pl); memcpy(p, ma, n1); memcpy(p + n1, mb, n2);
        free(a); free(b); free(ma); free(mb);
    } else {
        p = readall(g->p, g->np, &pl); if (!p) goto fail;
    }
    // P di 2 MB con le meta' invertite (savagere, kof94, kof95): solo romset non cifrati
    if (!g->cmc && g->pcrypt == NP_NONE && pl == 0x200000 && memcmp(p + 0x100, "EN-OEG", 6) && !memcmp(p + 0x100100, "EN-OEG", 6)) {
        uint8_t *t = (uint8_t*)malloc(0x100000);
        memcpy(t, p, 0x100000); memmove(p, p + 0x100000, 0x100000); memcpy(p + 0x100000, t, 0x100000); free(t);
    }
    // ---- C
    {
        size_t half = 0;
        for (int i = 0; i < g->nc; i += 2) half += g->c[i].size;
        size_t step = 0;
        for (int i = 0; i < g->nc; i += 2) if (g->c[i].size * 2 > step) step = g->c[i].size * 2;
        cl = (g->flags & NF_STEP8M) ? step * (g->nc / 2 - 1) + g->c[g->nc - 2].size * 2 : half * 2;
        if (g->csize > cl) cl = g->csize;   // stessa dimensione del .neo dell'app (zeri in fondo / nei buchi)
        c = (uint8_t*)calloc(cl, 1);
        size_t o = 0;
        for (int i = 0; i < g->nc; i += 2) {
            size_t l = g->c[i].size;
            uint8_t *a = (uint8_t*)malloc(l), *b = (uint8_t*)malloc(g->c[i + 1].size);
            if (!rd(&g->c[i], a) || !rd(&g->c[i + 1], b)) { free(a); free(b); goto fail; }
            uint8_t *pair = (uint8_t*)malloc(l * 2);
            for (size_t k = 0; k < l; ++k) { pair[2 * k] = a[k]; pair[2 * k + 1] = b[k]; }
            free(a); free(b);
            if (g->flags & NF_AOFC) {   // prime meta' di tutte le coppie, poi le seconde meta'
                memcpy(c + o / 2, pair, l); memcpy(c + half + o / 2, pair + l, l);
                o += l * 2;
            } else if (g->flags & NF_STEP8M) {
                memcpy(c + (i / 2) * step, pair, l * 2);
            } else {
                memcpy(c + o, pair, l * 2); o += l * 2;
            }
            free(pair);
        }
        if (g->cmc) { uint8_t *d = cmc_gfx(c, cl, g->cmckey, g->cmc); free(c); c = d; if (!c) goto fail; }
    }
    // ---- S
    if (g->sfix) { sl = g->sfix; s = (uint8_t*)malloc(sl); sfix(c, cl, s, (uint32_t)sl); }
    else { s = readall(g->s, 1, &sl); if (!s) goto fail; }
    // ---- M
    m = readall(g->m, 1, &ml); if (!m) goto fail;
    if (g->m1cmc) cmc50_m1(m, ml);
    // ---- V
    v = readall(g->v, g->nv, &vl); if (!v) goto fail;
    if (g->vcrypt == NV_PCM2SWAP) {
        if (vl < 0x1000000) { snprintf(err, sizeof err, "V size"); goto fail; }
        pcm2_swap(v, vl, g->vparam);
        vl = 0x1000000;
    }
    if (g->vcrypt == NV_SNK1999) pcm2_snk1999(v, vl, g->vparam);
    // ---- patch anti-flicker del progetto (indirizzi 68000 su P in formato MAME = parole scambiate)
    if (g->patch && *g->patch && !apply_patches(g->patch, p, pl)) goto fail;
    // ---- immagine .neo (header Geolith)
    *outsz = 4096 + pl + sl + ml + vl + cl;
    img = (uint8_t*)calloc(*outsz, 1);
    if (!img) { snprintf(err, sizeof err, "out of memory"); goto fail; }
    memcpy(img, "NEO\x01", 4);
    uint32_t h[10] = { (uint32_t)pl, (uint32_t)sl, (uint32_t)ml, (uint32_t)vl, 0, (uint32_t)cl,
                       (uint32_t)g->year, 1, 0, (uint32_t)g->ngh };
    for (int i = 0; i < 10; ++i) for (int k = 0; k < 4; ++k) img[4 + 4 * i + k] = (uint8_t)(h[i] >> (8 * k));
    strncpy((char*)img + 44, g->title, 33);
    memcpy(img + 77, "SNK", 3);
    size_t o = 4096;
    memcpy(img + o, p, pl); o += pl; memcpy(img + o, s, sl); o += sl; memcpy(img + o, m, ml); o += ml;
    memcpy(img + o, v, vl); o += vl; memcpy(img + o, c, cl);
fail:
    free(p); free(s); free(m); free(v); free(c);
    zclose_all();
    return img;
}

// identificativo della ricetta: CRC di tutti i file + patch (la cache si rifa' se cambia qualcosa)
static uint32_t recipe_crc(const neodb_t *g) {
    const nrf_t *f[40]; int n = all_files(g, f);
    mz_ulong crc = mz_crc32(MZ_CRC32_INIT, (const unsigned char*)g->id, strlen(g->id));
    for (int i = 0; i < n; ++i) crc = mz_crc32(crc, (const unsigned char*)f[i], sizeof *f[i]);
    if (g->patch) crc = mz_crc32(crc, (const unsigned char*)g->patch, strlen(g->patch));
    crc = mz_crc32(crc, (const unsigned char*)"neorom1", 7);
    return (uint32_t)crc;
}

// ------------------------------------------------------------------ cache su disco
// Solo per i giochi che richiedono piu' tempo (decrittazione): limite totale CACHE_MAX, file meno usati eliminati per
// primi, mai sotto CACHE_FREE di spazio libero sul disco; ogni errore di scrittura viene ignorato (cache facoltativa).
#define CACHE_MAX   (1536ull << 20)
#define CACHE_FREE  (2048ull << 20)
#define CACHE_MIN_MS 400
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <sys/utime.h>
static unsigned long long disk_free(const char *dir) {
    ULARGE_INTEGER a;
    return GetDiskFreeSpaceExA(dir, &a, NULL, NULL) ? a.QuadPart : 0;
}
static unsigned now_ms(void) { return (unsigned)GetTickCount(); }
#else
#include <sys/statvfs.h>
#include <utime.h>
static unsigned long long disk_free(const char *dir) {
    struct statvfs v;
    return statvfs(dir, &v) ? 0 : (unsigned long long)v.f_bavail * v.f_frsize;
}
static unsigned now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (unsigned)(t.tv_sec * 1000 + t.tv_nsec / 1000000); }
#endif
static int is_cache_name(const char *n) {   // <id>_<8 hex>.neo
    size_t l = strlen(n);
    return l > 13 && !strcmp(n + l - 4, ".neo") && n[l - 13] == '_';
}
// elimina i file piu' vecchi finche' ne restano per meno di (CACHE_MAX - need); rimuove anche le vecchie versioni di id
static void cache_trim(const char *dir, const char *id, unsigned long long need) {
    for (;;) {
        DIR *d = opendir(dir);
        if (!d) return;
        unsigned long long tot = 0; time_t oldest = 0; char oname[300] = "";
        size_t il = strlen(id);
        for (struct dirent *e; (e = readdir(d)); ) {
            char p[1200]; snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
            size_t nl = strlen(e->d_name);
            if (nl > 8 && !strcmp(e->d_name + nl - 8, ".neo.tmp")) { remove(p); continue; }   // scrittura interrotta
            if (!is_cache_name(e->d_name)) continue;
            struct stat st; if (stat(p, &st)) continue;
            if (!strncmp(e->d_name, id, il) && e->d_name[il] == '_') { remove(p); continue; }   // versione precedente
            tot += (unsigned long long)st.st_size;
            if (!oname[0] || st.st_mtime < oldest) { oldest = st.st_mtime; snprintf(oname, sizeof oname, "%s", e->d_name); }
        }
        closedir(d);
        if (tot + need <= CACHE_MAX || !oname[0]) return;
        char p[1200]; snprintf(p, sizeof p, "%s/%s", dir, oname);
        if (remove(p)) return;
    }
}

// svuota la cache (ROM CACHE = OFF): solo i file creati da neorom
void neorom_cache_clear(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    for (struct dirent *e; (e = readdir(d)); ) {
        size_t nl = strlen(e->d_name);
        if (is_cache_name(e->d_name) || (nl > 8 && !strcmp(e->d_name + nl - 8, ".neo.tmp"))) {
            char p[1200]; snprintf(p, sizeof p, "%s/%s", dir, e->d_name); remove(p);
        }
    }
    closedir(d);
}

void *neorom_build(const char *id, const char *cachedir, size_t *sz) {
    err[0] = 0;
    const neodb_t *g = db(id);
    if (!g) { snprintf(err, sizeof err, "unknown game %s", id); return NULL; }
    if (neorom_missing(id) > 0) { snprintf(err, sizeof err, "%d ROM files missing", neorom_missing(id)); return NULL; }
    char cp[1200] = "";
    if (cachedir) {
        snprintf(cp, sizeof cp, "%s/%s_%08x.neo", cachedir, id, (unsigned)recipe_crc(g));
        FILE *f = fopen(cp, "rb");
        if (f) {
            fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
            uint8_t *d = n > 4096 ? (uint8_t*)malloc(n) : NULL;
            int ok = d && fread(d, 1, n, f) == (size_t)n;
            fclose(f);
            if (ok) {   // controllo di coerenza: dimensioni dell'header
                size_t t = 4096;
                for (int i = 0; i < 6; ++i) t += (size_t)d[4 + 4 * i] | (size_t)d[5 + 4 * i] << 8 | (size_t)d[6 + 4 * i] << 16 | (size_t)d[7 + 4 * i] << 24;
                ok = !memcmp(d, "NEO\x01", 4) && t == (size_t)n;
            }
            if (ok) { utime(cp, NULL); *sz = (size_t)n; return d; }   // usato ora: ultimo a essere eliminato
            free(d); remove(cp);
        }
    }
    unsigned t0 = now_ms();
    uint8_t *img = build(g, sz);
    if (img && cachedir && now_ms() - t0 >= CACHE_MIN_MS && *sz < CACHE_MAX) {
        cache_trim(cachedir, id, *sz);
        if (disk_free(cachedir) >= *sz + CACHE_FREE) {
            char tp[1210]; snprintf(tp, sizeof tp, "%s.tmp", cp);
            FILE *f = fopen(tp, "wb");
            if (f) {
                int ok = fwrite(img, 1, *sz, f) == *sz;
                ok &= fclose(f) == 0;
                if (ok) { remove(cp); if (rename(tp, cp)) remove(tp); } else remove(tp);
            }
        }
    }
    return img;
}
