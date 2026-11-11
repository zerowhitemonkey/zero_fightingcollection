/**********************************************************************
 SNKHD - core CPS3

 Adattato dal driver CPS3 di FBNeo (cps3run.cpp / cps3snd.cpp):
   Decryption by Andreas Naive, driver by David Haywood with help from
   Tomasz Slanina and ElSemi, sound emulation by Philip Bennett,
   latest fixes by MetalliC, port to FBA by OopsWare.
 Licenza FBNeo: uso non commerciale.

 Modifiche SNKHD: caricamento diretto del romset zip (miniz), avvio veloce
 come FBNeo, audio alla frequenza nativa del chip (37286,89 Hz), overclock
 dell'SH-2, renderer HD (sprite/tilemap ricampionati alla risoluzione di
 uscita invece che al canvas 384x224 ridotto dallo zoom a schermo intero),
 renderer di riferimento identico a FBNeo per i test di regressione.
**********************************************************************/
#include "burnint.h"
#include "sh2_intf.h"
#include "cps3.h"
#include <miniz.h>
#include <math.h>

INT32 (*BurnAcb)(struct BurnArea *pba) = NULL;

// ------------------------------------------------------------------ memoria
static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;
static UINT8 *RomBios, *RomGame, *RomGame_D, *RomUser;
static UINT8 *RamMain;
static UINT32 *RamSpr, *SprList;
static UINT16 *RamPal;
static UINT32 *RamCRam, *RamSS, *RamVReg, *RamVRegBuf;
static UINT8 *RamC000, *RamC000_D;
static UINT16 *EEPROM;
static UINT32 *Pal32;           // tavolozza 0xAARRGGBB (0x20000 voci + 2)
static UINT32 *RamScreen;       // canvas del renderer di riferimento (1024 x 480)
static UINT32 *RamScreenBase;

static UINT32 cps3_key1, cps3_key2;
static UINT32 cps3_bios_test_hack, cps3_game_test_hack;
static UINT32 cps3_speedup_ram_address, cps3_speedup_code_address;
static UINT32 cps3_region_address;
static UINT8  cps3_dip;
static UINT32 cps3_data_rom_size;
static int    is_jojo;

static UINT16 Cps3Input[4];
extern "C" { int cps3_dbg_ioread = 0; unsigned cps3_dbg_bioswatch = 0; }
extern "C" const uint8_t *cps3_prog(void) { return RomGame_D; }   // programma decifrato (parole 32 bit little-endian)
// patch del codice: solo la copia decifrata usata per il fetch delle istruzioni (le letture dati vedono l'originale)
extern "C" int cps3_patch16(uint32_t addr, uint16_t expect, uint16_t val) {
    if (!RomGame_D || addr < 0x06000000 || addr >= 0x07000000 || (addr & 1)) return 0;
    UINT16 *w = (UINT16 *)(RomGame_D + ((addr - 0x06000000) ^ 2));
    if (*w != expect && *w != val) return 0;
    *w = val;
    return 1;
}

static UINT32 ss_bank_base, ss_pal_base, cram_bank;
static UINT16 cps3_current_eeprom_read;
static UINT32 gfxflash_bank;
static UINT32 paldma_source, paldma_dest, paldma_fade, paldma_length;
static UINT32 chardma_source, chardma_table_address;
static INT32  dma_timer;
static UINT16 dma_status;
static UINT16 spritelist_dma, spritelist_dma_prev;
static INT32  cps_int10_cnt;
static INT32  nExtraCycles;
static int    overclock_pct = 100;
static int    loaded;

static UINT32 watch_lo = 1, watch_hi = 0;

// ------------------------------------------------------------------ flash (29F016)
enum { FM_NORMAL, FM_READID, FM_READSTATUS, FM_WRITEPART1, FM_CLEARPART1, FM_SETMASTER,
       FM_READAMDID1, FM_READAMDID2, FM_READAMDID3, FM_ERASEAMD1, FM_ERASEAMD2, FM_ERASEAMD3,
       FM_ERASEAMD4, FM_BYTEPROGRAM };
typedef struct { INT32 status, flash_mode, flash_master_lock; } cps3_flash_chip;
static cps3_flash_chip main_flash;

static UINT32 cps3_flash_read(cps3_flash_chip *chip, UINT32 addr) {
    switch (chip->flash_mode) {
    case FM_READAMDID3:
    case FM_READID:
        switch (addr & 0x7fffff) {
        case 0: return 0x04040404;
        case 4: return 0xadadadad;
        case 8: return 0x00000000;
        }
        return 0;
    case FM_ERASEAMD4:
        chip->status ^= (1 << 6) | (1 << 2);
        // fallthrough
    case FM_READSTATUS:
        return (chip->status << 0) | (chip->status << 8) | (chip->status << 16) | (chip->status << 24);
    default:
        return 0;
    }
}

static void cps3_flash_write(cps3_flash_chip *chip, UINT32 addr, UINT32 data) {
    switch (chip->flash_mode) {
    case FM_NORMAL: case FM_READSTATUS: case FM_READID: case FM_READAMDID3:
        switch (data & 0xff) {
        case 0xf0: case 0xff: chip->flash_mode = FM_NORMAL; break;
        case 0x90: chip->flash_mode = FM_READID; break;
        case 0x40: case 0x10: chip->flash_mode = FM_WRITEPART1; break;
        case 0x50: chip->status = 0x80; chip->flash_mode = FM_READSTATUS; break;
        case 0x20: chip->flash_mode = FM_CLEARPART1; break;
        case 0x60: chip->flash_mode = FM_SETMASTER; break;
        case 0x70: chip->flash_mode = FM_READSTATUS; break;
        case 0xaa: if ((addr & 0xffff) == (0x555 << 2)) chip->flash_mode = FM_READAMDID1; break;
        }
        break;
    case FM_READAMDID1:
        if ((addr & 0xffff) == (0x2aa << 2) && (data & 0xff) == 0x55) chip->flash_mode = FM_READAMDID2;
        else chip->flash_mode = FM_NORMAL;
        break;
    case FM_READAMDID2:
        if ((addr & 0xffff) == (0x555 << 2) && (data & 0xff) == 0x90) chip->flash_mode = FM_READAMDID3;
        else if ((addr & 0xffff) == (0x555 << 2) && (data & 0xff) == 0x80) chip->flash_mode = FM_ERASEAMD1;
        else if ((addr & 0xffff) == (0x555 << 2) && (data & 0xff) == 0xa0) chip->flash_mode = FM_BYTEPROGRAM;
        else chip->flash_mode = FM_NORMAL;
        break;
    }
}

