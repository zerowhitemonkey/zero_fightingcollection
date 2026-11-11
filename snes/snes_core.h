// SNKHD - core SNES (LakeSnes nella versione FBNeo, licenza MIT): API C
#ifndef SNKHD_SNES_CORE_H
#define SNKHD_SNES_CORE_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SNES_FB_W 512
#define SNES_FB_H 448
#define SNES_AUDIO_PER_FRAME 534
#define SNES_FPS_NTSC 60.0988
#define SNES_FPS_PAL  50.0070

// bit ingressi (1 = premuto), stesso ordine del registro joypad hardware
enum { SN_B = 1 << 0, SN_Y = 1 << 1, SN_SELECT = 1 << 2, SN_START = 1 << 3, SN_UP = 1 << 4, SN_DOWN = 1 << 5,
       SN_LEFT = 1 << 6, SN_RIGHT = 1 << 7, SN_A = 1 << 8, SN_X = 1 << 9, SN_L = 1 << 10, SN_R = 1 << 11 };

int  snescore_load_zip(const char *zip_path);       // zip con un solo .sfc/.smc: 1 = ok
int  snescore_load_mem(const uint8_t *rom, size_t len);
void snescore_unload(void);
void snescore_reset(void);
void snescore_set_input(int port, unsigned buttons); // port 0/1
void snescore_frame(int render);                      // render 0 = avvio veloce (nessun disegno)
const uint32_t *snescore_video(unsigned *w, unsigned *h, unsigned *pitch_px); // 512 x 448, 0xAARRGGBB
const int16_t *snescore_audio(size_t *frames);        // stereo int16, SNES_AUDIO_PER_FRAME fotogrammi
double snescore_fps(void);
uint8_t snescore_wram8(uint32_t addr);                // 0-0x1FFFF
void snescore_wram_write8(uint32_t addr, uint8_t v);
int  snescore_is_pal(void);
// debug (banco di prova): ganci prima di ogni istruzione e sulle scritture della CPU; registri a x y sp pc dp k db flags
void snescore_set_debug(void (*exec)(uint32_t pc24), void (*write)(uint32_t adr24, uint8_t val), void (*read)(uint32_t adr24, uint8_t val));
void snescore_regs(uint32_t *r9);
const uint8_t *snescore_rom(size_t *len);
int  snescore_patch_weaponlord(void);                // limita le parate della CPU (solo Weapon Lord USA); 1 = applicata
size_t snescore_state_size(void);
int snescore_state_save(void *buf);
int snescore_state_load(const void *buf, size_t sz);

#ifdef __cplusplus
}
#endif
#endif
