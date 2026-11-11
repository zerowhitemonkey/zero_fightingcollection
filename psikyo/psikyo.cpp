/**********************************************************************
 SNKHD - core Psikyo PS3-V1 (The Fallen Angels / Daraku Tenshi)

 Fonti: driver FBNeo "psikyosh" (d_psikyosh.cpp, licenza FBNeo non
 commerciale; caricamento ROM, mappa memoria PS3-V1, EEPROM) e driver MAME
 "psikyosh" (psikyosh.cpp/psikyosh_v.cpp, BSD-3; video con effetti per riga,
 banco pre/post-lineblend da vidregs[7]: Daraku li usa, FBNeo no).
 Audio YMF278B (OPL4) con ymfm di Aaron Giles (BSD-3, psikyo/ymfm/).
 EEPROM 93C56 come eeprom.cpp di FBNeo (MAME storico).

 Hardware: SH-2 28,63635 MHz, YMF278B 28,63635 MHz (IRQ 12 dai timer),
 vblank IRQ 4, 320x224 a 60 Hz.

 Modifiche SNKHD: caricamento diretto dello zip (miniz), overclock SH-2
 (cicli per frame; timer e audio restano in tempo reale), renderer HD
 (psikyo_video.inc), API per l'app e per il banco di prova.
**********************************************************************/
#include "burnint.h"
#include "sh2_intf.h"
#include "psikyo.h"
#include <miniz.h>
#include "ymfm/ymfm_opl.h"
#include <vector>

// BurnAcb e' definito in cps3/cps3.cpp (stessa libreria)

// ------------------------------------------------------------------ memoria
static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;
static UINT8 *RomSh2;        // 0x200000: [0x000000] prog_l/prog_h, [0x100000] prog.u16 (a 0x02000000)
static UINT8 *RomTiles;      // grafica 0x3400000 + banco vuoto 0x20000
static UINT8 *RomSnd;        // 0x400000 campioni PCM (YMF278B)
static UINT32 *RamZoom;      // 0x10000: tabella zoom (0x000-0x1ff) + copia dei registri video (0xffe0)
static UINT32 *RamPal;       // 0x10000: palette (0x1400 voci RGBx)
static UINT32 *RamSpr;       // 0x10000: 32 banchi da 0x800 (sprite, lista a 0x3800, tilemap, registri per riga)
static UINT32 *VidRegs;      // 0x20 byte usati (8 long)
static UINT8  *RamSh2;       // 0x100000 RAM di lavoro (0x06000000)
static UINT32 *RamSprBuf;    // 0x4000: sprite + lista catturati al vblank (renderer: frame precedente)
static UINT32 *Pal32;        // palette convertita 0x00RRGGBB (0x1400)
static UINT8  *TransTab;     // tile vuote: 0x10000 (4bpp) + 0x8000 (8bpp)

static UINT32 gfx_size;
static int overclock_pct = 100;
static int loaded;
static INT32 nExtraCycles;
static UINT8 dip_region = 1;          // DIP B bit 0: 0 Japan, 1 World
static INT32 prev_bank = -1;
static UINT32 watch_lo = 1, watch_hi = 0;
// speed hack del ciclo di attesa (vedi idleReadLong)
static INT64 cpu_clk_base;          // cicli SH-2 assoluti all'inizio del frame
static INT64 idle_t0 = -1;          // istante della sospensione (-1 = CPU non sospesa)
#define IDLE_ITER 10                // cicli per giro del ciclo di attesa
static inline INT64 cpu_now(void) { return cpu_clk_base + Sh2TotalCycles(); }
static void idle_wake(void) {       // prima di ogni IRQ: giri del ciclo nel tempo sospeso
    if (idle_t0 < 0) return;
    INT64 n = (cpu_now() - idle_t0) / IDLE_ITER;
    if (n > 0) *(UINT32 *)(RamSh2 + 0x0c) += (UINT32)n;
    idle_t0 = -1;
}

#define CPU_RATE      28636350
#define FRAME_CLOCKS  ((INT64)CPU_RATE * 100 / 6000)       // 477272, come FBNeo (refresh 60,00 Hz)

extern "C" const uint8_t *psikyo_prog(void) { return RomSh2; }

// patch di una parola del codice: dopo gli scambi di caricamento la parola SH-2 all'indirizzo A sta in
// *(UINT16 *)(rom + (A ^ 2)) (lettura host little-endian, come cpu_readop16 del core SH-2)
extern "C" int psikyo_patch16(uint32_t addr, uint16_t expect, uint16_t val) {
    if (!RomSh2 || (addr & 1)) return 0;
    UINT8 *base;
    if (addr < 0x00100000) base = RomSh2 + addr;
    else if (addr >= 0x02000000 && addr < 0x02100000) base = RomSh2 + 0x100000 + (addr - 0x02000000);
    else return 0;
    UINT16 *p = (UINT16 *)(RomSh2 + ((base - RomSh2) ^ 2));
    if (*p != expect && *p != val) return 0;
    *p = val;
    return 1;
}

