// SNKHD - variabili globali attese dal core (normalmente dal framework FBNeo)
#include "burnint.h"
int nCurrentFrame = 0;
int bBurnRunAheadFrame = 0;
int nIpsDrvDefine = 0;
UINT8 *pBurnDraw = NULL;
void (*snes_dbg_exec)(uint32_t) = NULL;
void (*snes_dbg_write)(uint32_t, uint8_t) = NULL;
void (*snes_dbg_read)(uint32_t, uint8_t) = NULL;
