// SNKHD - cattura dello schermo per i giochi PC esterni (Windows)
#ifndef SNKHD_EXTCAP_H
#define SNKHD_EXTCAP_H
#ifdef _WIN32
#include <windows.h>
int  extcap_open(HWND self, char *err, int errlen);        // duplicazione del monitor della finestra dell'app
void extcap_monitor(RECT *r);                              // area del monitor catturato
int  extcap_frame(const RECT *src, unsigned *tex, int *w, int *h);   // copia l'area src; 1 = immagine valida
void extcap_close(void);
#endif
#endif