// ------------------------------------------------------------------ EEPROM 93C56 (eeprom.cpp di FBNeo)
// 8 bit indirizzo, 8 bit dati; read "*110x", write "*101x", erase "*111x", lock/unlock; multi read
static struct {
    int serial_count; char serial_buffer[40];
    UINT8 data[256];
    int data_bits, read_address, clock_count;
    int latch, reset_line, clock_line, sending, locked, reset_delay;
} ee;

static int ee_match(const char *buf, const char *cmd, int len) {
    if (!cmd || !len) return 0;
    while (len > 0) {
        char b = *buf, c = *cmd;
        if (!b || !c) return b == c;
        switch (c) {
        case '0': case '1':
            if (b != c) return 0;
            /* fallthrough */
        case 'X': case 'x':
            buf++; len--; cmd++;
            break;
        case '*':
            c = cmd[1];
            if (c == '0' || c == '1') { if (b == c) cmd++; else { buf++; len--; } }
            else return 0;
            break;
        default: return 0;
        }
    }
    return *cmd == 0;
}
static int ee_bits(int from, int n) {
    int v = 0;
    for (int i = from; i < from + n; i++) v = (v << 1) | (ee.serial_buffer[i] == '1');
    return v;
}
static void ee_write(int bit) {
    if (ee.serial_count >= 39) return;
    ee.serial_buffer[ee.serial_count++] = bit ? '1' : '0';
    ee.serial_buffer[ee.serial_count] = 0;
    int n = ee.serial_count, len = (int)strlen(ee.serial_buffer);
    if (n > 8 && ee_match(ee.serial_buffer, "*110x", len - 8)) {
        int a = ee_bits(n - 8, 8);
        ee.data_bits = ee.data[a]; ee.read_address = a; ee.clock_count = 0; ee.sending = 1; ee.serial_count = 0;
    } else if (n > 8 && ee_match(ee.serial_buffer, "*111x", len - 8)) {
        int a = ee_bits(n - 8, 8);
        if (!ee.locked) ee.data[a] = 0xff;
        ee.serial_count = 0;
    } else if (n > 16 && ee_match(ee.serial_buffer, "*101x", len - 16)) {
        int a = ee_bits(n - 16, 8), d = ee_bits(n - 8, 8);
        if (!ee.locked) ee.data[a] = (UINT8)d;
        ee.serial_count = 0;
    } else if (ee_match(ee.serial_buffer, "*10000xxxxxxx", len)) {
        ee.locked = 1; ee.serial_count = 0;
    } else if (ee_match(ee.serial_buffer, "*10011xxxxxxx", len)) {
        ee.locked = 0; ee.serial_count = 0;
    }
}
static void ee_reset(void) { ee.serial_count = 0; ee.sending = 0; ee.reset_delay = 0; }
static int ee_read(void) {
    if (ee.sending) return (ee.data_bits >> 8) & 1;
    if (ee.reset_delay > 0) { ee.reset_delay--; return 0; }
    return 1;
}
// EEPROMWrite(clock, cs, bit) di FBNeo: cs attivo = linea di reset CLEAR
static void ee_lines(int clk, int cs, int bit) {
    ee.latch = bit ? 1 : 0;
    ee.reset_line = cs ? 0 : 1;
    if (ee.reset_line) ee_reset();
    int state = clk ? 1 : 0;
    if (ee.clock_line == 0 && state) {
        if (ee.reset_line == 0) {
            if (ee.sending) {
                if (ee.clock_count == 8) {
                    ee.read_address = (ee.read_address + 1) & 0xff;
                    ee.data_bits = ee.data[ee.read_address];
                    ee.clock_count = 0;
                }
                ee.data_bits = (ee.data_bits << 1) | 1;
                ee.clock_count++;
            } else ee_write(ee.latch);
        }
    }
    ee.clock_line = state;
}
extern "C" uint16_t psikyo_eeprom_rd(int a) { return ee.data[a & 0xff]; }
extern "C" void psikyo_eeprom_wr(int a, uint16_t v) { ee.data[a & 0xff] = (UINT8)v; }

// ------------------------------------------------------------------ YMF278B (ymfm) + temporizzazione
// Tempo del chip in clock (28,63635 MHz = clock della CPU a 100%): base del frame + cicli SH-2
// eseguiti nel frame riportati a 100% (con l'overclock la CPU fa piu' cicli nello stesso tempo).
static INT64 frame_clk_base;
static inline INT64 ymf_now(void) {
    return frame_clk_base + (INT64)Sh2TotalCycles() * 100 / overclock_pct;
}

