# Strumenti PGM (Martial Masters)
- `build/snkhd_pgm <zip> <bios.zip|-> <regione> <frames> <kx> <ky> <out> [script]` banco del core (vedi testa di src/test_pgm.c):
  comandi `in shot ram w8 p16 spr dump save load dip`; env `FAST SHADOWS SHSPR=lo-hi,h OC DIP RAND RANDFROM START VSAUTO=1/2
  SPRLOG=a-b SPRLOGF=file (lista sprite per frame) LOGB=a1,a2 (byte quando cambiano) WATCH=lo-hi WATCHV=val WSTACK=1
  (scritture RAM 68000 con PC e stack) DUMPROM=file (ROM 68000 decifrata, big-endian, offset = indirizzo-0x100000) HIDE=lo-hi`.
- `build/snkhd_apppg <zip> martmast <jp> <versus> <diff> <ombre> <frames> <kx> <ky> <out> [every]` percorso dell'app (env BIOS=pgm.zip, RAND=1).
- `python3 -I tools/pgm/d68p.py <addr hex> <len hex> <rom.bin>` disassemblato 68000 (capstone).
- `sprflick.py sprlog...` sprite (boff) presenti a frame alterni; `sprflick_blocks.py` idem per blocchi di 0x1000.
