// SNKHD - audio: ricampionamento (speex) con controllo dinamico della frequenza
#include <SDL.h>
#include <string.h>
#include "audio.h"
#include "speex_resampler.h"

#define OUT_RATE 48000
#define RING (OUT_RATE)          // 1 s di buffer circolare (frame stereo)
#define TARGET (OUT_RATE / 20)   // 50 ms di riempimento desiderato

static SDL_AudioDeviceID dev;
static int16_t ring[RING * 2];
static volatile unsigned rd, wr;  // indici in frame
static SpeexResamplerState *rs;
static double in_rate = 55555.5;
static int paused = 1;
static int16_t tmp[8192 * 2];

static unsigned fill(void) { return (wr - rd + RING) % RING; }

static void cb(void *ud, Uint8 *stream, int len) {
    (void)ud;
    int16_t *o = (int16_t*)stream;
    int n = len / 4;
    unsigned f = fill();
    int i = 0;
    for (; i < n && f; ++i, --f) {
        o[i * 2] = ring[rd * 2];
        o[i * 2 + 1] = ring[rd * 2 + 1];
        rd = (rd + 1) % RING;
    }
    for (; i < n; ++i) o[i * 2] = o[i * 2 + 1] = 0;
}

int audio_init(void) {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = OUT_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = cb;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) return 0;
    int err;
    rs = speex_resampler_init(2, (spx_uint32_t)in_rate, OUT_RATE, 5, &err);
    return 1;
}

void audio_quit(void) {
    if (dev) SDL_CloseAudioDevice(dev);
    if (rs) speex_resampler_destroy(rs);
    dev = 0; rs = NULL;
}

void audio_set_input_rate(double r) { in_rate = r; }

void audio_push(const int16_t *in, size_t frames) {
    if (!dev || !rs || paused) return;
    // Controllo dinamico: +-0.5% in base al riempimento del buffer
    double dev_fill = ((double)TARGET - (double)fill()) / TARGET;
    if (dev_fill > 1) dev_fill = 1;
    if (dev_fill < -1) dev_fill = -1;
    // passi da 0.05% per non ricalcolare il filtro a ogni frame
    int step = (int)(dev_fill * 10.0 + (dev_fill >= 0 ? 0.5 : -0.5));
    // Rapporto in Hz interi (<= ~56000): con numeratore e denominatore x10 (es. 372869/477600, CPS3 e Psikyo
    // a 37286,9 Hz) la conversione della fase in speex_resampler_set_rate_frac va in overflow, la funzione esce
    // a meta' aggiornamento e il ricampionatore legge fuori dalla tabella (crash). Fuori da questi limiti o in
    // caso di errore si ricrea il ricampionatore.
    spx_uint32_t num = (spx_uint32_t)(in_rate + 0.5);
    spx_uint32_t den = (spx_uint32_t)(OUT_RATE * (1.0 + 0.0005 * step) + 0.5);
    if (speex_resampler_set_rate_frac(rs, num, den, num, OUT_RATE) != RESAMPLER_ERR_SUCCESS) {
        int err;
        speex_resampler_destroy(rs);
        rs = speex_resampler_init_frac(2, num, den, num, OUT_RATE, 5, &err);
        if (!rs) return;
    }
    spx_uint32_t inl = (spx_uint32_t)frames, outl = 8192;
    speex_resampler_process_interleaved_int(rs, in, &inl, tmp, &outl);
    SDL_LockAudioDevice(dev);
    for (spx_uint32_t i = 0; i < outl; ++i) {
        if (fill() >= RING - 1) break;
        ring[wr * 2] = tmp[i * 2];
        ring[wr * 2 + 1] = tmp[i * 2 + 1];
        wr = (wr + 1) % RING;
    }
    SDL_UnlockAudioDevice(dev);
}

void audio_pause(int p) {
    paused = p;
    if (dev) SDL_PauseAudioDevice(dev, p);
}

void audio_clear(void) {
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    rd = wr = 0;
    SDL_UnlockAudioDevice(dev);
    if (rs) speex_resampler_reset_mem(rs);
}