struct PsikyoYmf : public ymfm::ymfm_interface {
    INT64 timer_end[2] = { -1, -1 };
    INT64 busy_end = 0;
    int irq = 0;
    void ymfm_set_timer(uint32_t tnum, int32_t clocks) override { timer_end[tnum & 1] = clocks < 0 ? -1 : ymf_now() + clocks; }
    void ymfm_set_busy_end(uint32_t clocks) override { busy_end = ymf_now() + clocks; }
    bool ymfm_is_busy() override { return ymf_now() < busy_end; }
    void ymfm_update_irq(bool asserted) override {
        irq = asserted;
        if (asserted) idle_wake();
        Sh2SetIRQLine(12, asserted ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
    }
    uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override {
        return (type == ymfm::ACCESS_PCM && RomSnd) ? RomSnd[address & 0x3fffff] : 0;
    }
    void expire(uint32_t t) { timer_end[t] = -1; m_engine->engine_timer_expired(t); }
};
static PsikyoYmf *ymf_if;
static ymfm::ymf278b *ymf;

// audio: un campione ogni 768 clock (37286,9 Hz), generato fino al tempo corrente a ogni scrittura
#define ABUF_MAX 4096
static INT16 abuf[2 * ABUF_MAX];
static size_t asamps;
static INT64 gen_clk;
static void ymf_sync(void) {
    if (!ymf) return;
    INT64 now = ymf_now();
    while (gen_clk + 768 <= now) {
        ymfm::ymf278b::output_data o;
        ymf->generate(&o, 1);
        if (asamps < ABUF_MAX) {
            // uscita DO2: FM 0/1 + PCM 0/1 miscelati dal chip (stereo)
            abuf[asamps * 2 + 0] = (INT16)o.data[4];
            abuf[asamps * 2 + 1] = (INT16)o.data[5];
            asamps++;
        }
        gen_clk += 768;
    }
}
extern "C" const int16_t *psikyo_audio(size_t *frames) { *frames = asamps; return abuf; }

// ------------------------------------------------------------------ banco grafica per i test ROM del gioco
static void graphics_bank(void) {
    INT32 bank = (INT32)((VidRegs[4] & 0x1ff) * 0x20000u);
    if (bank == prev_bank) return;
    prev_bank = bank;
    if (bank < 0 || (UINT32)bank >= gfx_size) bank = (INT32)gfx_size;   // banco vuoto
    Sh2MapMemory(RomTiles + bank, 0x03060000, 0x0307ffff, MAP_ROM);
    Sh2MapMemory(RomTiles + bank, 0x04060000, 0x0407ffff, MAP_ROM);
}

// ------------------------------------------------------------------ ingressi
static unsigned in_joy[2], in_sys[2];
extern "C" void psikyo_set_input(int port, unsigned joy, unsigned sys) { in_joy[port & 1] = joy; in_sys[port & 1] = sys; }
// bit attivi bassi come Common4ButtonInputList di FBNeo (DrvJoy1[n])
static UINT32 build_inputs(void) {
    UINT32 b = 0;
    unsigned j0 = in_joy[0], s0 = in_sys[0], j1 = in_joy[1], s1 = in_sys[1];
    if (s0 & PSK_COIN) b |= 1u << 0;
    if (s1 & PSK_COIN) b |= 1u << 1;
    if (s0 & PSK_SERVICE) b |= 1u << 4;
    if (s0 & PSK_TEST) b |= 1u << 5;     // service mode (DIP A 0x20, attivo basso)
    if (j1 & PSK_B4) b |= 1u << 10;
    if (j1 & PSK_B3) b |= 1u << 11;
    if (j0 & PSK_B4) b |= 1u << 14;
    if (j0 & PSK_B3) b |= 1u << 15;
    if (s1 & PSK_START) b |= 1u << 16;
    if (j1 & PSK_B2) b |= 1u << 18;
    if (j1 & PSK_B1) b |= 1u << 19;
    if (j1 & PSK_LEFT) b |= 1u << 20;
    if (j1 & PSK_RIGHT) b |= 1u << 21;
    if (j1 & PSK_DOWN) b |= 1u << 22;
    if (j1 & PSK_UP) b |= 1u << 23;
    if (s0 & PSK_START) b |= 1u << 24;
    if (j0 & PSK_B2) b |= 1u << 26;
    if (j0 & PSK_B1) b |= 1u << 27;
    if (j0 & PSK_LEFT) b |= 1u << 28;
    if (j0 & PSK_RIGHT) b |= 1u << 29;
    if (j0 & PSK_DOWN) b |= 1u << 30;
    if (j0 & PSK_UP) b |= 1u << 31;
    return ~b;   // DrvInputs = ~0x60 | (DIP A & 0x60) con DIP A = 0x60 (service/debug off), poi XOR dei premuti
}

// ------------------------------------------------------------------ handler 0: tutto cio' che non e' mappato
// (YMF278B 0x05000000, ingressi/EEPROM 0x05800000, scritture tabella zoom / registri video / IRQ)
static void vid_write_byte(UINT32 a, UINT8 d) {
    // a gia' mascherato con 0xc7ffffff; zona 0x03050000-0x0305ffff
    if ((a & 0xfffffe00) == 0x03050000) { ((UINT8 *)RamZoom)[(a & 0x1ff) ^ 3] = d; return; }
    if ((a & 0xffffffe0) == 0x0305ffe0) {
        ((UINT8 *)VidRegs)[(a & 0x1f) ^ 3] = d;
        ((UINT8 *)RamZoom)[(a & 0xffff) ^ 3] = d;
        if ((a & 0x1c) == 0x10) graphics_bank();
        return;
    }
    if ((a & 0xfffffffc) == 0x0305ffdc) {
        // irqctrl_w di MAME: dato a 32 bit, IRQ 4 tolto se i bit 0x00c00000 sono a zero
        UINT32 v = (UINT32)d << (24 - 8 * (a & 3));
        if (!(v & 0x00c00000)) Sh2SetIRQLine(4, CPU_IRQSTATUS_NONE);
    }
}
static UINT8 __fastcall h0ReadByte(UINT32 a) {
    a &= 0xc7ffffff;
    if ((a & 0xfffffff8) == 0x05000000) { ymf_sync(); return ymf ? ymf->read(a & 7) : 0xff; }
    switch (a) {
    case 0x05800000: case 0x05800001: case 0x05800002: case 0x05800003:
        return (UINT8)(build_inputs() >> ((~a & 3) << 3));
    case 0x05800004:
        return (UINT8)(dip_region | ((ee_read() & 1) << 4));
    }
    return 0;
}
static UINT16 __fastcall h0ReadWord(UINT32 a) { return (UINT16)((h0ReadByte(a) << 8) | h0ReadByte(a + 1)); }
static UINT32 __fastcall h0ReadLong(UINT32 a) { return ((UINT32)h0ReadWord(a) << 16) | h0ReadWord(a + 2); }
static void __fastcall h0WriteByte(UINT32 a, UINT8 d) {
    a &= 0xc7ffffff;
    if ((a & 0xffff0000) == 0x03050000) { vid_write_byte(a, d); return; }
    if ((a & 0xfffffff8) == 0x05000000) { ymf_sync(); if (ymf) ymf->write(a & 7, d); return; }
    if (a == 0x05800004) { ee_lines(d & 0x40, d & 0x80, d & 0x20); return; }
}
static void __fastcall h0WriteWord(UINT32 a, UINT16 d) { h0WriteByte(a, (UINT8)(d >> 8)); h0WriteByte(a + 1, (UINT8)d); }
static void __fastcall h0WriteLong(UINT32 a, UINT32 d) { h0WriteWord(a, (UINT16)(d >> 16)); h0WriteWord(a + 2, (UINT16)d); }

// ------------------------------------------------------------------ RAM con write-watch (solo debug)
unsigned int Sh2GetPR(int);
static int watch_v = -1;   // debug: solo scritture con questo valore (WATCHV)
static inline void watch_hit(UINT32 addr, UINT32 data, int sz) {
    UINT32 a = addr & 0x000fffff;
    if (a + sz - 1 >= watch_lo && a <= watch_hi && (watch_v < 0 || (UINT32)watch_v == data))
        fprintf(stderr, "W%d %06x <- %0*x  pc %08x pr %08x\n", sz * 8, a, sz * 2, data, Sh2GetPC(0), Sh2GetPR(0));
}
extern "C" void psikyo_set_watch_value(int v) { watch_v = v; }
static UINT8 __fastcall wRamReadByte(UINT32 a) { return RamSh2[(a & 0xfffff) ^ 3]; }
static UINT16 __fastcall wRamReadWord(UINT32 a) { return *(UINT16 *)(RamSh2 + ((a & 0xfffff) ^ 2)); }
static UINT32 __fastcall wRamReadLong(UINT32 a) { return *(UINT32 *)(RamSh2 + (a & 0xfffff)); }
static void __fastcall wRamWriteByte(UINT32 a, UINT8 d) { watch_hit(a, d, 1); RamSh2[(a & 0xfffff) ^ 3] = d; }
static void __fastcall wRamWriteWord(UINT32 a, UINT16 d) { watch_hit(a, d, 2); *(UINT16 *)(RamSh2 + ((a & 0xfffff) ^ 2)) = d; }
static void __fastcall wRamWriteLong(UINT32 a, UINT32 d) { watch_hit(a, d, 4); *(UINT32 *)(RamSh2 + (a & 0xfffff)) = d; }
// Ciclo di attesa del vblank (speed hack di FBNeo, DarakuInit): il gioco incrementa il contatore 0x0600000C e
// controlla un flag finche' arriva l'interruzione; letto da quei PC, la CPU resta ferma fino al prossimo IRQ
// (stessi cicli consumati, nessuna istruzione emulata). Il contatore entra nel generatore casuale (0x47F42:
// seme * contatore), quindi alla lettura successiva gli si sommano i giri che il ciclo avrebbe fatto nel tempo
// sospeso (IDLE_ITER cicli per giro, misurato senza speed hack) quando arriva l'IRQ (idle_wake), come sull'hardware.
static UINT32 idle_pc[2] = { 0x0004761c, 0x00047978 };
static int idle_on = 1;
static UINT32 __fastcall idleReadLong(UINT32 a) {
    if ((a & 0xfffff) == 0x00000c && idle_on && idle_t0 < 0) {
        UINT32 pc = Sh2GetPC(0);
        if (pc == idle_pc[0] || pc == idle_pc[1]) { idle_t0 = cpu_now(); Sh2BurnUntilInt(0); }
    }
    return *(UINT32 *)(RamSh2 + (a & 0xfffff));
}
extern "C" void psikyo_set_idle_skip(int on) { idle_on = on; }
static void map_ram(void) {
    Sh2MapMemory(RamSh2, 0x06000000, 0x060fffff, MAP_RAM);
    if (watch_lo <= watch_hi) {
        Sh2MapHandler(5, 0x06000000, 0x060fffff, MAP_READ | MAP_WRITE);
        Sh2SetReadByteHandler(5, wRamReadByte); Sh2SetReadWordHandler(5, wRamReadWord); Sh2SetReadLongHandler(5, wRamReadLong);
        Sh2SetWriteByteHandler(5, wRamWriteByte); Sh2SetWriteWordHandler(5, wRamWriteWord); Sh2SetWriteLongHandler(5, wRamWriteLong);
    }
    Sh2MapHandler(1, 0x06000000, 0x0600ffff, MAP_READ);
    Sh2SetReadByteHandler(1, wRamReadByte); Sh2SetReadWordHandler(1, wRamReadWord); Sh2SetReadLongHandler(1, idleReadLong);
}
extern "C" void psikyo_set_watch(uint32_t lo, uint32_t hi) { watch_lo = lo; watch_hi = hi; if (loaded) map_ram(); }

// debug: scritture nella sprite RAM in [sw_lo, sw_hi] (offset) con (dato & mask) == val
static UINT32 sw_mask, sw_val, sw_on, sw_lo = 1, sw_hi = 0;
unsigned int Sh2GetR(int, int);
static void sw_log(UINT32 a, UINT32 d, int sz) {
    UINT32 o = a & 0xffff;
    if (o + sz - 1 >= sw_lo && o <= sw_hi && (d & sw_mask) == sw_val) {
        fprintf(stderr, "S%d %04x <- %0*x  pc %08x pr %08x", sz * 8, o, sz * 2, d, Sh2GetPC(0), Sh2GetPR(0));
        if (getenv("SPRSTACK")) {
            UINT32 sp = Sh2GetR(0, 15);
            fprintf(stderr, " sp %08x:", sp);
            for (int i = 0; i < 48; ++i) { UINT32 v = *(UINT32 *)(RamSh2 + ((sp + i * 4) & 0xffffc)); if (v < 0x100000) fprintf(stderr, " [%d]%06x", i, v); }
        }
        fprintf(stderr, "\n");
    }
}
static UINT8 __fastcall sRead8(UINT32 a) { return ((UINT8 *)RamSpr)[(a & 0xffff) ^ 3]; }
static UINT16 __fastcall sRead16(UINT32 a) { return *(UINT16 *)((UINT8 *)RamSpr + ((a & 0xffff) ^ 2)); }
static UINT32 __fastcall sRead32(UINT32 a) { return *(UINT32 *)((UINT8 *)RamSpr + (a & 0xffff)); }
static void __fastcall sWrite8(UINT32 a, UINT8 d) { sw_log(a, d, 1); ((UINT8 *)RamSpr)[(a & 0xffff) ^ 3] = d; }
static void __fastcall sWrite16(UINT32 a, UINT16 d) { sw_log(a, d, 2); *(UINT16 *)((UINT8 *)RamSpr + ((a & 0xffff) ^ 2)) = d; }
static void __fastcall sWrite32(UINT32 a, UINT32 d) { sw_log(a, d, 4); *(UINT32 *)((UINT8 *)RamSpr + (a & 0xffff)) = d; }
extern "C" void psikyo_set_sprwatch(uint32_t mask, uint32_t val, uint32_t lo, uint32_t hi) {
    sw_mask = mask; sw_val = val; sw_lo = lo; sw_hi = hi; sw_on = 1;
    if (!loaded) return;
    Sh2MapHandler(6, 0x03000000, 0x0300ffff, MAP_READ | MAP_WRITE);
    Sh2SetReadByteHandler(6, sRead8); Sh2SetReadWordHandler(6, sRead16); Sh2SetReadLongHandler(6, sRead32);
    Sh2SetWriteByteHandler(6, sWrite8); Sh2SetWriteWordHandler(6, sWrite16); Sh2SetWriteLongHandler(6, sWrite32);
}

// ------------------------------------------------------------------ layout memoria (MemIndex di FBNeo)
static void MemIndex(void) {
    UINT8 *Next = Mem;
    RomSh2 = Next;   Next += 0x200000;
    RomTiles = Next; Next += gfx_size + 0x20000;
    RomSnd = Next;   Next += 0x400000;
    RamStart = Next;
    RamZoom = (UINT32 *)Next;   Next += 0x10000;
    RamPal = (UINT32 *)Next;    Next += 0x10000;
    RamSpr = (UINT32 *)Next;    Next += 0x10000;
    VidRegs = (UINT32 *)Next;   Next += 0x200;
    RamSh2 = Next;              Next += 0x100000;
    RamSprBuf = (UINT32 *)Next; Next += 0x4000;
    RamEnd = Next;
    Pal32 = (UINT32 *)Next;     Next += 0x1400 * sizeof(UINT32);
    TransTab = Next;            Next += 0x18000;
    MemEnd = Next;
}

static void *zip_get(mz_zip_archive *z, const char *name, size_t expect) {
    int idx = mz_zip_reader_locate_file(z, name, NULL, 0);
    if (idx < 0) return NULL;
    size_t sz;
    void *d = mz_zip_reader_extract_to_heap(z, idx, &sz, 0);
    if (!d || sz != expect) { free(d); return NULL; }
    return d;
}
// BurnLoadRom(dst, i, 2): un byte ogni due a partire da dst
static void load_gap2(UINT8 *dst, const UINT8 *src, size_t n) { for (size_t i = 0; i < n; ++i) dst[i * 2] = src[i]; }

static void calc_transtab(void) {
    memset(TransTab, 0xff, 0x18000);
    for (UINT32 i = 0; i < gfx_size; i += 0x80)
        for (UINT32 j = 0; j < 0x80; j++) if (RomTiles[i + j]) { TransTab[(i >> 10)] &= ~(1 << ((i >> 7) & 7)); break; }
    for (UINT32 i = 0; i < gfx_size; i += 0x100)
        for (UINT32 j = 0; j < 0x100; j++) if (RomTiles[i + j]) { TransTab[(i >> 11) + 0x10000] &= ~(1 << ((i >> 8) & 7)); break; }
}

static void ymf_create(void) {
    ymf_if = new PsikyoYmf();
    ymf = new ymfm::ymf278b(*ymf_if);
    ymf->reset();
}
static void ymf_destroy(void) {
    delete ymf; ymf = NULL;
    delete ymf_if; ymf_if = NULL;
}

static void video_init(void);
static void calc_mono(void);

static void do_reset(void) {
    memset(RamStart, 0, RamEnd - RamStart);
    Sh2Reset();
    prev_bank = -1;
    graphics_bank();
    ee_reset(); ee.latch = 0; ee.reset_line = 1; ee.clock_line = 1; ee.locked = 1;
    nExtraCycles = 0; cpu_clk_base = 0; idle_t0 = -1;
    frame_clk_base = 0; gen_clk = 0; asamps = 0;
    ymf_if->timer_end[0] = ymf_if->timer_end[1] = -1; ymf_if->busy_end = 0; ymf_if->irq = 0;
    ymf->reset();
    Sh2SetIRQLine(12, CPU_IRQSTATUS_NONE);
    Sh2SetIRQLine(4, CPU_IRQSTATUS_NONE);
}

extern "C" int psikyo_load_zip(const char *zip_path, const char *game, int region) {
    psikyo_unload();
    if (strcmp(game, "daraku")) return 0;
    gfx_size = 0x3400000;
    dip_region = region ? 1 : 0;

    mz_zip_archive z; memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_file(&z, zip_path, 0)) return 0;
    Mem = NULL; MemIndex();
    size_t len = MemEnd - (UINT8 *)0;
    Mem = (UINT8 *)calloc(1, len);
    if (!Mem) { mz_zip_reader_end(&z); return 0; }
    MemIndex();

