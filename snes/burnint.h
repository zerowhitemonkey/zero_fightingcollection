// SNKHD - shim minimo dell'ambiente FBNeo (burnint.h) per compilare il core SNES (LakeSnes, licenza MIT) da solo
#ifndef SNKHD_SNES_BURNINT_SHIM_H
#define SNKHD_SNES_BURNINT_SHIM_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

typedef int8_t INT8;   typedef uint8_t UINT8;
typedef int16_t INT16; typedef uint16_t UINT16;
typedef int32_t INT32; typedef uint32_t UINT32;
typedef int64_t INT64; typedef uint64_t UINT64;

#ifndef LSB_FIRST
#define LSB_FIRST 1
#endif
#define _T(x) x
static inline int bprintf(int, const char *, ...) { return 0; }
#define SNKHD_SNES 1
static inline UINT8 *BurnMalloc(size_t n) { return (UINT8 *)calloc(1, n ? n : 1); }
static inline UINT8 *BurnRealloc(void *p, size_t n) { return (UINT8 *)realloc(p, n); }
static inline void BurnFree(void *p) { free(p); }
static inline uint32_t BurnRoundPowerOf2(uint32_t v) { v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; return v + 1; }
extern int nCurrentFrame;
extern int bBurnRunAheadFrame;
extern int nIpsDrvDefine;
extern UINT8 *pBurnDraw;
extern void (*snes_dbg_exec)(uint32_t pc24);                 // SNKHD debug: chiamata prima di ogni istruzione (NULL = spenta)
extern void (*snes_dbg_write)(uint32_t adr24, uint8_t val);  // SNKHD debug: scritture CPU
extern void (*snes_dbg_read)(uint32_t adr24, uint8_t val);   // SNKHD debug: letture CPU
void cpu_dbgRegs(uint32_t *r);                    // NULL = niente rendering (avvio veloce)
typedef char TCHAR;
#define PRINT_NORMAL 0
#define PRINT_UI 1
#define PRINT_IMPORTANT 2
#define PRINT_ERROR 3
#define DRV_NAME 0
#define DRV_PARENT 1
static inline const char *BurnDrvGetText(int) { return ""; }
static inline const char *BurnDrvGetTextA(int) { return ""; }
static inline void nes_init_cheat_functions(void (*)(char *, int), void (*)(char *)) {}
#endif