// ------------------------------------------------------------------ decrittazione
static UINT16 rotate_left(UINT16 value, INT32 n) { INT32 aux = value >> (16 - n); return ((value << n) | aux) & 0xffff; }
static UINT16 rotxor(UINT16 val, UINT16 x) {
    UINT16 res = val + rotate_left(val, 2);
    res = rotate_left(res, 4) ^ (res & (val ^ x));
    return res;
}
static UINT32 cps3_mask(UINT32 address, UINT32 key1, UINT32 key2) {
    UINT16 val;
    address ^= key1;
    val = (address & 0xffff) ^ 0xffff;
    val = rotxor(val, key2 & 0xffff);
    val ^= (address >> 16) ^ 0xffff;
    val = rotxor(val, key2 >> 16);
    val ^= (address & 0xffff) ^ (key2 & 0xffff);
    return val | (val << 16);
}
static void cps3_decrypt_bios(void) {
    UINT32 *coderegion = (UINT32 *)RomBios;
    for (INT32 i = 0; i < 0x20000; i += 4) {
        UINT32 xormask = cps3_mask(i, cps3_key1, cps3_key2);
        if ((i < 0x1ff00) || (i > 0x1ff6b)) coderegion[i / 4] ^= xormask;
    }
}
static void cps3_decrypt_game(void) {
    UINT32 *coderegion = (UINT32 *)RomGame, *dec = (UINT32 *)RomGame_D;
    for (INT32 i = 0; i < 0x1000000; i += 4)
        dec[i / 4] = coderegion[i / 4] ^ cps3_mask(i + 0x06000000, cps3_key1, cps3_key2);
}

// ------------------------------------------------------------------ DMA grafica (decompressione char RAM)
static INT32 last_normal_byte = 0;
static UINT32 process_byte(UINT8 real_byte, UINT32 destination, INT32 max_length) {
    UINT8 *dest = (UINT8 *)RamCRam;
    destination &= 0x7fffff;
    if (real_byte & 0x40) {
        INT32 tranfercount = 0;
        INT32 rle = (real_byte & 0x3f) + 1;
        while (rle) {
            dest[((destination + tranfercount) & 0x7fffff) ^ 3] = (last_normal_byte & 0x3f);
            tranfercount++; rle--; max_length--;
            if ((destination + tranfercount) > 0x7fffff) return max_length;
        }
        return tranfercount;
    }
    dest[(destination & 0x7fffff) ^ 3] = real_byte;
    last_normal_byte = real_byte;
    return 1;
}

static void cps3_do_char_dma(UINT32 real_source, UINT32 real_destination, UINT32 real_length) {
    UINT8 *src = RomUser;
    INT32 length_remaining = real_length;
    last_normal_byte = 0;
    while (length_remaining) {
        UINT8 current_byte = src[real_source % cps3_data_rom_size];
        real_source++;
        if (current_byte & 0x80) {
            current_byte &= 0x7f;
            for (int k = 0; k < 2; ++k) {
                UINT8 real_byte = src[(chardma_table_address + current_byte * 2 + k) % cps3_data_rom_size];
                UINT32 lp = process_byte(real_byte, real_destination, length_remaining);
                length_remaining -= lp; real_destination += lp;
                if (real_destination > 0x7fffff) return;
                if (length_remaining <= 0) return;
            }
        } else {
            UINT32 lp = process_byte(current_byte, real_destination, length_remaining);
            length_remaining -= lp; real_destination += lp;
            if (real_destination > 0x7fffff) return;
            if (length_remaining <= 0) return;
        }
    }
}

static UINT16 lastb, lastb2;
static UINT32 ProcessByte8(UINT8 b, UINT32 dst_offset) {
    UINT8 *destRAM = (UINT8 *)RamCRam;
    INT32 l = 0;
    if (lastb == lastb2) {
        INT32 rle = (b + 1) & 0xff;
        for (INT32 i = 0; i < rle; ++i) { destRAM[(dst_offset & 0x7fffff) ^ 3] = (UINT8)lastb; dst_offset++; ++l; }
        lastb2 = 0xffff;
        return l;
    }
    lastb2 = lastb; lastb = b;
    destRAM[(dst_offset & 0x7fffff) ^ 3] = b;
    return 1;
}

static void cps3_do_alt_char_dma(UINT32 src, UINT32 real_dest, UINT32 real_length) {
    UINT8 *px = RomUser;
    UINT32 start = real_dest, ds = real_dest;
    lastb = 0xfffe; lastb2 = 0xffff;
    for (;;) {
        UINT8 ctrl = px[src % cps3_data_rom_size]; ++src;
        for (INT32 i = 0; i < 8; ++i) {
            UINT8 p = px[src % cps3_data_rom_size];
            if (ctrl & 0x80) {
                p &= 0x7f;
                ds += ProcessByte8(px[(chardma_table_address + p * 2 + 0) % cps3_data_rom_size], ds);
                ds += ProcessByte8(px[(chardma_table_address + p * 2 + 1) % cps3_data_rom_size], ds);
            } else ds += ProcessByte8(p, ds);
            ++src; ctrl <<= 1;
            if ((ds - start) >= real_length) return;
        }
    }
}

static void cps3_process_character_dma(UINT32 address) {
    for (INT32 i = 0; i < 0x1000; i += 3) {
        UINT32 dat1 = RamCRam[(i + 0 + address) & 0x1fffff];
        UINT32 dat2 = RamCRam[(i + 1 + address) & 0x1fffff];
        UINT32 dat3 = RamCRam[(i + 2 + address) & 0x1fffff];
        UINT32 real_source = (dat3 << 1) - 0x400000;
        UINT32 real_destination = dat2 << 3;
        UINT32 real_length = (((dat1 & 0x001fffff) + 1) << 3);
        if (dat1 == 0x01000000) break;
        switch (dat1 & 0x00e00000) {
        case 0x00800000: chardma_table_address = real_source; Sh2SetIRQLine(10, CPU_IRQSTATUS_ACK); break;
        case 0x00400000: cps3_do_char_dma(real_source, real_destination, real_length); Sh2SetIRQLine(10, CPU_IRQSTATUS_ACK); break;
        case 0x00600000: cps3_do_alt_char_dma(real_source, real_destination, real_length); Sh2SetIRQLine(10, CPU_IRQSTATUS_ACK); break;
        case 0x00000000:
            for (UINT32 j = 0; j < real_length; j++)
                ((UINT8 *)RamCRam)[(real_destination + j) & 0x7fffff] = RomUser[((real_source + j) ^ 3) % cps3_data_rom_size];
            Sh2SetIRQLine(10, CPU_IRQSTATUS_ACK);
            break;
        }
    }
}

