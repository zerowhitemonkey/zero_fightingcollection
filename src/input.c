// SNKHD - ingressi
#include <SDL.h>
#include <string.h>
#include "input.h"
#include "emu.h"

static const char *neo_names[NACT] = { "A", "B", "C", "D", "A+B", "B+C", "C+D", "A+B+C", "A+B+C+D" };
static const unsigned neo_bits[NACT] = { IN_A, IN_B, IN_C, IN_D, IN_A | IN_B, IN_B | IN_C, IN_C | IN_D,
    IN_A | IN_B | IN_C, IN_A | IN_B | IN_C | IN_D };
// CPS3 (6 tasti): pugni LP MP HP = A B C, calci LK MK HK = D E F
static const char *c3_names[NACT] = { "LP", "MP", "HP", "LK", "MK", "HK", "3P", "3K", "LP+LK" };
static const unsigned c3_bits[NACT] = { IN_A, IN_B, IN_C, IN_D, IN_E, IN_F, IN_A | IN_B | IN_C, IN_D | IN_E | IN_F, IN_A | IN_D };
// Sega ST-V (Golden Axe: The Duel): 6 tasti, attacchi debole/medio/forte = SW1-SW3, calci = SW4-SW6
static const char *st_names[NACT] = { "LA", "MA", "HA", "LK", "MK", "HK", "3A", "3K", "LA+LK" };
// JoJo: attacchi A B C (debole, medio, forte) = tasti 1-3, S (Stand) = tasto 4
static const char *jj_names[NACT] = { "A", "B", "C", "S", "A+B", "B+C", "A+C", "A+B+C", "A+B+C+S" };
static const unsigned jj_bits[NACT] = { IN_A, IN_B, IN_C, IN_D, IN_A | IN_B, IN_B | IN_C, IN_A | IN_C, IN_A | IN_B | IN_C,
    IN_A | IN_B | IN_C | IN_D };
// SNES: stesso ordine delle posizioni sul pad (Y X L in alto, B A R in basso) + combinazioni
static const char *sn_names[NACT] = { "Y", "X", "L", "B", "A", "R", "Y+B", "X+A", "L+R" };
static const unsigned sn_bits[NACT] = { IN_A, IN_B, IN_C, IN_D, IN_E, IN_F, IN_A | IN_D, IN_B | IN_E, IN_C | IN_F };
const char **act_names = neo_names;
const unsigned *act_bits = neo_bits;
static int in_system = SYS_NEOGEO, in_layout = 1;
void input_set_system(int sys) {
    in_system = sys; in_layout = 1;
    act_names = sys == SYS_SNES ? sn_names : sys == SYS_CPS3 ? c3_names : sys == SYS_STV ? st_names : neo_names;
    act_bits = sys == SYS_SNES ? sn_bits : (sys == SYS_CPS3 || sys == SYS_STV) ? c3_bits : neo_bits;
}
void input_set_layout(int layout) {
    if (in_system != SYS_CPS3) return;
    in_layout = layout;
    act_names = layout == 2 ? jj_names : c3_names;
    act_bits = layout == 2 ? jj_bits : c3_bits;
}

#define MAXPAD 4
static SDL_GameController *pads[MAXPAD];
static SDL_JoystickID padid[MAXPAD];
static unsigned menu_now, menu_prev, menu_events;
static Uint32 rep_time;
static int capturing, cap_pad = -1, cap_key = -1;
static unsigned start_prev[2];
static unsigned pending;            // fronti da eventi (tasti premuti brevemente)
static unsigned start_pending[2], enter_pending;

static void open_pad(int idx) {
    if (!SDL_IsGameController(idx)) return;
    SDL_GameController *c = SDL_GameControllerOpen(idx);
    if (!c) return;
    SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c));
    for (int i = 0; i < MAXPAD; ++i) if (pads[i] && padid[i] == id) { SDL_GameControllerClose(c); return; }
    for (int i = 0; i < MAXPAD; ++i) if (!pads[i]) { pads[i] = c; padid[i] = id; return; }
    SDL_GameControllerClose(c);
}

