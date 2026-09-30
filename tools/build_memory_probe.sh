#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=${1:?SDK directory required}
id=${2:?commit id required}
case "$sdk" in /*) ;; *) sdk="$(pwd)/$sdk" ;; esac
sh tools/build_kernel_dump.sh "$sdk" "$id"
out=build/memory-probe
mkdir -p "$out"
gcc -I"$sdk/libPS4/include" -Ipayload -Os -std=c11 -ffreestanding \
 -fno-builtin -fno-stack-protector -nostartfiles -nostdlib -Wall -Wextra -Werror \
 -ffunction-sections -fdata-sections -masm=intel -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
 "-DPS4WL_BUILD_ID=\"$id\"" -c payload/memory_probe.c -o "$out/probe.o"
gcc "$sdk/libPS4/crt0.s" "$out/probe.o" -nostartfiles -nostdlib -no-pie \
 -Wl,-T,payload/probe_linker.ld -Wl,--build-id=none -Wl,--gc-sections -Wl,-z,noexecstack \
 -L"$sdk/libPS4" -lPS4 -o "$out/PS4WindowsLoader-Memory-Probe.elf"
test -z "$(nm -u "$out/PS4WindowsLoader-Memory-Probe.elf")"
objcopy --set-section-flags .bss=alloc,load,contents -O binary \
 "$out/PS4WindowsLoader-Memory-Probe.elf" "$out/PS4WindowsLoader-Memory-Probe.bin"
python3 - "$out" <<'PY'
import struct,sys
from pathlib import Path
p=Path(sys.argv[1]);e=(p/'PS4WindowsLoader-Memory-Probe.elf').read_bytes();b=(p/'PS4WindowsLoader-Memory-Probe.bin').read_bytes()
o=struct.unpack_from('<Q',e,40)[0];sz,n=struct.unpack_from('<HH',e,58)
for i in range(n):
    _,typ,flags,addr,off,size=struct.unpack_from('<IIQQQQ',e,o+i*sz)
    if typ in (4,9) and size:raise SystemExit('Raw payload must not require relocations')
    if flags&2:
        if addr+size>len(b):raise SystemExit('Allocated section omitted')
        if typ==8 and b[addr:addr+size]!=bytes(size):raise SystemExit('BSS not zero-filled')
PY
rm -f "$out/probe.o"
printf '%s\n' "$id" > "$out/COMMIT.txt"
cp docs/MEMORY_PROBE.md "$out/README.md"
(cd "$out" && sha256sum *.bin *.elf > SHA256SUMS.txt)