    // DarakuLoadCallback di FBNeo
    int ok = 1;
    void *pl = zip_get(&z, "4_prog_l.u18", 0x80000), *ph = zip_get(&z, "3_prog_h.u17", 0x80000), *pm = zip_get(&z, "prog.u16", 0x100000);
    if (!pl || !ph || !pm) ok = 0;
    else { load_gap2(RomSh2 + 1, (UINT8 *)pl, 0x80000); load_gap2(RomSh2, (UINT8 *)ph, 0x80000); memcpy(RomSh2 + 0x100000, pm, 0x100000); }
    free(pl); free(ph); free(pm);
    static const char *gl[7] = { "0l.u4", "1l.u3", "2l.u2", "3l.u1", "4l.u10", "5l.u9", "6l.u8" };
    static const char *gh[7] = { "0h.u13", "1h.u12", "2h.u20", "3h.u19", "4h.u31", "5h.u30", "6h.u37" };
    static const size_t gsz[7] = { 0x400000, 0x400000, 0x400000, 0x400000, 0x400000, 0x400000, 0x200000 };
    for (int i = 0; i < 7 && ok; ++i) {
        void *bl = zip_get(&z, gl[i], gsz[i]), *bh = zip_get(&z, gh[i], gsz[i]);
        if (!bl || !bh) ok = 0;
        else { load_gap2(RomTiles + 0x800000 * i, (UINT8 *)bl, gsz[i]); load_gap2(RomTiles + 0x800000 * i + 1, (UINT8 *)bh, gsz[i]); }
        free(bl); free(bh);
    }
    void *sr = zip_get(&z, "sound.u32", 0x400000);
    if (!sr) ok = 0; else memcpy(RomSnd, sr, 0x400000);
    free(sr);
    // EEPROM: contenuto di fabbrica del romset (MAME eeprom-daraku.bin) o, se manca, quello di FBNeo
    memset(ee.data, 0, sizeof ee.data);
    static const UINT8 daraku_eeprom[16] = { 0x03, 0x02, 0x00, 0x48 };
    void *eb = zip_get(&z, "eeprom-daraku.bin", 0x100);
    if (eb) memcpy(ee.data, eb, 0x100); else memcpy(ee.data, daraku_eeprom, 16);
    free(eb);
    mz_zip_reader_end(&z);
    if (!ok) { free(Mem); Mem = NULL; return 0; }

