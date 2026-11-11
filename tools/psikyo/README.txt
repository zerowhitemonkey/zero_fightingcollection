Strumenti di analisi Psikyo PS3-V1 (Daraku Tenshi), usati con il banco di prova build/snkhd_psikyo.
  mkprog.py daraku.zip out.bin      immagine del programma SH-2 big-endian (0x000000 prog_l/h, 0x100000 prog.u16)
  shdis.py prog.bin addr n          disassemblato SH-2 (capstone) con letterali PC-relativi
  shrefs.py prog.bin VAL            istruzioni mov.l @(disp,PC) il cui letterale vale VAL
  fe45.py prog.bin                  scritture del colore 0xFE (palette delle ombre)
  sprlist.py dump                   elenco sprite (dump MAME dump.lua o .vram del banco)
  mkshadowpatch.py prog.bin [c]     patch anti-lampeggio delle ombre (p16 per il banco, 'c' = tabella C di psikyocfg.c)
  gsprobe.py k r                    script per il menu Game Setting del test mode (stato gs.bin)
  rdscan*.py                        confronto di dump di RAM (comando "dump" del banco) per trovare flag di stato
Banco: snkhd_psikyo zip daraku regione frames kx ky out [script]; comandi script: in, shot, cmp, ram, ee, eew, w8,
  p16, save/load (percorsi relativi), vid, pal, prog, dump, vram, sprb, pcmap, rbench (tempo renderer), rawhd.
  env: WATCH/WATCHV, SPRWATCH, PCMAP, SHADOWS, OC, FAST, SHOTEVERY, WAV, MVID/MVK, THREADS, NOIDLE.
Percorso dell'app: snkhd_apppk zip id jp versus diff ombre frames kx ky out [every] (env OC, RAND, P2IDLE).
