#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=${1:?SDK directory required}
id=${2:?commit id required}
case "$sdk" in /*) ;; *) sdk="$(pwd)/$sdk" ;; esac
sh tools/build_kernel_dump.sh "$sdk" "$id"
out=build/workspace-probe
mkdir -p "$out"
gcc -I"$sdk/libPS4/include" -Ipayload -Os -std=c11 -ffreestanding \
 -fno-builtin -fno-stack-protector -nostartfiles -nostdlib -Wall -Wextra -Werror \
 -ffunction-sections -fdata-sections -masm=intel -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
 -DPWL_WORKSPACE_PROBE "-DPS4WL_BUILD_ID=\"$id\"" -c payload/memory_probe.c -o "$out/probe.o"
objects=""
for unit in main pe_loader handoff paging ps4_memory firmware_memory firmware_media native_workspace freestanding; do
 gcc -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns \
  -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -mno-red-zone -mgeneral-regs-only \
  -fpie -fPIC -Wall -Wextra -Werror -Iloader/include -c "loader/src/$unit.c" -o "$out/$unit.o"
 objects="$objects $out/$unit.o"
done
gcc -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns \
 -ffunction-sections -fdata-sections -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
 -Wall -Wextra -Werror -DPWL_WORKSPACE_DIAGNOSTIC -Iloader/include -Ipayload \
 -c payload/workspace_probe_core.c -o "$out/workspace_probe_core.o"
ld -r $objects "$out/workspace_probe_core.o" -o "$out/core.o"
# SDK has pointer globals named memcpy/memset/memmove; keep native functions distinct.
objcopy --redefine-sym memcpy=pwl_native_memcpy --redefine-sym memset=pwl_native_memset \
 --redefine-sym memmove=pwl_native_memmove "$out/core.o"
gcc "$sdk/libPS4/crt0.s" "$out/probe.o" "$out/core.o" -nostartfiles -nostdlib -pie \
 -Wl,-T,payload/probe_linker.ld -Wl,--build-id=none -Wl,--gc-sections -Wl,-z,noexecstack \
 -L"$sdk/libPS4" -lPS4 -o "$out/PS4WindowsLoader-Workspace-Probe.elf"
test -z "$(nm -u "$out/PS4WindowsLoader-Workspace-Probe.elf")"
objcopy --set-section-flags .bss=alloc,load,contents -O binary \
 "$out/PS4WindowsLoader-Workspace-Probe.elf" "$out/PS4WindowsLoader-Workspace-Probe.bin"
python3 - "$out" <<'PY'
import struct,sys
from pathlib import Path
p=Path(sys.argv[1]);e=(p/'PS4WindowsLoader-Workspace-Probe.elf').read_bytes();b=(p/'PS4WindowsLoader-Workspace-Probe.bin').read_bytes()
if struct.unpack_from('<H',e,16)[0]!=3:raise SystemExit('Raw payload requires PIE/DYN linking; EXEC can introduce absolute SDK addresses')
o=struct.unpack_from('<Q',e,40)[0];sz,n=struct.unpack_from('<HH',e,58)
for i in range(n):
    _,typ,flags,addr,off,size=struct.unpack_from('<IIQQQQ',e,o+i*sz)
    if typ in (4,9) and size:raise SystemExit('Raw payload must not require relocations')
    if flags&2:
        if addr+size>len(b):raise SystemExit('Allocated section omitted')
        if typ==8 and b[addr:addr+size]!=bytes(size):raise SystemExit('BSS not zero-filled')
PY
rm -f "$out"/*.o
printf '%s\n' "$id" > "$out/COMMIT.txt"
cp docs/WORKSPACE_PROBE.md "$out/README.md"
(cd "$out" && sha256sum *.bin *.elf > SHA256SUMS.txt)
