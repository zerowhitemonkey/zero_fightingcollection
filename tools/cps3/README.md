# Strumenti CPS3 (banco di prova `build/snkhd_cps3`)
- `seq.py <frame> "TOKENS"`: genera righe `in` per il banco (U D L R 1-6 S C T, `2:` = P2, Wn attesa, P shot, E eeprom). HOLD/GAP = frame premuto/rilasciato.
- `eeimg.py "w:v ..."` / `eeimg_r.py <regione> "w:v ..."`: immagine EEPROM di Red Earth (default + modifiche) come righe `eew` al frame 0.
- `view.sh` / `view2.sh`: apre il test menu (CONFIGURATION, pagina SYSTEM/GAME) con una EEPROM modificata e salva lo screenshot.
- Variabili del banco: `FAST=1`, `SHOTEVERY`, `DUMPEVERY`+`DUMPPRE` (RAM 512 KB), `SSPAL` (palette dello strato testo presenti), `WAV` (audio grezzo), `ONLY`/`SKIP` (voci della lista sprite), `WATCH=lo-hi` (scritture RAM con PC), `OC`.
- Comandi script: `in shot ram ee eew w8 save load spr ss dump cmp` (vedi `src/test_cps3.c`).