    // scambi di byte di DrvInit (verificati sul vettore di reset: PC 0x000623a4, SP 0x06100000)
    for (UINT8 *p = RomSh2, *e = RomSh2 + 0x100000; p < e; p += 4) { UINT8 t = p[1]; p[1] = p[2]; p[2] = t; }  // BurnSwap32
    for (UINT8 *p = RomSh2, *e = RomSh2 + 0x200000; p < e; p += 2) { UINT8 t = p[0]; p[0] = p[1]; p[1] = t; }  // BurnByteswap
    for (UINT8 *p = RomSh2, *e = RomSh2 + 0x200000; p < e; p += 4) {                                           // BurnSwapEndian
        UINT8 t = p[0]; p[0] = p[3]; p[3] = t; t = p[1]; p[1] = p[2]; p[2] = t;
    }
    for (UINT8 *p = RomTiles, *e = RomTiles + gfx_size; p < e; p += 4) { UINT8 t = p[1]; p[1] = p[2]; p[2] = t; }  // DrvGfxDecode
    calc_transtab();
    calc_mono();

    Sh2Init(1);
    Sh2Open(0);
    cps3speedhack = 1;   // Sh2BurnUntilInt: la CPU sospesa consuma i cicli fino al prossimo IRQ
    Sh2MapMemory(RomSh2, 0x00000000, 0x000fffff, MAP_ROM);
    Sh2MapMemory(RomSh2 + 0x100000, 0x02000000, 0x020fffff, MAP_ROM);
    Sh2MapMemory((UINT8 *)RamSpr, 0x03000000, 0x0300ffff, MAP_RAM);
    Sh2MapMemory((UINT8 *)RamPal, 0x03040000, 0x0304ffff, MAP_RAM);
    Sh2MapMemory((UINT8 *)RamZoom, 0x03050000, 0x0305ffff, MAP_ROM);   // letture dirette, scritture all'handler 0
    Sh2SetReadByteHandler(0, h0ReadByte);   Sh2SetReadWordHandler(0, h0ReadWord);   Sh2SetReadLongHandler(0, h0ReadLong);
    Sh2SetWriteByteHandler(0, h0WriteByte); Sh2SetWriteWordHandler(0, h0WriteWord); Sh2SetWriteLongHandler(0, h0WriteLong);
    map_ram();
    ymf_create();
    video_init();
    loaded = 1;
    do_reset();
    return 1;
}

