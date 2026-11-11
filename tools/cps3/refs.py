# refs.py prog.bin value : trova i mov.l @(disp,pc) che caricano 'value' e stampa le istruzioni successive
import sys, capstone
prog=open(sys.argv[1],'rb').read(); val=int(sys.argv[2],16); n=int(sys.argv[3]) if len(sys.argv)>3 else 8
md=capstone.Cs(capstone.CS_ARCH_SH, capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_SH2); md.skipdata=True
b=val.to_bytes(4,'big'); i=-1
while True:
    i=prog.find(b,i+1)
    if i<0: break
    if i%4: continue
    L=i
    for pc in range(max(0,L-1020), L, 2):
        op=int.from_bytes(prog[pc:pc+2],'big')
        if op>>12==0xD and ((pc+4)&~3)+(op&0xff)*4==L:
            out=[]
            for ins in md.disasm(prog[pc:pc+2*n], 0x06000000+pc):
                out.append('%s %s'%(ins.mnemonic,ins.op_str))
            print('%08x: '%(0x06000000+pc)+' | '.join(out))
