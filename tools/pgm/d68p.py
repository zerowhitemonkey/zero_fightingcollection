import capstone,sys
p=open('sys.argv[3] if len(sys.argv) > 3 else "p.bin"','rb').read()
md=capstone.Cs(capstone.CS_ARCH_M68K,capstone.CS_MODE_BIG_ENDIAN|capstone.CS_MODE_M68K_000)
a=int(sys.argv[1],16); n=int(sys.argv[2],16)
end=a+n
while a<end:
    off=a-0x100000; ok=False
    for i in md.disasm(p[off:off+(end-a)],a):
        print(f"{i.address:06x}: {p[i.address-0x100000:i.address-0x100000+i.size].hex():<20} {i.mnemonic} {i.op_str}")
        a=i.address+i.size; ok=True
    if not ok:
        print(f"{a:06x}: {p[a-0x100000:a-0x100000+2].hex()} dc.w"); a+=2