extern "C" void psikyo_unload(void) {
    if (!loaded) return;
    Sh2Exit();
    ymf_destroy();
    free(Mem); Mem = NULL;
    loaded = 0;
}

extern "C" void psikyo_reset(void) { if (loaded) do_reset(); }
extern "C" void psikyo_set_overclock(int percent) { overclock_pct = percent < 50 ? 50 : percent > 400 ? 400 : percent; }
extern "C" void psikyo_set_region(int region) { dip_region = region ? 1 : 0; }

// ------------------------------------------------------------------ fotogramma
static void hd_render(void);
static uint32_t *hd_out;

extern "C" void psikyo_frame(int render) {
    if (!loaded) return;
    asamps = 0;
    { INT32 tprev = Sh2TotalCycles(); Sh2NewFrame(); Sh2Idle(nExtraCycles); cpu_clk_base += tprev - nExtraCycles; }
    const INT32 total = (INT32)(FRAME_CLOCKS * overclock_pct / 100);
    // esecuzione a fette: ogni fetta si ferma alla prossima scadenza di un timer del YMF278B
    while (Sh2TotalCycles() < total) {
        INT32 pos = Sh2TotalCycles();
        INT32 end = total;
        for (int t = 0; t < 2; ++t) {
            if (ymf_if->timer_end[t] < 0) continue;
            INT64 c = (ymf_if->timer_end[t] - frame_clk_base) * overclock_pct / 100;   // ciclo CPU della scadenza
            if (c < pos + 1) c = pos + 1;
            if (c < end) end = (INT32)c;
        }
        if (end - pos > 4096) end = pos + 4096;
        Sh2Run(end - pos);
        INT64 now = ymf_now();
        for (int t = 0; t < 2; ++t)
            if (ymf_if->timer_end[t] >= 0 && ymf_if->timer_end[t] <= now) { ymf_sync(); ymf_if->expire(t); }
    }
    idle_wake();
    Sh2SetIRQLine(4, CPU_IRQSTATUS_ACK);
    nExtraCycles = Sh2TotalCycles() - total;
    // fine frame: audio fino al confine del frame in tempo reale, poi base del frame successivo
    {
        INT64 end_clk = frame_clk_base + FRAME_CLOCKS;
        while (gen_clk + 768 <= end_clk) {
            ymfm::ymf278b::output_data o;
            ymf->generate(&o, 1);
            if (asamps < ABUF_MAX) { abuf[asamps * 2] = (INT16)o.data[4]; abuf[asamps * 2 + 1] = (INT16)o.data[5]; asamps++; }
            gen_clk += 768;
        }
        frame_clk_base = end_clk;
    }
    if (render && hd_out) hd_render();
    memcpy(RamSprBuf, RamSpr, 0x4000);   // sprite + lista catturati al vblank (FBNeo DrvSprBuf / MAME get_sprites)
}

