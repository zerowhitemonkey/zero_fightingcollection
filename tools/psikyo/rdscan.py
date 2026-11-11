import sys, numpy as np, glob
d=sys.argv[1]
fr=sorted(int(f.split('/')[-1][:-4]) for f in glob.glob(d+'/*.bin'))
M=np.stack([np.frombuffer(open(f'{d}/{f}.bin','rb').read(),np.uint8) for f in fr])
fr=np.array(fr)
def rng(a,b): return (fr>=a)&(fr<=b)
# ingame: 1500..12300, out: <=900 and >=13500
ing=rng(1500,12300); out=(fr<=900)|(fr>=13500)
inv=M[ing]; outv=M[out]
c=(inv.min(0)==inv.max(0)) & (outv.min(0)==outv.max(0)) & (inv[0]!=outv[0])
idx=np.nonzero(c)[0]
print(len(idx))
for i in idx[:200]:
    print(hex(0x06000000+i), inv[0][i], outv[0][i], ' '.join(str(x) for x in M[:,i][::4]))
