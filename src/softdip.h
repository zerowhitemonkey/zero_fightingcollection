#ifndef SNKHD_SOFTDIP_H
#define SNKHD_SOFTDIP_H
#include <stdint.h>
#include <stddef.h>
// Legge la tabella soft-DIP della ROM (header $11A) e imposta i valori forzati:
// difficolta' 1..8, crediti/livello nascosti, sangue attivo, violenza massima, FLASH off.
void softdip_apply(const uint8_t *prom, size_t psz, int difficulty, int region_jp);
#endif
