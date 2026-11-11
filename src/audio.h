#ifndef SNKHD_AUDIO_H
#define SNKHD_AUDIO_H
#include <stdint.h>
#include <stddef.h>
int  audio_init(void);
void audio_quit(void);
void audio_set_input_rate(double rate);   // frequenza nominale dei campioni in ingresso
void audio_push(const int16_t *in, size_t frames);
void audio_pause(int p);
void audio_clear(void);
#endif
