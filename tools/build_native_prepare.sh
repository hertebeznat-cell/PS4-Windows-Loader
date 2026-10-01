#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=${1:?SDK directory required}
id=${2:?build id required}
case "$sdk" in /*) ;; *) sdk="$(pwd)/$sdk" ;; esac
sh tools/build_kernel_dump.sh "$sdk" "$id"
sh tools/build_resident.sh
out=build/native-prepare
mkdir -p "$out"
gcc -I"$sdk/libPS4/include" -Iloader/include -Ibuild/resident -Ipayload \
 -Os -std=c11 -ffreestanding -fno-builtin -fno-stack-protector \
 -nostartfiles -nostdlib -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
 -fshort-wchar -masm=intel -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
 "-DPS4WL_BUILD_ID=\"$id\"" -c payload/native_prepare.c -o "$out/entry.o"
objects=""
for unit in main time cpu_environment cpu_state native_call pe_loader handoff paging ps4_binding ps4_profile ps4_reader ps4_memory \
 firmware_memory firmware_files firmware_media efi_tables graphics resident_workspace \
 native_workspace acpi acpi_configuration transition_map freestanding; do
 gcc -Iloader/include -Os -std=c11 -ffreestanding -fno-builtin -fno-stack-protector \
  -fno-tree-loop-distribute-patterns -fno-asynchronous-unwind-tables \
  -Wall -Wextra -Werror -ffunction-sections -fdata-sections -mno-red-zone \
  -mgeneral-regs-only -fpie -fPIC -c "loader/src/$unit.c" -o "$out/$unit.o"
 objects="$objects $out/$unit.o"
done
ld -r $objects -o "$out/core.o"
objcopy --redefine-sym memcpy=pwl_native_memcpy --redefine-sym memset=pwl_native_memset \
 --redefine-sym memmove=pwl_native_memmove "$out/core.o"
# Complete linked image, including BSS, is pinned before entering the callback.
cp payload/probe_linker.ld "$out/linker.x"
gcc -c -m64 payload/probe_start.S -o "$out/start.o"
gcc "$out/start.o" "$out/entry.o" "$out/core.o" -nostartfiles -nostdlib \
 -fpie -fPIC -Wl,-T,"$out/linker.x" -Wl,--build-id=none -Wl,--gc-sections \
 -Wl,-z,noexecstack -L"$sdk/libPS4" -lPS4 -o "$out/PS4WindowsLoader-Native-Prepare.elf"
test -z "$(nm -u "$out/PS4WindowsLoader-Native-Prepare.elf")"
strings "$out/PS4WindowsLoader-Native-Prepare.elf" | grep -Fq \
 'MODE: NATIVE_PREPARATION_ONLY; Microsoft entry not called'
objcopy --set-section-flags '.bss*=alloc,load,contents' -O binary \
 "$out/PS4WindowsLoader-Native-Prepare.elf" "$out/PS4WindowsLoader-Native-Prepare.bin"
python3 - "$out" <<'PY'
import struct,sys
from pathlib import Path
p=Path(sys.argv[1]);e=(p/'PS4WindowsLoader-Native-Prepare.elf').read_bytes()
b=(p/'PS4WindowsLoader-Native-Prepare.bin').read_bytes()
if struct.unpack_from('<H',e,16)[0]!=3: raise SystemExit('Raw preparation requires PIE/DYN')
if struct.unpack_from('<Q',e,24)[0]!=0: raise SystemExit('Raw preparation entry must start at zero')
offset=struct.unpack_from('<Q',e,40)[0];size,count=struct.unpack_from('<HH',e,58)
for i in range(count):
    _,typ,flags,address,_,bytes_=struct.unpack_from('<IIQQQQ',e,offset+i*size)
    if typ in (4,9) and bytes_: raise SystemExit('Raw preparation requires no relocations')
    if flags&2:
        if address+bytes_>len(b): raise SystemExit('Allocated section omitted')
        if typ==8 and b[address:address+bytes_]!=bytes(bytes_): raise SystemExit('BSS omitted or not zero')
PY
objdump -d --disassemble=pwl_ps4_protected_read "$out/PS4WindowsLoader-Native-Prepare.elf" > "$out/READER-DISASSEMBLY.txt"
printf '%s\n' "$id" > "$out/COMMIT.txt"
printf '%s\n' 'NATIVE_PREPARATION_ONLY; no Microsoft call, CR3 change or device handoff' > "$out/MODE.txt"
cp docs/PS4_NATIVE_PREPARATION.md "$out/README.md"
(cd "$out" && sha256sum PS4WindowsLoader-Native-Prepare.bin PS4WindowsLoader-Native-Prepare.elf > SHA256SUMS.txt)
rm -f "$out"/*.o "$out/linker.x"
