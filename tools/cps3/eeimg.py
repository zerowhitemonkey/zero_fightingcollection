# eeimg.py "w:v w:v ..." -> righe eew a frame 0 con l'immagine di default di Red Earth + modifiche (entrambe le copie)
import sys
d=[0x0000,0x0101,0x0001,0x0000,0x0100,0x0002,0x0200,0x0000, 0,0,0,0,0,0,0,0, 0x0501,0x0101,0x0001,0x0000,0x9505,0x2302,0x5332,0x4420]
for t in sys.argv[1].split():
    w,v=t.split(':'); d[int(w,16)]=int(v,16)
for i,v in enumerate(d):
    print(f"eew 0 {i} {v}")
    if len(sys.argv)<3: print(f"eew 0 {i+0x18} {v}")
