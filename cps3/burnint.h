// SNKHD - shim minimo dell'ambiente FBNeo (burnint.h) per compilare il core SH-2 da solo
#ifndef SNKHD_BURNINT_SHIM_H
#define SNKHD_BURNINT_SHIM_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int8_t INT8;   typedef uint8_t UINT8;
typedef int16_t INT16; typedef uint16_t UINT16;
typedef int32_t INT32; typedef uint32_t UINT32;
typedef int64_t INT64; typedef uint64_t UINT64;

#ifndef LSB_FIRST
#define LSB_FIRST 1
#endif
#define _T(x) x
#define PRINT_NORMAL 0
#define PRINT_UI 1
#define PRINT_IMPORTANT 2
#define PRINT_ERROR 3
static inline int bprintf(int, const char *, ...) { return 0; }

#define MAP_READ    0x01
#define MAP_WRITE   0x02
#define MAP_FETCH   0x0C
#define MAP_ROM     0x0D
#define MAP_RAM_ALL 0x0F
#define MAP_RAM     0x0F

#define CPU_IRQSTATUS_NONE 0
#define CPU_IRQSTATUS_ACK  1
#define CPU_IRQSTATUS_AUTO 2
#define CPU_IRQSTATUS_HOLD 4

struct cpu_core_config {
    char cpu_name[32];
    void (*open)(int);
    void (*close)();
    UINT8 (*read)(UINT32);
    void (*write)(UINT32, UINT8);
    int (*active)();
    int (*totalcycles)();
    void (*newframe)();
    int (*idle)(int);
    void (*irq)(int, int, int);
    int (*run)(int);
    void (*runend)();
    void (*reset)();
    int (*scan)(int);
    void (*exit)();
    UINT64 nMemorySize;
    UINT32 nAddressFlags;
};
static int DebugCPU_SH2Initted __attribute__((unused));
static inline void CpuCheatRegister(int, cpu_core_config *) {}

// stato (savestate del banco di prova): callback impostata dal driver
struct BurnArea { void *Data; UINT32 nLen; INT32 nAddress; const char *szName; };
extern INT32 (*BurnAcb)(struct BurnArea *pba);
#define ACB_READ        (1 << 0)
#define ACB_WRITE       (1 << 1)
#define ACB_MEMORY_ROM  (1 << 2)
#define ACB_NVRAM       (1 << 3)
#define ACB_MEMCARD     (1 << 4)
#define ACB_MEMORY_RAM  (1 << 5)
#define ACB_DRIVER_DATA (1 << 6)
#define ACB_FULLSCAN    (ACB_NVRAM | ACB_MEMCARD | ACB_MEMORY_RAM | ACB_DRIVER_DATA)
#define STRUCT_SIZE_HELPER(type, last) (offsetof(type, last) + sizeof(((type *)0)->last))
#define SCAN_VAR(x) { struct BurnArea ba_; ba_.Data = &(x); ba_.nLen = sizeof(x); ba_.nAddress = 0; ba_.szName = #x; BurnAcb(&ba_); }
#endif
