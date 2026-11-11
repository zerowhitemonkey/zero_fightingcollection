# Daraku: immagine del programma SH-2 in big-endian (0x000000-0x0fffff = prog_l/h, 0x100000-0x1fffff = prog.u16 @0x02000000)
import zipfile, sys
z = zipfile.ZipFile(sys.argv[1])
pl, ph, pm = z.read('4_prog_l.u18'), z.read('3_prog_h.u17'), z.read('prog.u16')
b = bytearray(0x200000)
b[1:0x100000:2] = pl; b[0:0x100000:2] = ph; b[0x100000:] = pm
for i in range(0, 0x100000, 4): b[i+1], b[i+2] = b[i+2], b[i+1]
for i in range(0, 0x200000, 2): b[i], b[i+1] = b[i+1], b[i]
for i in range(0, 0x200000, 4): b[i:i+4] = b[i:i+4][::-1]
# ora: u32 little-endian in b[i] = long SH-2 -> big-endian = byte invertiti per long
be = bytearray(0x200000)
for i in range(0, 0x200000, 4): be[i:i+4] = b[i:i+4][::-1]
open(sys.argv[2], 'wb').write(be)
print('reset PC %08x SP %08x' % (int.from_bytes(be[0:4],'big'), int.from_bytes(be[4:8],'big')))