void input_init(void) {
    SDL_GameControllerEventState(SDL_ENABLE);
    for (int i = 0; i < SDL_NumJoysticks(); ++i) open_pad(i);
}

int input_pad_count(void) {
    int n = 0;
    for (int i = 0; i < MAXPAD; ++i) if (pads[i]) ++n;
    return n;
}

// controller del giocatore p (0/1): il p-esimo collegato
// SELECT/BACK e START tenuti insieme su un pad qualsiasi (uscita dai giochi esterni)
int input_pad_back(void) {
    for (int i = 0; i < MAXPAD; ++i)
        if (pads[i] && SDL_GameControllerGetButton(pads[i], SDL_CONTROLLER_BUTTON_BACK)) return 1;
    return 0;
}

int input_pad_back_start(void) {
    for (int i = 0; i < MAXPAD; ++i)
        if (pads[i] && SDL_GameControllerGetButton(pads[i], SDL_CONTROLLER_BUTTON_BACK) && SDL_GameControllerGetButton(pads[i], SDL_CONTROLLER_BUTTON_START)) return 1;
    return 0;
}

static SDL_GameController *pad_of(int p) {
    int n = 0;
    for (int i = 0; i < MAXPAD; ++i) if (pads[i]) { if (n == p) return pads[i]; ++n; }
    return NULL;
}

static int pad_btn(SDL_GameController *c, int b) {
    if (!c || b < 0) return 0;
    if (b == PAD_LT) return SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000;
    if (b == PAD_RT) return SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000;
    return SDL_GameControllerGetButton(c, (SDL_GameControllerButton)b);
}

static unsigned pad_dirs(SDL_GameController *c) {
    unsigned d = 0;
    if (!c) return 0;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP)) d |= IN_UP;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) d |= IN_DOWN;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) d |= IN_LEFT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) d |= IN_RIGHT;
    int x = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    int y = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
    if (x < -16000) d |= IN_LEFT;
    if (x > 16000) d |= IN_RIGHT;
    if (y < -16000) d |= IN_UP;
    if (y > 16000) d |= IN_DOWN;
    return d;
}

