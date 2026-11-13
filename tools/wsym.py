import re, subprocess, sys, collections
log, elf = sys.argv[1], sys.argv[2]; BASE = int(sys.argv[3], 16)
A2L = r"C:/devkitPro/devkitA64/bin/aarch64-none-elf-addr2line.exe"
rows = []
for line in open(log, encoding="utf-8", errors="replace"):
    m = re.search(r"\[wprof\] (X|I) ([0-9a-f]+) (\d+)", line)
    if m: rows.append((m.group(1), int(m.group(2), 16) - BASE, int(m.group(3))))
    elif "[wprof]" in line: print(line.rstrip())
if not rows: sys.exit("no wprof rows")
addrs = sorted({a for _, a, _ in rows})
out = subprocess.run([A2L, "-f", "-C", "-e", elf] + ["%x" % a for a in addrs],
                     capture_output=True, text=True).stdout.splitlines()
sym = {}
for i, a in enumerate(addrs):
    fn, loc = out[2*i], out[2*i+1]
    sym[a] = (fn, loc.split("/")[-1])
tot = collections.Counter(c for k, _, c in rows if k == "X")
for kind in ("X", "I"):
    agg = collections.Counter(); where = {}
    for k, a, c in rows:
        if k != kind: continue
        fn, loc = sym[a]
        agg[fn] += c; where.setdefault(fn, loc)
    print(f"== {kind} por funcion ==")
    for fn, c in agg.most_common(40 if kind == "X" else 60):
        print(f"{c:6d}  {fn[:90]}  [{where[fn]}]")
