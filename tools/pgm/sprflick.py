import sys, collections
alt=collections.Counter(); pres=collections.Counter(); info={}
for fn in sys.argv[1:]:
    frames={}
    for line in open(fn):
        p=line.split(); f=int(p[1]); s={}
        for e in p[2:]:
            b,pal,x,y,w,h=e.split(':'); s[int(b,16)]=(pal,w,h)
        frames[f]=s
    for f in frames:
        for k in frames[f]: pres[k]+=1; info[k]=frames[f][k]
        if f+1 in frames and f+2 in frames:
            for k in frames[f]:
                if k in frames[f+2] and k not in frames[f+1]: alt[k]+=1
for k,c in sorted(alt.items(), key=lambda x:-x[1]):
    if c>=20 and c/pres[k]>0.2: print(hex(k), c, pres[k], info[k])