static INT32 MemIndex(void) {
    UINT8 *Next = Mem;
    RomBios = Next; Next += 0x0080000;
    RomUser = Next; Next += cps3_data_rom_size;
    RomGame = Next; Next += 0x1000000;
    RomGame_D = Next; Next += 0x1000000;
    RamStart = Next;
    RamC000 = Next; Next += 0x0000400;
    RamC000_D = Next; Next += 0x0000400;
    RamMain = Next; Next += 0x0080000;
    RamPal = (UINT16 *)Next; Next += 0x0020000 * sizeof(UINT16);
    RamSpr = (UINT32 *)Next; Next += (0x0080000 / 4) * sizeof(UINT32);
    SprList = (UINT32 *)Next; Next += (0x0080000 / 4) * sizeof(UINT32);
    RamCRam = (UINT32 *)Next; Next += 0x0200000 * sizeof(UINT32);
    RamSS = (UINT32 *)Next; Next += 0x0004000 * sizeof(UINT32);
    RamVReg = (UINT32 *)Next; Next += 0x0000040 * sizeof(UINT32);
    RamVRegBuf = (UINT32 *)Next; Next += 0x0000040 * sizeof(UINT32);
    RamEnd = Next;
    EEPROM = (UINT16 *)Next; Next += 0x0000100 * sizeof(UINT16);
    Pal32 = (UINT32 *)Next; Next += 0x020002 * sizeof(UINT32);
    RamScreenBase = (UINT32 *)Next; Next += (512 * 2) * (224 * 2 + 32) * sizeof(UINT32);
    MemEnd = Next;
    return 0;
}

static inline UINT32 col15(UINT16 data) {
    INT32 r = (data & 0x001F) << 3, g = (data & 0x03E0) >> 2, b = (data & 0x7C00) >> 7;
    r |= r >> 5; g |= g >> 5; b |= b >> 5;
    return 0xff000000u | (r << 16) | (g << 8) | b;
}

// ------------------------------------------------------------------ handler memoria
static UINT8 __fastcall cps3ReadByte(UINT32) { return 0; }

static UINT16 __fastcall cps3ReadWord(UINT32 addr) {
    addr &= 0xc7ffffff;
    switch (addr) {
    case 0x040c0000: case 0x040c0002: case 0x040c0004: case 0x040c0006: return 0;
    case 0x040c000c: return dma_status;
    case 0x040c000e: return 0;
    case 0x05000000: if (cps3_dbg_ioread) fprintf(stderr, "IO 05000000 pc %08x pr %08x\n", Sh2GetPC(0), 0); return ~Cps3Input[1];
    case 0x05000002: return ~Cps3Input[0];
    case 0x05000004: return ~Cps3Input[3];
    case 0x05000006: return ~Cps3Input[2];
    case 0x05140000: case 0x05140002: break;
    default:
        if ((addr >= 0x05000a00) && (addr < 0x05000a20)) return 0xffff;
        if ((addr >= 0x05001000) && (addr < 0x05001204)) {
            addr -= 0x05001000;
            if (addr >= 0x100 && addr < 0x180) cps3_current_eeprom_read = EEPROM[((addr - 0x100) >> 1) ^ 1];
            else if (addr == 0x202) return cps3_current_eeprom_read;
        }
    }
    return 0;
}

static UINT32 __fastcall cps3ReadLong(UINT32 addr) {
    addr &= 0xc7ffffff;
    if (addr == 0x04200000) return 0x0404adad;
    return 0;
}

static void __fastcall cps3WriteByte(UINT32 addr, UINT8 data) {
    addr &= 0xc7ffffff;
    switch (addr) {
    case 0x05050020: ss_bank_base = (ss_bank_base & 0x00ffffff) | (data << 24); break;
    case 0x05050021: ss_bank_base = (ss_bank_base & 0xff00ffff) | (data << 16); break;
    case 0x05050022: ss_bank_base = (ss_bank_base & 0xffff00ff) | (data << 8); break;
    case 0x05050023: ss_bank_base = (ss_bank_base & 0xffffff00) | (data << 0); break;
    case 0x05050024: ss_pal_base = (ss_pal_base & 0x00ff) | (data << 8); break;
    case 0x05050025: ss_pal_base = (ss_pal_base & 0xff00) | (data << 0); break;
    }
}

static inline UINT8 cps3_get_fade(INT32 c, INT32 f) {
    if (f & 0x40) c = (f & 0x20) ? ((((c ^ 0x1f) * (~f & 0x1f)) >> 5) ^ 0x1f) : (c * (f & 0x1f) >> 5);
    return c;
}

static void bankswitch_cram(void) {
    Sh2MapMemory(((UINT8 *)RamCRam) + (cram_bank << 20), 0x04100000, 0x041fffff, MAP_RAM_ALL);
}