void input_handle_event(const void *ev) {
    const SDL_Event *e = (const SDL_Event*)ev;
    switch (e->type) {
    case SDL_CONTROLLERDEVICEADDED: open_pad(e->cdevice.which); break;
    case SDL_CONTROLLERDEVICEREMOVED:
        for (int i = 0; i < MAXPAD; ++i)
            if (pads[i] && padid[i] == e->cdevice.which) { SDL_GameControllerClose(pads[i]); pads[i] = NULL; }
        break;
    case SDL_KEYDOWN:
        if (capturing && !e->key.repeat && cap_key < 0 && cap_pad < 0) cap_key = e->key.keysym.scancode;
        if (!e->key.repeat) {
            switch (e->key.keysym.scancode) {
            case SDL_SCANCODE_UP: pending |= M_UP; break;
            case SDL_SCANCODE_DOWN: pending |= M_DOWN; break;
            case SDL_SCANCODE_LEFT: pending |= M_LEFT; break;
            case SDL_SCANCODE_RIGHT: pending |= M_RIGHT; break;
            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: pending |= M_OK; enter_pending = 1; break;
            case SDL_SCANCODE_ESCAPE: pending |= M_BACK; start_pending[0] = 1; break;
            case SDL_SCANCODE_BACKSPACE: pending |= M_BACK; break;
            case SDL_SCANCODE_F1: pending |= M_START; start_pending[0] = 1; break;
            case SDL_SCANCODE_F2: pending |= M_X; break;
            default: break;
            }
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        if (capturing && cap_key < 0 && cap_pad < 0) cap_pad = e->cbutton.button;
        switch (e->cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: pending |= M_UP; break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: pending |= M_DOWN; break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: pending |= M_LEFT; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: pending |= M_RIGHT; break;
        case SDL_CONTROLLER_BUTTON_A: pending |= M_OK; break;
        case SDL_CONTROLLER_BUTTON_B: pending |= M_BACK; break;
        case SDL_CONTROLLER_BUTTON_BACK: pending |= M_SELECT; break;
        case SDL_CONTROLLER_BUTTON_X: pending |= M_X; break;
        case SDL_CONTROLLER_BUTTON_START: {
            pending |= M_START;
            int n = 0;
            for (int i = 0; i < MAXPAD; ++i) if (pads[i]) {
                if (padid[i] == e->cbutton.which && n < 2) start_pending[n] = 1;
                ++n;
            }
            break; }
        default: break;
        }
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (capturing && cap_key < 0 && cap_pad < 0 && e->caxis.value > 20000) {
            if (e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) cap_pad = PAD_LT;
            if (e->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) cap_pad = PAD_RT;
        }
        break;
    }
}

void input_update(void) {
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    unsigned m = 0;
    if (k[SDL_SCANCODE_UP]) m |= M_UP;
    if (k[SDL_SCANCODE_DOWN]) m |= M_DOWN;
    if (k[SDL_SCANCODE_LEFT]) m |= M_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) m |= M_RIGHT;
    if (k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_KP_ENTER]) m |= M_OK;
    if (k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_BACKSPACE]) m |= M_BACK;
    for (int i = 0; i < MAXPAD; ++i) {
        SDL_GameController *c = pads[i];
        if (!c) continue;
        unsigned d = pad_dirs(c);
        if (d & IN_UP) m |= M_UP;
        if (d & IN_DOWN) m |= M_DOWN;
        if (d & IN_LEFT) m |= M_LEFT;
        if (d & IN_RIGHT) m |= M_RIGHT;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A)) m |= M_OK;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B)) m |= M_BACK;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START)) m |= M_START;
        if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK)) m |= M_SELECT;
    }
    // START da tastiera: F1 (Invio e' "A" nei menu)
    if (k[SDL_SCANCODE_F1]) m |= M_START;
    Uint32 now = SDL_GetTicks();
    unsigned ev = (m & ~menu_now) | pending;
    pending = 0;
    unsigned dirs = M_UP | M_DOWN | M_LEFT | M_RIGHT;
    if (ev & dirs) rep_time = now + 350;
    else if ((m & dirs) && now >= rep_time) { ev |= m & dirs; rep_time = now + 90; }
    menu_prev = menu_now;
    menu_now = m;
    menu_events = capturing ? 0 : ev;
}

unsigned input_menu(void) { return menu_events; }

static const int kdir[2][4] = {
    { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT },
    { SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L } };

unsigned input_game(int p, const pmap_t *m, int *start_edge, int *select_held, int enter_is_start) {
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    SDL_GameController *c = pad_of(p);
    unsigned j = pad_dirs(c);
    if (k[kdir[p][0]]) j |= IN_UP;
    if (k[kdir[p][1]]) j |= IN_DOWN;
    if (k[kdir[p][2]]) j |= IN_LEFT;
    if (k[kdir[p][3]]) j |= IN_RIGHT;
    for (int a = 0; a < NACT; ++a) {
        if ((m->pad[a] >= 0 && pad_btn(c, m->pad[a])) || (m->key[a] >= 0 && k[m->key[a]]))
            j |= act_bits[a];
    }
    // START (pausa): Start del controller; da tastiera Esc/Invio per il giocatore 1
    unsigned st = (c && SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START)) ? 1 : 0;
    if (p == 0 && (k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_F1] || (enter_is_start && k[SDL_SCANCODE_RETURN]))) st = 1;
    *start_edge = (st && !start_prev[p]) || start_pending[p] || (p == 0 && enter_is_start && enter_pending);
    start_pending[p] = 0;
    if (p == 0) enter_pending = 0;
    start_prev[p] = st;
    // SELECT: Back/View del controller; tastiera 1 / 2
    *select_held = (c && SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK)) ||
        k[p == 0 ? SDL_SCANCODE_1 : SDL_SCANCODE_2];
    return j;
}