// ------------------------------------------------------------------ accesso per il front-end / banco di prova
extern "C" uint8_t psikyo_read8(uint32_t addr) {
    if (!loaded) return 0;
    if ((addr & 0xfff00000) == 0x06000000) return RamSh2[(addr & 0xfffff) ^ 3];
    if ((addr & 0xffff0000) == 0x03000000) return ((UINT8 *)RamSpr)[(addr & 0xffff) ^ 3];
    if ((addr & 0xffff0000) == 0x03040000) return ((UINT8 *)RamPal)[(addr & 0xffff) ^ 3];
    if ((addr & 0xffff0000) == 0x03050000) return ((UINT8 *)RamZoom)[(addr & 0xffff) ^ 3];
    if (addr < 0x00100000) return RomSh2[addr ^ 3];
    if ((addr & 0xfff00000) == 0x02000000) return RomSh2[(0x100000 + (addr & 0xfffff)) ^ 3];
    return 0;
}
extern "C" void psikyo_write8(uint32_t addr, uint8_t v) {
    if (loaded && (addr & 0xfff00000) == 0x06000000) RamSh2[(addr & 0xfffff) ^ 3] = v;
}
extern "C" unsigned psikyo_pc(void) { return loaded ? Sh2GetPC(0) : 0; }
extern "C" uint32_t psikyo_vidreg(int i) { return loaded ? VidRegs[i & 7] : 0; }
extern "C" const uint32_t *psikyo_spritebuf(void) { return RamSprBuf; }
extern "C" const uint32_t *psikyo_sprram(void) { return RamSpr; }
extern "C" const uint32_t *psikyo_palette32(void) { return Pal32; }

