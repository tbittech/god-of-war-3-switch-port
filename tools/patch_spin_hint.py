#!/usr/bin/env python3
"""Insert PPU_SPIN_HINT(); into pure spin-wait loops of the lifted PPU code.

A pure spin is exactly:
    loc_X:
        ctx->gpr[A] = vm_readN(ctx->gpr[B] + off);   (B != A, nothing written)
        { ...compare... }
        if (...) goto loc_X;
The loop makes no progress of its own: it only waits for another thread/SPU to
change memory. On Horizon a same-priority spinner never lets the other thread
on its core run, so the hint yields the core. Idempotent.

usage: patch_spin_hint.py <dir with ppu_recomp_*.cpp>
"""
import re, sys, pathlib

LAB = re.compile(r'^(loc_[0-9A-F]+):\s*$')
RD = re.compile(r'^\s*ctx->gpr\[(\d+)\] = vm_read(8|16|32|64)\(ctx->gpr\[(\d+)\] \+ -?0x[0-9A-F]+\);\s*$')
CMP = re.compile(r'^\s*\{ (u?int64_t) a = .*\}\s*$')

def main(d):
    total = 0
    for p in sorted(pathlib.Path(d).glob('ppu_recomp_*.cpp')):
        lines = p.read_bytes().decode('latin-1').split('\n')
        out, n, i = [], 0, 0
        while i < len(lines):
            l = lines[i]
            m = LAB.match(l)
            if m and i + 3 < len(lines):
                r = RD.match(lines[i + 1])
                if (r and r.group(1) != r.group(3) and CMP.match(lines[i + 2])
                        and ('goto %s;' % m.group(1)) in lines[i + 3]
                        and lines[i + 3].lstrip().startswith('if (')):
                    out += [l, '        PPU_SPIN_HINT();']
                    n += 1
                    i += 1
                    continue
            out.append(l)
            i += 1
        if n:
            p.write_bytes('\n'.join(out).encode('latin-1'))
            print('%s: %d' % (p.name, n))
            total += n
    print('total spins patched:', total)

if __name__ == '__main__':
    main(sys.argv[1])