static void __fastcall cps3WriteWord(UINT32 addr, UINT16 data) {
    addr &= 0xc7ffffff;
    switch (addr) {
    case 0x040c0080: break;
    case 0x040c0082:
        spritelist_dma_prev = spritelist_dma;
        spritelist_dma = data;
        if ((spritelist_dma & 9) == 8 && (spritelist_dma_prev & 9) == 9) {
            for (int i = 0; i < 0x2000 / 4; i += 4) {
                memcpy(SprList + i, RamSpr + i, 4 * sizeof(UINT32));
                UINT32 dat = RamSpr[i];
                if (dat & 0x80000000) break;
                UINT32 offs = (dat & 0x00007fff) << 2;
                UINT32 length = (dat & 0x01ff0000) >> 16;
                if (offs + length * 4 <= 0x20000) memcpy(SprList + offs, RamSpr + offs, length * 4 * sizeof(UINT32));
            }
            memcpy(RamVRegBuf, RamVReg, 0x40 * sizeof(UINT32));
        }
        break;
    case 0x040c0084: break;
    case 0x040c0086:
        if (cram_bank != (UINT32)(data & 7)) { cram_bank = data & 7; bankswitch_cram(); }
        break;
    case 0x040c0088: gfxflash_bank = data - 2; break;
    case 0x040c0096: chardma_source = data; break;
    case 0x040c0098:
        if (data & 0x0040) cps3_process_character_dma(chardma_source | ((data & 0x003f) << 16));
        break;
    case 0x040c00a0: paldma_source = (paldma_source & 0x0000ffff) | (data << 16); break;
    case 0x040c00a2: paldma_source = (paldma_source & 0xffff0000) | (data << 0); break;
    case 0x040c00a4: paldma_dest = (paldma_dest & 0x0000ffff) | (data << 16); break;
    case 0x040c00a6: paldma_dest = (paldma_dest & 0xffff0000) | (data << 0); break;
    case 0x040c00a8: paldma_fade = (paldma_fade & 0x0000ffff) | (data << 16); break;
    case 0x040c00aa: paldma_fade = (paldma_fade & 0xffff0000) | (data << 0); break;
    case 0x040c00ac: paldma_length = data; break;
    case 0x040c00ae:
        if (data & 0x0002) {
            for (UINT32 i = 0; i < paldma_length; i++) {
                UINT16 *src = (UINT16 *)RomUser;
                UINT32 si = (paldma_source - 0x200000 + i) % (cps3_data_rom_size / 2);
                UINT16 coldata = src[si];
                coldata = (coldata << 8) | (coldata >> 8);
                UINT32 r = (coldata & 0x001F) >> 0, g = (coldata & 0x03E0) >> 5, b = (coldata & 0x7C00) >> 10;
                if (paldma_fade & 0x40400040) {
                    r = cps3_get_fade(r, (paldma_fade & 0x7f000000) >> 24);
                    g = cps3_get_fade(g, (paldma_fade & 0x007f0000) >> 16);
                    b = cps3_get_fade(b, (paldma_fade & 0x0000007f) >> 0);
                    coldata = (coldata & 0x8000) | (r << 0) | (g << 5) | (b << 10);
                }
                UINT32 d = (paldma_dest + i) & 0x1ffff;
                RamPal[d ^ 1] = coldata;
                // FBNeo: colore da r,g,b << 3 senza replica dei bit bassi
                Pal32[d] = 0xff000000u | ((r << 3) << 16) | ((g << 3) << 8) | (b << 3);
            }
            dma_status |= 4;
            dma_timer = ((25000000 / 1000000) * 100);
            Sh2StopRun();
        }
        break;
    case 0x04200554: case 0x04200aaa: break;
    case 0x05050020: ss_bank_base = (ss_bank_base & 0x0000ffff) | (data << 16); break;
    case 0x05050022: ss_bank_base = (ss_bank_base & 0xffff0000) | (data << 0); break;
    case 0x05050024: ss_pal_base = data; break;
    case 0x05050026: break;
    case 0x05100000: Sh2SetIRQLine(12, CPU_IRQSTATUS_NONE); break;
    case 0x05110000: Sh2SetIRQLine(10, CPU_IRQSTATUS_NONE); break;
    case 0x05140000: case 0x05140002: break;
    default:
        if ((addr >= 0x040c0000) && (addr < 0x040c0100)) {
            addr &= 0xff;
            ((UINT16 *)RamVReg)[(addr >> 1) ^ 1] = data;
        } else if ((addr >= 0x05001000) && (addr < 0x05001204)) {
            addr -= 0x05001000;
            if ((addr >= 0x080) && (addr < 0x100)) EEPROM[((addr - 0x080) >> 1) ^ 1] = data;
        }
    }
}

static void __fastcall cps3WriteLong(UINT32, UINT32) {}

static void __fastcall cps3C0WriteByte(UINT32, UINT8) {}
static void __fastcall cps3C0WriteWord(UINT32, UINT16) {}
static void __fastcall cps3C0WriteLong(UINT32 addr, UINT32 data) {
    if (addr < 0xc0000400) {
        *(UINT32 *)(RamC000 + (addr & 0x3ff)) = data;
        *(UINT32 *)(RamC000_D + (addr & 0x3ff)) = data ^ cps3_mask(addr, cps3_key1, cps3_key2);
    }
}

static UINT8 __fastcall cps3RomReadByte(UINT32 addr) {
    addr &= 0xc7ffffff; addr ^= 0x03;
    return *(RomGame_D + (addr & 0x00ffffff));
}
static UINT16 __fastcall cps3RomReadWord(UINT32 addr) {
    addr &= 0xc7ffffff; addr ^= 0x02;
    return *(UINT16 *)(RomGame_D + (addr & 0x00ffffff));
}
static UINT32 __fastcall cps3RomReadLong(UINT32 addr) {
    addr &= 0xc7ffffff;
    UINT32 retvalue = cps3_flash_read(&main_flash, addr);
    if (main_flash.flash_mode == FM_NORMAL) retvalue = *(UINT32 *)(RomGame_D + (addr & 0x00ffffff));
    UINT32 pc = Sh2GetPC(0);
    if (pc == cps3_bios_test_hack || pc == cps3_game_test_hack) {
        if (main_flash.flash_mode == FM_NORMAL) retvalue = *(UINT32 *)(RomGame + (addr & 0x00ffffff));
    }
    return retvalue;
}
static void __fastcall cps3RomWriteByte(UINT32, UINT8) {}
static void __fastcall cps3RomWriteWord(UINT32, UINT16) {}
static void __fastcall cps3RomWriteLong(UINT32 addr, UINT32 data) {
    addr &= 0x00ffffff;
    cps3_flash_write(&main_flash, addr, data);
    if (main_flash.flash_mode == FM_NORMAL) {
        *(UINT32 *)(RomGame + addr) = data;
        *(UINT32 *)(RomGame_D + addr) = data ^ cps3_mask(addr + 0x06000000, cps3_key1, cps3_key2);
    }
}

static UINT8 __fastcall cps3VidReadByte(UINT32) { return 0; }
static UINT16 __fastcall cps3VidReadWord(UINT32) { return 0; }
static UINT32 __fastcall cps3VidReadLong(UINT32) { return 0; }
static void __fastcall cps3VidWriteByte(UINT32, UINT8) {}
static void __fastcall cps3VidWriteWord(UINT32 addr, UINT16 data) {
    addr &= 0xc7ffffff;
    if ((addr >= 0x04080000) && (addr < 0x040c0000)) {
        UINT32 palindex = (addr - 0x04080000) >> 1;
        RamPal[palindex ^ 1] = data;
        Pal32[palindex] = col15(data);
    }
}
static void __fastcall cps3VidWriteLong(UINT32, UINT32) {}

