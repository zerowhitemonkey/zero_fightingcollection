// stub: DSP-1/2/3/4 non usati da Weapon Lord
#include "burnint.h"
static inline void upd96050Init(int, UINT8 *, UINT8 *, UINT8 *, void *, void *) {}
static inline void upd96050Reset() {}
static inline INT32 upd96050Run(INT32 n) { return n; }
static inline INT32 upd96050ExportRegs(void **p) { static UINT8 z[4]; *p = z; return 0; }
static inline UINT8 snesdsp_read(UINT32) { return 0xff; }
static inline void snesdsp_write(UINT32, UINT8) {}
