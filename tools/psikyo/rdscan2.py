import numpy as np, glob
def load(d):
    fr=sorted(int(f.split('/')[-1][:-4]) for f in glob.glob(d+'/*.bin'))
    return np.array(fr), np.stack([np.frombuffer(open(f'{d}/{f}.bin','rb').read(),np.uint8) for f in fr])
fr,M=load('rd3')
def sel(*rs): 
    m=np.zeros(len(fr),bool)
    for a,b in rs: m|=(fr>=a)&(fr<=b)
    return m
def cand(inm,outm):
    i=M[inm]; o=M[outm]
    # in: all nonzero-equal? allow in-set values all != any out-set value; out constant
    oc=(o.min(0)==o.max(0))
    ok=oc.copy()
    for r in i: ok&=(r!=o[0])
    return set(np.nonzero(ok)[0])
p1=cand(sel((1140,5500)), sel((0,900),(6300,20000)))
p2=cand(sel((2700,12900)), sel((0,2000),(14100,20000)))
print(len(p1),len(p2))
for a in sorted(p1):
    for b in sorted(p2):
        if 0<abs(b-a)<=0x400: print(hex(0x06000000+a),hex(0x06000000+b), M[:,a][::3], M[:,b][::3])
print([hex(0x06000000+a) for a in sorted(p1)], [hex(0x06000000+a) for a in sorted(p2)])
for a in sorted(p1|p2): print(hex(0x06000000+a), ' '.join(str(x) for x in M[:,a][::3]))
print(fr[::3])