// RAM principale: lettura con speed hack (pagina dell'indirizzo) e, in debug, watch delle scritture
static inline void speedup_check(UINT32 addr) {
    if (addr == cps3_speedup_ram_address && Sh2GetPC(0) == cps3_speedup_code_address) Sh2BurnUntilInt(0);
}
unsigned int Sh2GetPR(int); unsigned int Sh2GetR(int, int);
static UINT32 rwatch_lo = 1, rwatch_hi = 0;
static inline void rwatch_hit(UINT32 addr, int sz) {
    UINT32 a = addr & 0x0207ffff;
    if (a + sz - 1 >= rwatch_lo && a <= rwatch_hi) {
        UINT32 sp = Sh2GetR(0, 15);
        fprintf(stderr, "R%d %08x pc %08x pr %08x sp", sz * 8, a, Sh2GetPC(0), Sh2GetPR(0));
        if ((sp & 0xfff80000) == 0x02000000)
            for (int k = 0; k < 16; ++k) fprintf(stderr, " %08x", *(UINT32 *)(RamMain + ((sp + k * 4) & 0x7fffc)));
        fprintf(stderr, "\n");
    }
}
static UINT8 __fastcall cps3RamReadByte(UINT32 addr) { rwatch_hit(addr, 1); speedup_check(addr); addr &= 0x7ffff; return *(RamMain + (addr ^ 0x03)); }
static UINT16 __fastcall cps3RamReadWord(UINT32 addr) { rwatch_hit(addr, 2); speedup_check(addr); addr &= 0x7ffff; return *(UINT16 *)(RamMain + (addr ^ 0x02)); }
static UINT32 __fastcall cps3RamReadLong(UINT32 addr) { rwatch_hit(addr, 4); speedup_check(addr); addr &= 0x7ffff; return *(UINT32 *)(RamMain + addr); }
unsigned int Sh2GetPR(int);
static void watch_hit(UINT32 addr, UINT32 data, int sz) {
    UINT32 a = addr & 0x0207ffff;
    if (a + sz - 1 >= watch_lo && a <= watch_hi)
        fprintf(stderr, "W%d %08x <- %0*x  pc %08x pr %08x\n", sz * 8, a, sz * 2, data, Sh2GetPC(0), Sh2GetPR(0));
}
static void __fastcall cps3RamWriteByte(UINT32 addr, UINT8 d) { watch_hit(addr, d, 1); *(RamMain + ((addr & 0x7ffff) ^ 0x03)) = d; }
static void __fastcall cps3RamWriteWord(UINT32 addr, UINT16 d) { watch_hit(addr, d, 2); *(UINT16 *)(RamMain + ((addr & 0x7ffff) ^ 0x02)) = d; }
static void __fastcall cps3RamWriteLong(UINT32 addr, UINT32 d) { watch_hit(addr, d, 4); *(UINT32 *)(RamMain + (addr & 0x7ffff)) = d; }

static void map_ram(void) {
    Sh2MapMemory(RamMain, 0x02000000, 0x0207ffff, MAP_RAM_ALL);
    if (watch_lo <= watch_hi || rwatch_lo <= rwatch_hi) {
        Sh2MapHandler(5, 0x02000000, 0x0207ffff, MAP_READ | MAP_WRITE);
    } else {
        Sh2MapHandler(5, 0x02000000 | (cps3_speedup_ram_address & 0x030000),
                         0x0200ffff | (cps3_speedup_ram_address & 0x030000), MAP_READ);
    }
    Sh2SetReadByteHandler(5, cps3RamReadByte);
    Sh2SetReadWordHandler(5, cps3RamReadWord);
    Sh2SetReadLongHandler(5, cps3RamReadLong);
    Sh2SetWriteByteHandler(5, cps3RamWriteByte);
    Sh2SetWriteWordHandler(5, cps3RamWriteWord);
    Sh2SetWriteLongHandler(5, cps3RamWriteLong);
}

extern "C" void cps3_set_rwatch(uint32_t lo, uint32_t hi) { rwatch_lo = lo; rwatch_hi = hi; if (loaded) map_ram(); }
extern "C" void cps3_set_watch(uint32_t lo, uint32_t hi) {
    watch_lo = lo; watch_hi = hi;
    if (loaded) map_ram();
}

// ------------------------------------------------------------------ audio (16 voci PCM 8 bit)
#define CPS3_VOICES 16
typedef struct { UINT16 regs[16]; UINT32 pos; UINT16 frac; } cps3_voice;
static struct { cps3_voice voice[CPS3_VOICES]; UINT16 key; } snd;
static INT16 abuf[2 * 4096];
static size_t asamps;
static double aacc;

static UINT8 __fastcall cps3SndReadByte(UINT32) { return 0; }
static UINT16 __fastcall cps3SndReadWord(UINT32 addr) {
    addr &= 0x000003ff;
    if (addr < 0x200) return snd.voice[addr >> 5].regs[(addr >> 1) & 0xf];
    if (addr == 0x200) return snd.key;
    return 0;
}
static UINT32 __fastcall cps3SndReadLong(UINT32) { return 0; }
static void __fastcall cps3SndWriteByte(UINT32, UINT8) {}
static void __fastcall cps3SndWriteWord(UINT32 addr, UINT16 data) {
    addr &= 0x000003ff;
    if (addr < 0x200) snd.voice[addr >> 5].regs[(addr >> 1) & 0xf] = data;
    else if (addr == 0x200) {
        for (INT32 i = 0; i < CPS3_VOICES; i++)
            if ((data & (1 << i)) && !(snd.key & (1 << i))) { snd.voice[i].frac = 0; snd.voice[i].pos = 0; }
        snd.key = data;
    }
}
static void __fastcall cps3SndWriteLong(UINT32, UINT32) {}

static inline INT16 clip16(INT32 v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : (INT16)v; }

static void snd_update(void) {
    // campioni del fotogramma alla frequenza nativa (resto frazionario conservato)
    aacc += CPS3_AUDIO_RATE / CPS3_FPS;
    int n = (int)aacc;
    aacc -= n;
    if (n > 4096) n = 4096;
    memset(abuf, 0, n * 2 * sizeof(INT16));
    INT8 *base = (INT8 *)RomUser;
    for (INT32 i = 0; i < CPS3_VOICES; i++) {
        cps3_voice *v = &snd.voice[i];
        if (!(snd.key & (1 << i))) continue;
        UINT32 start = ((v->regs[3] << 16) | v->regs[2]) - 0x400000;
        UINT32 end = ((v->regs[11] << 16) | v->regs[10]) - 0x400000;
        UINT32 loop = ((v->regs[9] << 16) | v->regs[7]) - 0x400000;
        UINT32 step = v->regs[6];
        INT32 vol_l = (INT16)v->regs[15], vol_r = (INT16)v->regs[14];
        UINT32 pos = v->pos, frac = v->frac;
        INT16 *b = abuf;
        for (INT32 j = 0; j < n; j++) {
            pos += (frac >> 12);
            frac &= 0xfff;
            if (start + pos >= end) {
                if (v->regs[5]) pos = loop - start;
                else break;
            }
            INT32 sample = base[((start + pos) ^ 1) % cps3_data_rom_size];
            frac += step;
            // come FBNeo: canale 0 = volume reg 14, canale 1 = volume reg 15
            b[0] = clip16(b[0] + ((sample * vol_r) >> 8));
            b[1] = clip16(b[1] + ((sample * vol_l) >> 8));
            b += 2;
        }
        v->pos = pos; v->frac = (UINT16)frac;
    }
    asamps = n;
}

