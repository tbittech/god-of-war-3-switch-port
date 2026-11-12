"""Lifta los jobmods WWS v5 (base 0, relocables) del EBOOT de GoW3.
Cada uno: wrap como SPU ELF en base nominal 0x4000, spu_lifter --auto-functions --reloc.
Uso: python tools/lift_reloc_jobmods.py  (desde la raiz del proyecto)"""
import json, os, struct, subprocess, sys, hashlib
T = 'ref/ps3recomp/tools'; BASE = 0x4000
d = open('game/EBOOT.ELF', 'rb').read()
idx = json.load(open('analysis/spu_jobmods/index.json'))
man = []
for o in idx:
    s = int(o['off'], 16)
    base_hdr, ver = struct.unpack_from('>II', d, s + 0x28)
    if base_hdr != 0: continue            # v4 con base fija: los lifta lift_jobmods.py
    entry, image = struct.unpack_from('>II', d, s + 0x10)
    raw = d[s:s + o['dumped']]
    name = f"rjm_{s:06x}"
    out = f'port/spu/reloc/{name}'; os.makedirs(out, exist_ok=True)
    open(f'{out}/raw.bin', 'wb').write(raw)
    r = subprocess.run([sys.executable, f'{T}/wrap_spu_elf.py', f'{out}/raw.bin', '--out', f'{out}/mod.elf',
                        '--base', hex(BASE), '--entry', hex(BASE + entry)], capture_output=True, text=True)
    if r.returncode: print(name, 'wrap FAIL', r.stderr[-300:]); continue
    r = subprocess.run([sys.executable, f'{T}/spu_lifter.py', '--auto-functions', f'{out}/mod.elf', '-o', out,
                        '--symbol-prefix', f'gow3_{name}_', '--reloc', '--report', f'{out}/report.json'],
                       capture_output=True, text=True)
    if r.returncode: print(name, 'lift FAIL', r.stderr[-400:]); continue
    rep = json.load(open(f'{out}/report.json'))
    e = dict(name=name, file_off=o['off'], entry=hex(BASE + entry), image_size=hex(image), dumped=len(raw),
             ver=ver, sha1=hashlib.sha1(raw).hexdigest()[:12], funcs=rep.get('functions'), unsupported=rep.get('unsupported'))
    man.append(e); print(e)
json.dump(man, open('port/spu/reloc/manifest.json', 'w'), indent=1)
print(len(man), 'modulos; unsupported total', sum(m['unsupported'] or 0 for m in man))
