/* SNKHD: supporto CD rimosso (solo cartucce). Stub. */
#include <stdint.h>
#include "geo_disc.h"
int geo_disc_open(const char *path) { (void)path; return 0; }
void geo_disc_close(void) {}
int geo_disc_read_sector(uint32_t l, uint8_t *b) { (void)l; (void)b; return 0; }
int geo_disc_read_audio(uint32_t l, int16_t *b) { (void)l; (void)b; return 0; }
unsigned geo_disc_num_tracks(void) { return 0; }
int geo_disc_track_is_audio(unsigned t) { (void)t; return 0; }
uint32_t geo_disc_track_start(unsigned t) { (void)t; return 0; }
uint32_t geo_disc_track_frames(unsigned t) { (void)t; return 0; }
uint32_t geo_disc_leadout(void) { return 0; }
void geo_disc_lba_to_msf(uint32_t lba, uint8_t *m, uint8_t *s, uint8_t *f) {
    lba += 150; *m = lba / 4500; *s = (lba / 75) % 60; *f = lba % 75; }
uint32_t geo_disc_msf_to_lba(uint8_t m, uint8_t s, uint8_t f) { return m * 4500 + s * 75 + f - 150; }