extern "C" const int16_t *cps3_audio(size_t *frames) { *frames = asamps; return abuf; }


// debug: letture della BIOS nella pagina 0x10000-0x1ffff (indirizzo cps3_dbg_bioswatch +-0x40) con PC
static void bw_log(UINT32 a, int sz, UINT32 v) {
    UINT32 w = cps3_dbg_bioswatch;
    if (a + sz > w - 0x40 && a < w + 0x40) fprintf(stderr, "BIOSRD %08x/%d = %x pc %08x\n", a, sz, v, Sh2GetPC(0));
}
static UINT8 __fastcall bwReadByte(UINT32 a) { a &= 0x7ffff; UINT8 v = RomBios[a ^ 3]; bw_log(a, 1, v); return v; }
static UINT16 __fastcall bwReadWord(UINT32 a) { a &= 0x7ffff; UINT16 v = *(UINT16 *)(RomBios + (a ^ 2)); bw_log(a, 2, v); return v; }
static UINT32 __fastcall bwReadLong(UINT32 a) { a &= 0x7ffff; UINT32 v = *(UINT32 *)(RomBios + a); bw_log(a, 4, v); return v; }

// ------------------------------------------------------------------ regione e reset
static void Cps3PatchRegion(void) {
    if (cps3_region_address)
        RomBios[cps3_region_address] = (RomBios[cps3_region_address] & 0xf0) | (cps3_dip & 0x7f);
}

static void Cps3Reset(void) {
    memset(RamStart, 0, RamEnd - RamStart);
    cram_bank = 0;
    bankswitch_cram();
    Cps3PatchRegion();
    // avvio veloce come FBNeo (senza BIOS/CD): PC e SP dal programma decifrato
    Sh2Reset(*(UINT32 *)(RomGame_D + 0), *(UINT32 *)(RomGame_D + 4));
    Sh2SetVBR(0x06000000);
    ss_bank_base = ss_pal_base = 0;
    cps3_current_eeprom_read = 0;
    gfxflash_bank = 0;
    paldma_source = paldma_dest = paldma_fade = paldma_length = 0;
    chardma_source = chardma_table_address = 0;
    dma_timer = -1; dma_status = 0;
    spritelist_dma = spritelist_dma_prev = 0;
    cps_int10_cnt = 0;
    memset(&snd, 0, sizeof snd);
    nExtraCycles = 0;
    SprList[0] = 0x80000000;
    memset(&main_flash, 0, sizeof main_flash);   // come FBNeo (cps3_flash_init non viene chiamata)
    for (int i = 0; i < 0x20000; ++i) Pal32[i] = col15(RamPal[i ^ 1]);
}

extern "C" void cps3_reset(void) { if (loaded) Cps3Reset(); }

// ------------------------------------------------------------------ caricamento
static void be_to_le(UINT8 *p, INT32 size) {
    for (INT32 i = 0; i < size; i += 4, p += 4) {
        UINT8 c = p[0]; p[0] = p[3]; p[3] = c;
        c = p[1]; p[1] = p[2]; p[2] = c;
    }
}

// file della zip il cui nome termina con "suffix" (es. "-simm1.0" o ".29f400.u2")
static void *zip_get(mz_zip_archive *z, const char *suffix, size_t *sz) {
    int n = (int)mz_zip_reader_get_num_files(z);
    size_t sl = strlen(suffix);
    for (int i = 0; i < n; ++i) {
        char name[512];
        mz_zip_reader_get_filename(z, i, name, sizeof name);
        size_t nl = strlen(name);
        if (nl >= sl && !strcmp(name + nl - sl, suffix))
            return mz_zip_reader_extract_to_heap(z, i, sz, 0);
    }
    return NULL;
}

