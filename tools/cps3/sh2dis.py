import sys, capstone
prog=open(sys.argv[1],'rb').read()
start=int(sys.argv[2],16); end=int(sys.argv[3],16)
md=capstone.Cs(capstone.CS_ARCH_SH, capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_SH2)
md.skipdata=True
off=start-0x06000000
for i in md.disasm(prog[off:end-0x06000000], start):
    extra=''
    if i.mnemonic.startswith('mov.l') and '@(' in i.op_str and 'pc' in i.op_str:
        # mov.l @(disp,pc),rn
        try:
            disp=int(i.op_str.split('(')[1].split(',')[0],0)
            addr=((i.address+4)&~3)+disp
            val=int.from_bytes(prog[addr-0x06000000:addr-0x06000000+4],'big'); extra='   ; [%08x]=%08x'%(addr,val)
        except Exception as e: pass
    if i.mnemonic.startswith('mov.w') and '@(' in i.op_str and 'pc' in i.op_str:
        try:
            disp=int(i.op_str.split('(')[1].split(',')[0],0)
            addr=i.address+4+disp
            val=int.from_bytes(prog[addr-0x06000000:addr-0x06000000+2],'big'); extra='   ; [%08x]=%04x'%(addr,val)
        except: pass
    if not extra and i.mnemonic in ('mov.l','mov.w') and i.op_str.startswith('0x'):
        try:
            a=int(i.op_str.split(',')[0],16); n=4 if i.mnemonic=='mov.l' else 2
            extra='   ; =%0*x'%(n*2,int.from_bytes(prog[a-0x06000000:a-0x06000000+n],'big'))
        except: pass
    print('%08x: %-8s %s%s'%(i.address,i.mnemonic,i.op_str,extra))
