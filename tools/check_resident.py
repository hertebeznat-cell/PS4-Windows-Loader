"""Audit a closed RIP-relative AMD64 service image and export linked offsets."""
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib

p = Path(sys.argv[1])
elf = (p / 'resident.elf').read_bytes()
blob = (p / 'resident.bin').read_bytes()
if elf[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', elf, 18)[0] != 62:
    raise SystemExit('Expected little-endian AMD64 ELF')
offset = struct.unpack_from('<Q', elf, 40)[0]
size, count = struct.unpack_from('<HH', elf, 58)
text_extents = []
for i in range(count):
    _, kind, flags, addr, file_offset, length = struct.unpack_from('<IIQQQQ', elf, offset+i*size)
    if kind in (4, 9) and length:
        raise SystemExit('Resident image must not require relocations')
    if flags & 2 and length:
        if flags & 1 or kind == 8 or addr+length > len(blob):
            raise SystemExit('Unexpected writable, missing or BSS section')
        if blob[addr:addr+length] != elf[file_offset:file_offset+length]:
            raise SystemExit('Binary differs from linked section')
        if flags & 4:
            text_extents.append((addr, addr+length))
if subprocess.check_output(['nm', '-u', str(p/'resident.elf')]).strip():
    raise SystemExit('Unresolved imports')
dis = (p/'DISASSEMBLY.txt').read_text()
if re.search(r'\b(syscall|sysenter|cli|sti|hlt|rdmsr|wrmsr)\b|%cr[0-9]', dis):
    raise SystemExit('Unexpected CPU/process instruction')
names = ['raise_tpl', 'restore_tpl', 'allocate_pages', 'free_pages',
         'get_memory_map', 'exit_boot_services', 'calculate_crc32', 'copy_mem', 'set_mem',
         'allocate_pool', 'free_pool', 'install_protocol', 'reinstall_protocol',
         'uninstall_protocol', 'handle_protocol', 'locate_handle', 'locate_protocol',
         'open_volume', 'file_open', 'file_close', 'file_delete', 'file_read', 'file_write',
         'file_get_position', 'file_set_position', 'file_get_info', 'file_set_info', 'file_flush',
         'open_protocol', 'close_protocol']
symbols = {}
for line in subprocess.check_output(['nm', '-n', str(p/'resident.elf')], text=True).splitlines():
    fields = line.split()
    if len(fields) == 3:
        symbols[fields[2]] = (int(fields[0], 16), fields[1])
callbacks = [symbols['pwl_resident_'+n][0] for n in names]
for name, address in zip(names, callbacks):
    if symbols['pwl_resident_'+name][1].lower() != 't' or not any(a <= address < b for a,b in text_extents):
        raise SystemExit('Callback is outside executable text')
binding = symbols['pwl_resident_binding'][0]
if binding % 8 or blob[binding:binding+8] != bytes(8):
    raise SystemExit('Invalid binding slot')
metadata = dict(size=len(blob), binding_offset=binding, callbacks=callbacks,
                crc32=zlib.crc32(blob), mode='PREPARATION_ONLY')
(p/'MANIFEST.json').write_text(json.dumps(metadata, indent=2)+'\n')
(p/'resident_fixture.h').write_text(
    '#include "pwl_resident.h"\nstatic const unsigned char resident_bytes[] = {'+
    ','.join(str(x) for x in blob)+'};\nstatic inline pwl_resident_image_t resident_fixture(void) {\n'+
    'pwl_resident_image_t r={0}; r.bytes=resident_bytes; r.size=sizeof(resident_bytes);\n'+
    f'r.binding_offset={binding}; r.crc32={metadata["crc32"]}U;\n'+
    ''.join(f'r.callbacks[{i}]={address};\n' for i,address in enumerate(callbacks))+
    'return r; }\n')
print(f'Resident image: {len(blob)} bytes, {len(callbacks)} linked callbacks, no imports or relocations')
