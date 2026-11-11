// SNKHD - ingressi: controller (SDL GameController) e tastiera
#ifndef SNKHD_INPUT_H
#define SNKHD_INPUT_H
#include <stdint.h>

#define NACT 9   // Neo Geo: A, B, C, D, A+B, B+C, C+D, A+B+C, A+B+C+D / CPS3: LP MP HP LK MK HK 3P 3K LP+LK
extern const char **act_names;
extern const unsigned *act_bits;
void input_set_system(int sys);          // SYS_NEOGEO / SYS_CPS3: azioni e mappatura predefinita
void input_set_layout(int layout);       // CPS3: 1 = 6 tasti (LP..HK), 2 = JoJo (A B C + S = Stand)

#define PAD_LT 100
#define PAD_RT 101

typedef struct { int pad[NACT]; int key[NACT]; } pmap_t;   // -1 = vuoto

// eventi di menu (fronte di salita, con ripetizione per le direzioni)
enum { M_UP = 1, M_DOWN = 2, M_LEFT = 4, M_RIGHT = 8, M_OK = 16, M_BACK = 32, M_START = 64, M_SELECT = 128, M_X = 256 };   // M_X: tasto X del controller / F2

void input_init(void);
void input_handle_event(const void *sdl_event);
void input_update(void);                 // una volta per ciclo
unsigned input_menu(void);               // eventi di menu del ciclo corrente (qualsiasi giocatore)
// stato per il gioco: joystick (bit IN_*) e sistema (start pausa, select = START del gioco)
unsigned input_game(int player, const pmap_t *m, int *start_edge, int *select_held, int enter_is_start);
void input_default_map(int player, pmap_t *m);
// registrazione di un tasto: ritorna 1 se catturato (pad >= 0 oppure key >= 0)
void input_capture_begin(void);
int  input_capture_poll(int *pad, int *key);
void input_capture_cancel(void);
void input_flush_start(void);
const char *input_pad_name(int b);
const char *input_key_name(int k);
int  input_pad_count(void);
int input_pad_back(void);         // SELECT/BACK premuto su un pad qualsiasi
int input_pad_back_start(void);
#endif
