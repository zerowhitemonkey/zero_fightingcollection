// stub: ST018 (ARM7) non usato da Weapon Lord
#include "burnint.h"
#define MAP_ROM 0x0D
#define MAP_RAM 0x0F
#define MAP_READ 0x01
#define MAP_WRITE 0x02
static inline void Arm7Init(int) {}
static inline void Arm7Open(int) {}
static inline void Arm7Close() {}
static inline void Arm7Exit() {}
static inline void Arm7Reset() {}
static inline INT32 Arm7Run(INT32 n) { return n; }
static inline void Arm7MapMemory(UINT8 *, UINT32, UINT32, int) {}
template <typename F> static inline void Arm7SetReadByteHandler(F) {}
template <typename F> static inline void Arm7SetReadWordHandler(F) {}
template <typename F> static inline void Arm7SetReadLongHandler(F) {}
template <typename F> static inline void Arm7SetWriteByteHandler(F) {}
template <typename F> static inline void Arm7SetWriteWordHandler(F) {}
template <typename F> static inline void Arm7SetWriteLongHandler(F) {}
static inline void arm7_get_context(void **ctx, INT32 *sz) { *ctx = 0; *sz = 0; }
