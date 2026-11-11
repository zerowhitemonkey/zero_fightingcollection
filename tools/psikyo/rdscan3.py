import numpy as np, glob,sys
exec(open('tools/rdscan2.py').read().split('p1=cand')[0])
inm=sel((2700,12900)); outm=sel((0,2000),(14100,20000))
for bit in [1,2,4,8,16,32,64,128]:
    B=(M&bit)!=0
    ok=B[inm].all(0) & (~B[outm]).all(0)
    for a in np.nonzero(ok)[0]: print('P2 bit',bit,hex(0x06000000+a), ' '.join('%02x'%x for x in M[:,a][::3]))
inm=sel((1140,5500)); outm=sel((0,900),(6300,20000))
for bit in [1,2,4,8,16,32,64,128]:
    B=(M&bit)!=0
    ok=B[inm].all(0) & (~B[outm]).all(0)
    for a in np.nonzero(ok)[0]: print('P1 bit',bit,hex(0x06000000+a), ' '.join('%02x'%x for x in M[:,a][::3]))
