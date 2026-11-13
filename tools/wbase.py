import re, struct, sys
log, elf = sys.argv[1], sys.argv[2]
d = open(elf, "rb").read()
phoff, = struct.unpack_from("<Q", d, 0x20); phnum, = struct.unpack_from("<H", d, 0x38)
segs = []
for i in range(phnum):
    o = phoff + i * 56
    t, fl, off, va, pa, fsz, msz, al = struct.unpack_from("<IIQQQQQQ", d, o)
    if t == 1 and (fl & 1): segs.append((va, off, fsz))
def ins(v):
    for va, off, fsz in segs:
        if va <= v < va + fsz - 3: return struct.unpack_from("<I", d, off + v - va)[0]
    return None
def iscall(w): return w is not None and ((w >> 26) == 0b100101 or (w & 0xFFFFFC1F) == 0xD63F0000)
X, I = [], []
for line in open(log, encoding="utf-8", errors="replace"):
    m = re.search(r"\[wprof\] (X|I) ([0-9a-f]+) (\d+)", line)
    if m: (X if m.group(1) == "X" else I).append(int(m.group(2), 16))
rets = [a for a in I if a not in X]
lo = min(X + I); textend = max(va + fsz for va, _, fsz in segs)
best = []
for base in range((lo - textend) & ~0xFFF, lo + 1, 0x1000):
    h = sum(iscall(ins(a - base - 4)) for a in rets)
    best.append((h, base))
best.sort(reverse=True)
print("rets", len(rets), "top:", [(h, hex(b)) for h, b in best[:5]])