void input_default_map(int p, pmap_t *m) {
    for (int a = 0; a < NACT; ++a) m->pad[a] = m->key[a] = -1;
    if (in_system == SYS_CPS3 && in_layout == 2) {
        // JoJo (come la HD Ver. su Xbox): X=A, Y=B, B=C, A=S (Stand), RB=A+B+C
        m->pad[0] = SDL_CONTROLLER_BUTTON_X;
        m->pad[1] = SDL_CONTROLLER_BUTTON_Y;
        m->pad[2] = SDL_CONTROLLER_BUTTON_B;
        m->pad[3] = SDL_CONTROLLER_BUTTON_A;
        m->pad[7] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        if (p == 0) {   // tastiera: Z X C = A B C, V = S, B = A+B+C
            m->key[0] = SDL_SCANCODE_Z; m->key[1] = SDL_SCANCODE_X; m->key[2] = SDL_SCANCODE_C;
            m->key[3] = SDL_SCANCODE_V; m->key[7] = SDL_SCANCODE_B;
        } else {        // tastierino: 4 5 1 = A B C, 2 = S, 6 = A+B+C
            m->key[0] = SDL_SCANCODE_KP_4; m->key[1] = SDL_SCANCODE_KP_5; m->key[2] = SDL_SCANCODE_KP_1;
            m->key[3] = SDL_SCANCODE_KP_2; m->key[7] = SDL_SCANCODE_KP_6;
        }
        return;
    }
    if (in_system == SYS_SNES) {
        // controller: Y X L = X Y LB (stesse posizioni del pad SNES), B A R = A B RB
        m->pad[0] = SDL_CONTROLLER_BUTTON_X;
        m->pad[1] = SDL_CONTROLLER_BUTTON_Y;
        m->pad[2] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
        m->pad[3] = SDL_CONTROLLER_BUTTON_A;
        m->pad[4] = SDL_CONTROLLER_BUTTON_B;
        m->pad[5] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        if (p == 0) {   // tastiera: A S D = Y X L, Z X C = B A R
            m->key[0] = SDL_SCANCODE_A; m->key[1] = SDL_SCANCODE_S; m->key[2] = SDL_SCANCODE_D;
            m->key[3] = SDL_SCANCODE_Z; m->key[4] = SDL_SCANCODE_X; m->key[5] = SDL_SCANCODE_C;
        } else {        // tastierino: 7 8 9 = Y X L, 4 5 6 = B A R
            m->key[0] = SDL_SCANCODE_KP_7; m->key[1] = SDL_SCANCODE_KP_8; m->key[2] = SDL_SCANCODE_KP_9;
            m->key[3] = SDL_SCANCODE_KP_4; m->key[4] = SDL_SCANCODE_KP_5; m->key[5] = SDL_SCANCODE_KP_6;
        }
        return;
    }
    if (in_system == SYS_CPS3 || in_system == SYS_STV) {   // ST-V: stessa disposizione a 6 tasti (attacchi in alto, calci in basso)
        // controller (disposizione Capcom): X=LP, Y=MP, RB=HP, A=LK, B=MK, RT=HK, LB=3P, LT=3K
        m->pad[0] = SDL_CONTROLLER_BUTTON_X;
        m->pad[1] = SDL_CONTROLLER_BUTTON_Y;
        m->pad[2] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
        m->pad[3] = SDL_CONTROLLER_BUTTON_A;
        m->pad[4] = SDL_CONTROLLER_BUTTON_B;
        m->pad[5] = PAD_RT;
        m->pad[6] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
        m->pad[7] = PAD_LT;
        if (p == 0) {   // tastiera: pugni A S D, calci Z X C, 3P = F, 3K = V
            m->key[0] = SDL_SCANCODE_A; m->key[1] = SDL_SCANCODE_S; m->key[2] = SDL_SCANCODE_D;
            m->key[3] = SDL_SCANCODE_Z; m->key[4] = SDL_SCANCODE_X; m->key[5] = SDL_SCANCODE_C;
            m->key[6] = SDL_SCANCODE_F; m->key[7] = SDL_SCANCODE_V;
        } else {        // tastierino: pugni 7 8 9, calci 4 5 6, 3P = +, 3K = 3
            m->key[0] = SDL_SCANCODE_KP_7; m->key[1] = SDL_SCANCODE_KP_8; m->key[2] = SDL_SCANCODE_KP_9;
            m->key[3] = SDL_SCANCODE_KP_4; m->key[4] = SDL_SCANCODE_KP_5; m->key[5] = SDL_SCANCODE_KP_6;
            m->key[6] = SDL_SCANCODE_KP_PLUS; m->key[7] = SDL_SCANCODE_KP_3;
        }
        return;
    }
    // controller: A=X, B=Y, C=A, D=B (come la Samurai Shodown Collection), RB=A+B, RT=C+D
    m->pad[0] = SDL_CONTROLLER_BUTTON_X;
    m->pad[1] = SDL_CONTROLLER_BUTTON_Y;
    m->pad[2] = SDL_CONTROLLER_BUTTON_A;
    m->pad[3] = SDL_CONTROLLER_BUTTON_B;
    m->pad[4] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    m->pad[6] = PAD_RT;
    if (p == 0) {
        m->key[0] = SDL_SCANCODE_Z; m->key[1] = SDL_SCANCODE_X;
        m->key[2] = SDL_SCANCODE_C; m->key[3] = SDL_SCANCODE_V;
        m->key[4] = SDL_SCANCODE_B; m->key[6] = SDL_SCANCODE_N;
    }
    else {
        m->key[0] = SDL_SCANCODE_KP_4; m->key[1] = SDL_SCANCODE_KP_5;
        m->key[2] = SDL_SCANCODE_KP_1; m->key[3] = SDL_SCANCODE_KP_2;
        m->key[4] = SDL_SCANCODE_KP_6; m->key[6] = SDL_SCANCODE_KP_3;
    }
}

