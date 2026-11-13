"""Genera port/spu/reloc_register.c: registra los 19 jobmods WWS reubicables liftados
por tools/lift_reloc_jobmods.py (base canonica 0x4000) como regiones reloc
(spu_overlay_register_region_reloc). Sin esto, la copia que el job manager pone en
cualquier LS corre en el interprete SPU (25a: todo WWSP interpretado, ~1 s por job).
Span = solo el codigo, [modulo, code_end): los jobs hacen GET de sus datos arriba del
codigo y un GET dentro del span desde otra EA borra la residencia.
Uso: python tools/register_reloc_jobmods.py  (desde la raiz del proyecto)"""
import json, os, struct, sys
sys.path.insert(0, 'ref/ps3recomp/tools')
from lift_jobmods import module_code_end, elf_load_segments

BASE, FIRST_ID = 0x4000, 300
elf = open('game/EBOOT.ELF', 'rb').read()
phoff = struct.unpack_from('>Q', elf, 0x20)[0]
phentsize, phnum = struct.unpack_from('>HH', elf, 0x36)
segs = []
for i in range(phnum):
    p_type, _, off, vaddr, _, filesz = struct.unpack_from('>IIQQQQ', elf, phoff + i * phentsize)
    if p_type == 1 and filesz: segs.append((off, vaddr, filesz))

def file_to_ea(fo):
    for off, vaddr, sz in segs:
        if off <= fo < off + sz: return vaddr + fo - off
    sys.exit(f'offset 0x{fo:X} fuera de los PT_LOAD')

man = json.load(open('port/spu/reloc/manifest.json'))
L = ['/* GENERADO por tools/register_reloc_jobmods.py -- no editar a mano. */',
     '#include <stdint.h>', '#include <stdio.h>',
     'extern void spu_begin_image(int image_id);',
     'extern void spu_overlay_register_region_reloc(uint32_t content_ea, uint32_t span, uint32_t canon, int image_id);']
for m in man: L.append(f'extern void gow3_{m["name"]}_spu_recomp_register(void);')
L += ['', 'void port_reloc_jobmods_register_all(void)', '{']
for i, m in enumerate(man):
    raw = open(f'port/spu/reloc/{m["name"]}/raw.bin', 'rb').read()
    ce = module_code_end(raw, m['name'], BASE)
    fmax = max(e for _, e in m['funcs']) if m['funcs'] else None
    if not ce: print(m['name'], 'sin code_end, salteado'); continue
    span = ce - BASE
    ea = file_to_ea(int(m['file_off'], 16))
    img = FIRST_ID + i
    print(f'{m["name"]} img={img} ea=0x{ea:08X} span=0x{span:X} lift_max=0x{fmax:X} dumped=0x{m["dumped"]:X}')
    L += [f'    spu_begin_image({img}); gow3_{m["name"]}_spu_recomp_register();   /* entry {m["entry"]} */',
          f'    spu_overlay_register_region_reloc(0x{ea:08X}u, 0x{span:X}u, 0x{BASE:X}u, {img});']
L += ['    spu_begin_image(0);',
      '    fprintf(stderr, "[jobmods] %d reloc jobmods registrados\\n", ' + str(len(man)) + ');', '}', '',
      '__attribute__((constructor)) static void port_reloc_jobmods_ctor(void) { port_reloc_jobmods_register_all(); }', '']
open('port/spu/reloc_register.c', 'w', newline='\n').write('\n'.join(L))
print('-> port/spu/reloc_register.c')
