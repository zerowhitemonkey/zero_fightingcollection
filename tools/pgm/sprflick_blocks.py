import sys, collections
frames={}
for line in open(sys.argv[1]):
    p=line.split()
    f=int(p[1]); s=set()
    for e in p[2:]:
        b,pal,x,y,w,h=e.split(':'); s.add((int(b,16),int(pal,16)))
    frames[f]=s
fs=sorted(frames)
alt=collections.Counter(); pres=collections.Counter(); pals=collections.defaultdict(set)
for f in fs:
    for k in frames[f]: pres[k[0]]+=1; pals[k[0]].add(k[1])
    if f+2 in frames and f+1 in frames:
        for k in frames[f]:
            if k in frames[f+2] and k not in frames[f+1]: alt[k[0]]+=1
# group by 0x1000 blocks
blk=collections.Counter(); blkp=collections.Counter()
for b,c in alt.items(): blk[b>>12]+=c
for b,c in pres.items(): blkp[b>>12]+=c
for k,c in sorted(blk.items(), key=lambda x:-x[1])[:25]:
    print("blk %03x000 alt %6d pres %6d ratio %.2f"%(k,c,blkp[k],c/blkp[k]))
