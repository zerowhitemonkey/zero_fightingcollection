# genera uno script del banco CPS3: seq.py start_frame "TOKENS" [extra righe]
# token: U D L R 1..6 (tasti) S (start) C (coin) T (test) W<n> (attesa n frame) P<frame> (shot) E (ee)
import sys
f = int(sys.argv[1]); toks = sys.argv[2].split()
bits = {'U':1,'D':2,'L':4,'R':8,'1':16,'2':32,'3':64,'4':128,'5':256,'6':512}
sysb = {'S':1,'C':4,'T':16}
out = []
for t in toks:
    if t[0] == 'W': f += int(t[1:]); continue
    if t[0] == 'P': out.append(f"shot {f}"); continue
    if t[0] == 'E': out.append(f"ee {f}"); continue
    port = 0
    if t.startswith('2:'): port = 1; t = t[2:]
    j = 0; s = 0
    for ch in t:
        if ch in bits: j |= bits[ch]
        if ch in sysb: s |= sysb[ch]
    out.append(f"in {f} {port} {j} {s}"); f += int(__import__("os").environ.get("HOLD","5"))
    out.append(f"in {f} {port} 0 0"); f += int(__import__("os").environ.get("GAP","10"))
out.append(f"shot {f+30}")
out.append(f"ee {f+30}")
print("\n".join(out))
print(f"# end {f+31}", file=sys.stderr)
