"""Audit the self-contained returning CPU call and its early privilege guard."""
from pathlib import Path
import re
import struct
import subprocess
import sys

path = Path(sys.argv[1])
elf = path.read_bytes()
if elf[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', elf, 18)[0] != 62:
    raise SystemExit('Expected AMD64 ELF')
offset = struct.unpack_from('<Q', elf, 40)[0]
size, count = struct.unpack_from('<HH', elf, 58)
for i in range(count):
    _, kind, flags, _, _, length = struct.unpack_from('<IIQQQQ', elf, offset+i*size)
    if kind in (4, 9) and length:
        raise SystemExit('Native call must be copyable without relocations')
    if flags & 2 and flags & 1 and length:
        raise SystemExit('Native call must not use writable globals')
if subprocess.check_output(['nm', '-u', str(path)]).strip():
    raise SystemExit('Native call has external imports')
dis = subprocess.check_output(['objdump', '-d', '--no-show-raw-insn', str(path)], text=True)
instructions = []
for line in dis.splitlines():
    match = re.match(r'\s*([0-9a-f]+):\s+(\S+)\s*(.*)', line)
    if match:
        instructions.append((int(match[1], 16), match[2], match[3]))
if len(instructions) < 20 or instructions[0][1:] != ('mov', '%cs,%ax'):
    raise SystemExit('Missing first-instruction CPL guard')
if instructions[1][1:] != ('and', '$0x3,%eax') or instructions[2][1] != 'jne':
    raise SystemExit('CPL must be tested before argument reads and privileged instructions')
target = int(instructions[2][2].split()[0], 16)
at = next((i for i, x in enumerate(instructions) if x[0] == target), None)
if at is None or instructions[at][1:] != ('mov', '$0xfffffff8,%eax') or instructions[at+1][1] != 'ret':
    raise SystemExit('CPL3 path must immediately return ACCESS_DENIED')
if re.search(r'\b(syscall|sysenter|int|sti|hlt|wrmsr|xsetbv|lgdt|lidt|ltr)\b', dis):
    raise SystemExit('Unexpected process, descriptor or persistent state instruction')
for mnemonic in ('xsave64', 'xrstor64', 'fxsave64', 'fxrstor64', 'clts', 'cli'):
    if not any(x[1] == mnemonic for x in instructions):
        raise SystemExit('Missing state-save / returning transition instruction: '+mnemonic)
print('Native returning call: no imports, relocations or writable globals; CPL3 refusal precedes all reads and privileged instructions')