void input_capture_begin(void) { capturing = 1; cap_pad = cap_key = -1; }
void input_flush_start(void) { start_pending[0] = start_pending[1] = 0; enter_pending = 0; }
int input_capture_poll(int *pad, int *key) {
    if (!capturing) return 0;
    if (cap_pad >= 0 || cap_key >= 0) {
        *pad = cap_pad; *key = cap_key;
        capturing = 0;
        return 1;
    }
    return 0;
}
void input_capture_cancel(void) { capturing = 0; }

const char *input_pad_name(int b) {
    switch (b) {
    case -1: return "-";
    case PAD_LT: return "LT";
    case PAD_RT: return "RT";
    case SDL_CONTROLLER_BUTTON_A: return "A";
    case SDL_CONTROLLER_BUTTON_B: return "B";
    case SDL_CONTROLLER_BUTTON_X: return "X";
    case SDL_CONTROLLER_BUTTON_Y: return "Y";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LB";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RB";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "LS";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "RS";
    case SDL_CONTROLLER_BUTTON_BACK: return "BACK";
    case SDL_CONTROLLER_BUTTON_START: return "START";
    case SDL_CONTROLLER_BUTTON_GUIDE: return "GUIDE";
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return "D-UP";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "D-DOWN";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "D-LEFT";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "D-RIGHT";
    default: return "BTN";
    }
}
const char *input_key_name(int k) {
    if (k < 0) return "-";
    const char *n = SDL_GetScancodeName((SDL_Scancode)k);
    return (n && *n) ? n : "KEY";
}