// debug: carica la RAM video da due dump big-endian (frame F-1 per gli sprite, F per il resto):
// sprite RAM 0x10000, palette 0x5000, zoom 0x200, vidregs 0x20 (dump.lua di MAME)
static void be_copy(UINT32 *dst, const uint8_t *src, size_t bytes) {
    for (size_t i = 0; i < bytes / 4; ++i) dst[i] = ((UINT32)src[i*4] << 24) | (src[i*4+1] << 16) | (src[i*4+2] << 8) | src[i*4+3];
}
extern "C" void psikyo_debug_load_video(const uint8_t *prev, const uint8_t *cur) {
    if (!loaded) return;
    be_copy(RamSprBuf, prev, 0x4000);
    be_copy(RamSpr, cur, 0x10000);
    be_copy(RamPal, cur + 0x10000, 0x5000);
    be_copy(RamZoom, cur + 0x15000, 0x200);
    be_copy(VidRegs, cur + 0x15200, 0x20);
}

// ------------------------------------------------------------------ stato
static UINT8 *st_ptr; static size_t st_pos, st_cap; static int st_mode;
static INT32 st_acb(struct BurnArea *ba) {
    if (st_mode == 1 && st_pos + ba->nLen <= st_cap) memcpy(st_ptr + st_pos, ba->Data, ba->nLen);
    if (st_mode == 2 && st_pos + ba->nLen <= st_cap) memcpy(ba->Data, st_ptr + st_pos, ba->nLen);
    st_pos += ba->nLen;
    return 0;
}
static void st_area(void *d, size_t n) { struct BurnArea ba; ba.Data = d; ba.nLen = (UINT32)n; ba.nAddress = 0; ba.szName = ""; st_acb(&ba); }
static void st_scan(void) {
    BurnAcb = st_acb;
    st_area(RamStart, RamEnd - RamStart);
    Sh2Scan(ACB_DRIVER_DATA | (st_mode == 2 ? ACB_WRITE : ACB_READ));
    st_area(&ee, sizeof ee);
    SCAN_VAR(prev_bank); SCAN_VAR(nExtraCycles); SCAN_VAR(dip_region); SCAN_VAR(cpu_clk_base); SCAN_VAR(idle_t0);
    SCAN_VAR(frame_clk_base); SCAN_VAR(gen_clk);
    SCAN_VAR(ymf_if->timer_end); SCAN_VAR(ymf_if->busy_end); SCAN_VAR(ymf_if->irq);
    // stato del YMF278B (ymfm): lunghezza fissa per questo chip
    std::vector<uint8_t> v;
    if (st_mode != 2) { ymfm::ymfm_saved_state s(v, true); ymf->save_restore(s); }
    UINT32 n = (UINT32)v.size();
    SCAN_VAR(n);
    if (st_mode == 2) {
        v.resize(n);
        st_area(v.data(), n);
        ymfm::ymfm_saved_state s(v, false); ymf->save_restore(s);
    } else st_area(v.data(), n);
}
extern "C" size_t psikyo_state_size(void) { if (!loaded) return 0; st_mode = 0; st_pos = 0; st_scan(); return st_pos; }
extern "C" int psikyo_state_save(void *buf, size_t sz) {
    if (!loaded) return 0;
    st_mode = 1; st_ptr = (UINT8 *)buf; st_cap = sz; st_pos = 0; st_scan();
    return st_pos <= sz;
}
extern "C" int psikyo_state_load(const void *buf, size_t sz) {
    if (!loaded) return 0;
    st_mode = 2; st_ptr = (UINT8 *)buf; st_cap = sz; st_pos = 0; st_scan();
    prev_bank = -1; graphics_bank();
    return st_pos <= sz;
}

#include "psikyo_video.inc"
