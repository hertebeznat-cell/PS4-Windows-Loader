#!/usr/bin/env python3
"""Reject a context probe that uses the ELF's fixed callback address."""
import re
import subprocess
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: check_context_link.py probe.elf")
elf = sys.argv[1]
symbols = subprocess.check_output(["nm", "-n", elf], text=True)
addresses = {}
for line in symbols.splitlines():
    fields = line.split()
    if len(fields) == 3 and fields[2] in {"pwl_context_capture", "pwl_context_capture_end"}:
        addresses[fields[2]] = int(fields[0], 16)
if set(addresses) != {"pwl_context_capture", "pwl_context_capture_end"}:
    raise SystemExit("missing capture symbols")
length = addresses["pwl_context_capture_end"] - addresses["pwl_context_capture"]
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
