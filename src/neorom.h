// Caricamento dei giochi Neo Geo direttamente dai romset MAME (.zip) messi dall'utente in roms\.
// I file si riconoscono dal contenuto (CRC32 + dimensione letti dall'indice degli zip), non dal nome:
// funzionano set split / merged / non-merged e zip rinominati. L'immagine .neo (formato Geolith) si costruisce
// in memoria come facevano tools/build_neo_mame.py e build_neo_enc.py (decrittazione compresa) con le patch
// anti-flicker del progetto, e si salva facoltativamente in una cache.
#ifndef NEOROM_H
#define NEOROM_H
#include <stddef.h>
#include <stdint.h>

// indicizza tutti gli .zip di una cartella (si puo' richiamare per aggiornare)
void neorom_scan(const char *romsdir);
// 1 se l'id e' un gioco Neo Geo gestito da neorom (anche se mancano i file)
int neorom_known(const char *id);
// numero di file mancanti per il gioco (0 = tutto presente, -1 = gioco sconosciuto)
int neorom_missing(const char *id);
// romset MAME necessari ("samsho5 + samsho5a") e quelli con file mancanti (per ROM CHECK)
const char *neorom_sets(const char *id);
int neorom_missing_sets(const char *id, char *out, size_t n);
// numero di file del romset
int neorom_total(const char *id);
// elenco dei file mancanti ("crc32 dimensione" per riga) per la schermata ROM CHECK
int neorom_missing_list(const char *id, char *out, size_t n);
// costruisce l'immagine .neo in memoria (malloc). cachedir NULL = niente cache. Ritorna NULL in caso di errore
// (vedi neorom_error()).
void *neorom_build(const char *id, const char *cachedir, size_t *sz);
const char *neorom_error(void);
// elimina i file di cache creati da neorom_build
void neorom_cache_clear(const char *dir);

// ricerca generica di un file per CRC/dimensione in tutti gli zip indicizzati (per gli altri sistemi):
// ritorna il percorso dello zip che lo contiene oppure NULL
const char *neorom_zip_with(uint32_t crc, uint32_t size);
// gli zip indicizzati: numero e percorso (per l'identificazione dei set degli altri sistemi)
int neorom_zip_count(void);
const char *neorom_zip_path(int i);
#endif
