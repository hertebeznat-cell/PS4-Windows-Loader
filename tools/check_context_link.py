#!/usr/bin/env python3
"""Reject a context probe that uses the ELF's fixed callback address."""
import re
import os
import subprocess
import sys
import struct
from pathlib import Path

if len(sys.argv) not in (2, 3):
    raise SystemExit("usage: check_context_link.py probe.elf [probe.bin]")
prefix = os.environ.get("PWL_CAPTURE_PREFIX", "pwl_context")
elf = sys.argv[1]
symbols = subprocess.check_output(["nm", "-n", elf], text=True)
addresses = {}
for line in symbols.splitlines():
    fields = line.split()
    if len(fields) == 3 and fields[2] in {prefix + "_capture", prefix + "_capture_end"}:
        addresses[fields[2]] = int(fields[0], 16)
if set(addresses) != {prefix + "_capture", prefix + "_capture_end"}:
    raise SystemExit("missing capture symbols")
length = addresses[prefix + "_capture_end"] - addresses[prefix + "_capture"]
if not 0 < length <= 4096:
    raise SystemExit(f"invalid capture extent in ELF: {length}")
main = subprocess.check_output(["objdump", "-d", "--disassemble=main", elf], text=True)
relative_targets = {
    int(match.group(1), 16)
    for match in re.finditer(r"\blea\s+[^\n]*\(%rip\)[^\n]*# ([0-9a-f]+) <", main)
}
for symbol, address in addresses.items():
    if address not in relative_targets:
        raise SystemExit(f"{symbol} address is not resolved relative to RIP")
    if re.search(r"\bmov(?:abs)?\s+\$0x" + f"{address:x}" + r"\b", main):
        raise SystemExit(f"{symbol} has a fixed address in main")
print(f"context callback: {length} bytes; both addresses use RIP-relative LEA")

if len(sys.argv) == 3:
    # Raw payload senders do not implement ELF NOBITS/.bss initialization.
    # Check the actual shipped bytes, not only the ELF's memory size.
    data = Path(elf).read_bytes()
    raw = Path(sys.argv[2]).read_bytes()
    header = struct.unpack_from("<16sHHIQQQIHHHHHH", data)
    if header[0][:6] != b"\x7fELF\x02\x01" or header[11] != 64:
        raise SystemExit("expected ELF64 little-endian section headers")
    sections = [struct.unpack_from("<IIQQQQIIQQ", data, header[6] + i * 64)
                for i in range(header[12])]
    strings = sections[header[13]]
    names = data[strings[4]:strings[4] + strings[5]]
    selected = {}
    for section in sections:
        name = names[section[0]:].split(b"\0", 1)[0].decode("ascii")
        if name in {".text", ".data", ".rodata", ".bss"} and section[5]:
            selected[name] = section
    if not {".text", ".rodata", ".bss"}.issubset(selected):
        raise SystemExit("missing raw payload sections")
    base = min(s[3] for s in selected.values())
    end = max(s[3] + s[5] for s in selected.values())
    if len(raw) != end - base:
        raise SystemExit("raw payload does not cover all code/data/BSS bytes")
    for name, section in selected.items():
        expected = bytes(section[5]) if section[1] == 8 else data[section[4]:section[4] + section[5]]
        start = section[3] - base
        if raw[start:start + section[5]] != expected:
            raise SystemExit(f"raw payload section mismatch: {name}")
    print("raw payload: all sections present, including zero-initialized BSS")