extern "C" int cps3_load_zip(const char *zip_path, const char *game, int region) {
    cps3_unload();
    int nsimm[6] = { 0 };
    if (!strcmp(game, "redearth")) {
        cps3_key1 = 0x9e300ab1; cps3_key2 = 0xa175b82c;
        cps3_bios_test_hack = 0x00016530; cps3_game_test_hack = 0x060105f0;
        cps3_speedup_ram_address = 0x0202136c; cps3_speedup_code_address = 0x0600194e;
        cps3_region_address = 0x0001fed8;
        nsimm[1] = 4; nsimm[2] = 0; nsimm[3] = 8; nsimm[4] = 8; nsimm[5] = 2;
        is_jojo = 0;
    } else if (!strcmp(game, "jojoba")) {
        cps3_key1 = 0x23323ee3; cps3_key2 = 0x03021972;
        cps3_bios_test_hack = 0x00011c90; cps3_game_test_hack = 0x061c45bc;
        cps3_speedup_ram_address = 0x020267dc; cps3_speedup_code_address = 0x0600065c;
        cps3_region_address = 0x0001fec8;
        nsimm[1] = 4; nsimm[2] = 4; nsimm[3] = 8; nsimm[4] = 8; nsimm[5] = 8;
        is_jojo = 1;
    } else return 0;
    cps3_dip = (UINT8)region;
    cps3_data_rom_size = (nsimm[3] + nsimm[4] + nsimm[5]) * 0x200000;

    mz_zip_archive z; memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_file(&z, zip_path, 0)) return 0;
    MemIndex();
    size_t len = MemEnd - (UINT8 *)0;
    Mem = (UINT8 *)calloc(1, len);
    if (!Mem) { mz_zip_reader_end(&z); return 0; }
    MemIndex();
    RamScreen = RamScreenBase + (512 * 2) * 16 + 16;
    int ok = 1;
    size_t sz;
    void *b = zip_get(&z, ".29f400.u2", &sz);
    if (!b || sz != 0x80000) ok = 0; else { memcpy(RomBios, b, 0x80000); }
    free(b);
    // programma: SIMM 1 e 2, quattro chip interlacciati a byte
    for (int s = 1; s <= 2 && ok; ++s) {
        for (int c = 0; c < nsimm[s] && ok; ++c) {
            char suf[32]; snprintf(suf, sizeof suf, "-simm%d.%d", s, c);
            UINT8 *d = (UINT8 *)zip_get(&z, suf, &sz);
            if (!d || sz != 0x200000) { ok = 0; free(d); break; }
            UINT8 *dst = RomGame + (s - 1) * 0x800000 + c;
            for (size_t i = 0; i < sz; ++i) dst[i * 4] = d[i];
            free(d);
        }
    }
    // grafica/audio: SIMM 3-5, coppie interlacciate a byte
    UINT32 off = 0;
    for (int s = 3; s <= 5 && ok; ++s) {
        for (int c = 0; c < nsimm[s] && ok; c += 2) {
            for (int k = 0; k < 2; ++k) {
                char suf[32]; snprintf(suf, sizeof suf, "-simm%d.%d", s, c + k);
                UINT8 *d = (UINT8 *)zip_get(&z, suf, &sz);
                if (!d || sz != 0x200000) { ok = 0; free(d); break; }
                UINT8 *dst = RomUser + off + k;
                for (size_t i = 0; i < sz; ++i) dst[i * 2] = d[i];
                free(d);
            }
            off += 0x400000;
        }
    }
    mz_zip_reader_end(&z);
    if (!ok) { free(Mem); Mem = NULL; return 0; }

    be_to_le(RomBios, 0x080000);
    cps3_decrypt_bios();
    be_to_le(RomGame, 0x1000000);
    cps3_decrypt_game();

    Sh2Init(1);
    Sh2Open(0);
    cps3speedhack = 1;
    Sh2MapMemory(RomBios, 0x00000000, 0x0007ffff, MAP_ROM);
    Sh2MapMemory((UINT8 *)RamSpr, 0x04000000, 0x0407ffff, MAP_RAM_ALL);
    Sh2MapMemory((UINT8 *)RamSS, 0x05040000, 0x0504ffff, MAP_RAM_ALL);
    Sh2SetReadByteHandler(0, cps3ReadByte);
    Sh2SetReadWordHandler(0, cps3ReadWord);
    Sh2SetReadLongHandler(0, cps3ReadLong);
    Sh2SetWriteByteHandler(0, cps3WriteByte);
    Sh2SetWriteWordHandler(0, cps3WriteWord);
    Sh2SetWriteLongHandler(0, cps3WriteLong);
    Sh2MapMemory(RamC000_D, 0xc0000000, 0xc00003ff, MAP_FETCH);
    Sh2MapMemory(RamC000, 0xc0000000, 0xc00003ff, MAP_READ);
    Sh2MapHandler(1, 0xc0000000, 0xc00003ff, MAP_WRITE);
    Sh2SetWriteByteHandler(1, cps3C0WriteByte);
    Sh2SetWriteWordHandler(1, cps3C0WriteWord);
    Sh2SetWriteLongHandler(1, cps3C0WriteLong);
    Sh2MapMemory(RomGame_D, 0x06000000, 0x06ffffff, MAP_FETCH);
    Sh2MapHandler(2, 0x06000000, 0x06ffffff, MAP_READ | MAP_WRITE);
    Sh2SetReadByteHandler(2, cps3RomReadByte);
    Sh2SetReadWordHandler(2, cps3RomReadWord);
    Sh2SetReadLongHandler(2, cps3RomReadLong);
    Sh2SetWriteByteHandler(2, cps3RomWriteByte);
    Sh2SetWriteWordHandler(2, cps3RomWriteWord);
    Sh2SetWriteLongHandler(2, cps3RomWriteLong);
    Sh2MapHandler(3, 0x040e0000, 0x040e02ff, MAP_RAM_ALL);
    Sh2SetReadByteHandler(3, cps3SndReadByte);
    Sh2SetReadWordHandler(3, cps3SndReadWord);
    Sh2SetReadLongHandler(3, cps3SndReadLong);
    Sh2SetWriteByteHandler(3, cps3SndWriteByte);
    Sh2SetWriteWordHandler(3, cps3SndWriteWord);
    Sh2SetWriteLongHandler(3, cps3SndWriteLong);
    Sh2MapMemory((UINT8 *)RamPal, 0x04080000, 0x040bffff, MAP_READ);
    Sh2MapHandler(4, 0x04080000, 0x040bffff, MAP_WRITE);
    Sh2SetReadByteHandler(4, cps3VidReadByte);
    Sh2SetReadWordHandler(4, cps3VidReadWord);
    Sh2SetReadLongHandler(4, cps3VidReadLong);
    Sh2SetWriteByteHandler(4, cps3VidWriteByte);
    Sh2SetWriteWordHandler(4, cps3VidWriteWord);
    Sh2SetWriteLongHandler(4, cps3VidWriteLong);
    if (cps3_dbg_bioswatch) {
        Sh2MapHandler(6, 0x00010000, 0x0001ffff, MAP_READ);
        Sh2SetReadByteHandler(6, bwReadByte);
        Sh2SetReadWordHandler(6, bwReadWord);
        Sh2SetReadLongHandler(6, bwReadLong);
    }
    loaded = 1;
    map_ram();
    aacc = 0;
    Cps3Reset();
    return 1;
}

extern "C" void cps3_unload(void) {
    if (!loaded) return;
    Sh2Exit();
    free(Mem); Mem = NULL;
    loaded = 0;
}

// ------------------------------------------------------------------ ingressi
static unsigned in_joy[2], in_sys[2];
extern "C" void cps3_set_input(int port, unsigned joy, unsigned sys) { in_joy[port & 1] = joy; in_sys[port & 1] = sys; }

static void build_inputs(void) {
    UINT16 i0 = 0, i1 = 0, i3 = 0;
    for (int p = 0; p < 2; ++p) {
        unsigned j = in_joy[p], s = in_sys[p];
        if ((j & (C3_UP | C3_DOWN)) == (C3_UP | C3_DOWN)) j &= ~(C3_UP | C3_DOWN);
        if ((j & (C3_LEFT | C3_RIGHT)) == (C3_LEFT | C3_RIGHT)) j &= ~(C3_LEFT | C3_RIGHT);
        int sh = p ? 8 : 0;
        if (j & C3_UP) i0 |= 1 << (0 + sh);
        if (j & C3_DOWN) i0 |= 1 << (1 + sh);
        if (j & C3_LEFT) i0 |= 1 << (2 + sh);
        if (j & C3_RIGHT) i0 |= 1 << (3 + sh);
        if (j & C3_B1) i0 |= 1 << (4 + sh);
        if (j & C3_B2) i0 |= 1 << (5 + sh);
        if (j & C3_B3) i0 |= 1 << (6 + sh);
        if (!p) {
            if (j & C3_B4) i3 |= 1 << 3;
            if (j & C3_B5) i3 |= 1 << 2;
            if (j & C3_B6) i3 |= 1 << 1;
            if (s & C3_COIN) i1 |= 1 << 8;
            if (s & C3_START) i1 |= 1 << 12;
            if (s & C3_SERVICE) i1 |= 1 << 0;
            if (s & C3_TEST) i1 |= 1 << 1;
        } else {
            if (j & C3_B4) i3 |= 1 << 4;
            if (j & C3_B5) i3 |= 1 << 5;
            if (j & C3_B6) i1 |= 1 << 10;
            if (s & C3_COIN) i1 |= 1 << 9;
            if (s & C3_START) i1 |= 1 << 13;
        }
    }
    Cps3Input[0] = i0; Cps3Input[1] = i1; Cps3Input[3] = i3;
}

