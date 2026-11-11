# genera lo script: stato gs.bin, voce k (giu' k volte), destra r volte, shot, poi su Exit e conferma, ee
import sys
k, r = int(sys.argv[1]), int(sys.argv[2])
L = ['load 0 gs.bin']; f = 10
def press(m):
    global f
    L.append('in %d 0 %d 0' % (f, m)); L.append('in %d 0 0 0' % (f + 6)); f += 14
for _ in range(k): press(2)
for _ in range(r): press(8)
L.append('shot %d' % (f + 10)); f += 20
for _ in range(11 - k): press(2)
press(16); f += 30; press(16)
L.append('ee %d' % (f + 60)); L.append('shot %d' % (f + 60))
print('\n'.join(L)); sys.stderr.write('%d\n' % (f + 61))