extern "C" void cps3_set_overclock(int percent) { overclock_pct = percent < 50 ? 50 : percent > 400 ? 400 : percent; }

// ------------------------------------------------------------------ fotogramma
static void hd_render(void);
static uint32_t *hd_out; static unsigned hd_w, hd_h;

extern "C" void cps3_frame(int render) {
    if (!loaded) return;
    build_inputs();
    Sh2NewFrame();
    const INT32 nInterleave = 4;
    INT32 nCyclesTotal = (INT32)((double)25000000 * 100 / 5959 * overclock_pct / 100.0);   // come FBNeo (nBurnFPS 5959)
    Sh2Idle(nExtraCycles);
    for (INT32 i = 0; i < nInterleave; i++) {
        do {
            if (dma_timer > 0) {
                Sh2Run(dma_timer);
                dma_timer = -1;
                dma_status &= ~6;
                Sh2SetIRQLine(10, CPU_IRQSTATUS_ACK);
            }
            Sh2Run(((i + 1) * nCyclesTotal / nInterleave) - Sh2TotalCycles());
        } while (dma_timer != -1);
        if (cps_int10_cnt >= 2) { cps_int10_cnt = 0; Sh2SetIRQLine(10, CPU_IRQSTATUS_ACK); }
        else cps_int10_cnt++;
    }
    Sh2SetIRQLine(12, CPU_IRQSTATUS_ACK);
    nExtraCycles = Sh2TotalCycles() - nCyclesTotal;
    snd_update();
    if (render && hd_out) hd_render();
}

// ------------------------------------------------------------------ accesso per il front-end
extern "C" uint8_t cps3_read8(uint32_t addr) {
    if (!loaded) return 0;
    if ((addr & 0xfff80000) == 0x02000000) return RamMain[(addr & 0x7ffff) ^ 3];
    return 0;
}
extern "C" void cps3_write8(uint32_t addr, uint8_t v) {
    if (loaded && (addr & 0xfff80000) == 0x02000000) RamMain[(addr & 0x7ffff) ^ 3] = v;
}
extern "C" uint16_t *cps3_eeprom(void) { return EEPROM; }
extern "C" uint16_t cps3_eeprom_rd(int w) { return EEPROM ? EEPROM[(w & 0xff) ^ 1] : 0; }
extern "C" void cps3_eeprom_wr(int w, uint16_t v) { if (EEPROM) EEPROM[(w & 0xff) ^ 1] = v; }
extern "C" unsigned cps3_pc(void) { return loaded ? Sh2GetPC(0) : 0; }
extern "C" uint32_t cps3_vreg(int i) { return loaded ? RamVRegBuf[i & 0x3f] : 0; }
extern "C" const uint32_t *cps3_spritelist(void) { return SprList; }
extern "C" const uint32_t *cps3_palette32(void) { return Pal32; }

// ------------------------------------------------------------------ stato
static UINT8 *st_ptr; static size_t st_pos, st_cap; static int st_mode; // 0 misura, 1 salva, 2 carica
static INT32 st_acb(struct BurnArea *ba) {
    if (st_mode == 1 && st_pos + ba->nLen <= st_cap) memcpy(st_ptr + st_pos, ba->Data, ba->nLen);
    if (st_mode == 2 && st_pos + ba->nLen <= st_cap) memcpy(ba->Data, st_ptr + st_pos, ba->nLen);
    st_pos += ba->nLen;
    return 0;
}
static void st_area(void *d, size_t n) { struct BurnArea ba; ba.Data = d; ba.nLen = (UINT32)n; ba.nAddress = 0; ba.szName = ""; st_acb(&ba); }
static void st_scan(void) {
    BurnAcb = st_acb;
    st_area(EEPROM, 0x100 * sizeof(UINT16));
    st_area(RamMain, 0x80000);
    st_area(RamSpr, 0x80000);
    st_area(SprList, 0x80000);
    st_area(RamSS, 0x4000 * 4);
    st_area(RamVReg, 0x40 * 4);
    st_area(RamVRegBuf, 0x40 * 4);
    st_area(RamC000, 0x800);
    st_area(RamPal, 0x20000 * 2);
    st_area(RamCRam, 0x200000 * 4);
    Sh2Scan(ACB_DRIVER_DATA | (st_mode == 2 ? ACB_WRITE : ACB_READ));
    st_area(&snd, sizeof snd);
    SCAN_VAR(ss_bank_base); SCAN_VAR(ss_pal_base); SCAN_VAR(cram_bank);
    SCAN_VAR(cps3_current_eeprom_read); SCAN_VAR(gfxflash_bank);
    SCAN_VAR(paldma_source); SCAN_VAR(paldma_dest); SCAN_VAR(paldma_fade); SCAN_VAR(paldma_length);
    SCAN_VAR(chardma_source); SCAN_VAR(chardma_table_address);
    SCAN_VAR(spritelist_dma); SCAN_VAR(spritelist_dma_prev);
    SCAN_VAR(dma_status); SCAN_VAR(dma_timer); SCAN_VAR(main_flash);
    SCAN_VAR(last_normal_byte); SCAN_VAR(lastb); SCAN_VAR(lastb2);
    SCAN_VAR(cps_int10_cnt); SCAN_VAR(nExtraCycles);
}
extern "C" size_t cps3_state_size(void) { if (!loaded) return 0; st_mode = 0; st_pos = 0; st_scan(); return st_pos; }
extern "C" int cps3_state_save(void *buf, size_t sz) {
    if (!loaded) return 0;
    st_mode = 1; st_ptr = (UINT8 *)buf; st_cap = sz; st_pos = 0; st_scan();
    return st_pos <= sz;
}
extern "C" int cps3_state_load(const void *buf, size_t sz) {
    if (!loaded) return 0;
    st_mode = 2; st_ptr = (UINT8 *)buf; st_cap = sz; st_pos = 0; st_scan();
    for (int i = 0; i < 0x20000; ++i) Pal32[i] = col15(RamPal[i ^ 1]);
    bankswitch_cram();
    return st_pos <= sz;
}

#include "cps3_video.inc"
