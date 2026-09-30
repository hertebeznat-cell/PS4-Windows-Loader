#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=${1:?SDK directory required}
id=${2:?commit id required}
case "$sdk" in /*) ;; *) sdk="$(pwd)/$sdk" ;; esac
sh tools/build_kernel_dump.sh "$sdk" "$id"
mode=${3:-workspace}
defs=""
probe_defs="-DPWL_WORKSPACE_PROBE"
probe_source=payload/memory_probe.c
name=PS4WindowsLoader-Workspace-Probe
document=docs/WORKSPACE_PROBE.md
case "$mode" in
 workspace) out=build/workspace-probe ;;
 resident)
  sh tools/build_resident.sh
  out=build/resident-probe
  defs="-DPWL_RESIDENT_PROBE -Ibuild/resident"
  name=PS4WindowsLoader-Resident-Probe
  document=docs/RESIDENT_PROBE.md ;;
 root-efi)
  sh tools/build_resident.sh
  out=build/root-efi
  defs="-DPWL_ROOT_CLONE_PROBE -DPWL_ROOT_EFI_PROBE -fshort-wchar -Ibuild/resident -Iloader/include"
  probe_defs=""
  name=PS4WindowsLoader-Root-EFI
  document=docs/ROOT_EFI.md ;;
 root-clone)
  out=build/root-clone
  defs="-DPWL_ROOT_CLONE_PROBE"
  probe_defs=""
  name=PS4WindowsLoader-Root-Clone
  document=docs/ROOT_CLONE.md ;;
 usb-existing)
  out=build/usb-existing
  probe_defs=""
  probe_source=payload/usb_existing_check.c
  name=PS4WindowsLoader-USB-Existing-File
  document=docs/USB_EXISTING.md ;;
 usb-log)
  out=build/usb-identity-check
  probe_defs=""
  probe_source=payload/usb_log_check.c
  name=PS4WindowsLoader-USB-Identity-Check
  document=docs/USB_LOG_CHECK.md ;;
 *) echo 'Unknown preparation mode' >&2; exit 1 ;;
esac
mkdir -p "$out"
gcc -I"$sdk/libPS4/include" -Ipayload -Os -std=c11 -ffreestanding \
 -fno-builtin -fno-stack-protector -nostartfiles -nostdlib -Wall -Wextra -Werror \
 -ffunction-sections -fdata-sections -masm=intel -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
 $probe_defs $defs "-DPS4WL_BUILD_ID=\"$id\"" -c "$probe_source" -o "$out/probe.o"
objects=""
for unit in main pe_loader handoff paging ps4_memory firmware_memory firmware_files firmware_media efi_tables resident_workspace native_workspace freestanding; do
 gcc -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns \
  -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -mno-red-zone -mgeneral-regs-only \
  -fpie -fPIC -Wall -Wextra -Werror -Iloader/include -c "loader/src/$unit.c" -o "$out/$unit.o"
 objects="$objects $out/$unit.o"
done
gcc -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns \
 -ffunction-sections -fdata-sections -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
 -Wall -Wextra -Werror -DPWL_WORKSPACE_DIAGNOSTIC $defs -Iloader/include -Ipayload \
 -c payload/workspace_probe_core.c -o "$out/workspace_probe_core.o"
if [ "$mode" = root-clone ] || [ "$mode" = root-efi ]; then
 for unit in cpu_state root_clone; do
  gcc -std=c11 -Os -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector \
   -fpie -fPIC -mno-red-zone -mgeneral-regs-only -Iloader/include -c "loader/src/$unit.c" -o "$out/$unit.o"
  objects="$objects $out/$unit.o"
 done
 gcc -std=c11 -Os -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector \
  -fpie -fPIC -mno-red-zone -mgeneral-regs-only -Iloader/include -Ipayload $defs \
  -c payload/root_clone_probe_core.c -o "$out/root_clone_probe_core.o"
 if [ "$mode" = root-efi ]; then
  for unit in resident_selftest; do
   gcc -std=c11 -Os -Wall -Wextra -Werror -ffreestanding -fno-builtin -fno-stack-protector \
    -fno-tree-loop-distribute-patterns -fpie -fPIC -mno-red-zone -mgeneral-regs-only -Iloader/include \
    -c "loader/src/$unit.c" -o "$out/$unit.o"
   objects="$objects $out/$unit.o"
  done
  gcc -c -m64 loader/src/root_efi_call.S -o "$out/root_efi_call.o"
  objects="$objects $out/root_efi_call.o"
 fi
 gcc -c -m64 loader/src/root_clone_call.S -o "$out/root_clone_call.o"
 gcc -c -m64 payload/raw_journal.S -o "$out/raw_journal.o"
 objects="$objects $out/root_clone_probe_core.o $out/root_clone_call.o $out/raw_journal.o"
fi
ld -r $objects "$out/workspace_probe_core.o" -o "$out/core.o"
# SDK has pointer globals named memcpy/memset/memmove; keep native functions distinct.
objcopy --redefine-sym memcpy=pwl_native_memcpy --redefine-sym memset=pwl_native_memset \
 --redefine-sym memmove=pwl_native_memmove "$out/core.o"
startup="$sdk/libPS4/crt0.s"
if [ "$mode" = root-clone ] || [ "$mode" = root-efi ] || [ "$mode" = usb-log ] || [ "$mode" = usb-existing ]; then
 gcc -c -m64 payload/probe_start.S -o "$out/probe_start.o"
 startup="$out/probe_start.o"
fi
gcc "$startup" "$out/probe.o" "$out/core.o" -nostartfiles -nostdlib -pie \
 -Wl,-T,payload/probe_linker.ld -Wl,--build-id=none -Wl,--gc-sections -Wl,-z,noexecstack \
 -L"$sdk/libPS4" -lPS4 -o "$out/$name.elf"
test -z "$(nm -u "$out/$name.elf")"
objcopy --set-section-flags .bss=alloc,load,contents -O binary \
 "$out/$name.elf" "$out/$name.bin"
python3 - "$out" "$name" <<'PY'
import struct,sys
from pathlib import Path
p=Path(sys.argv[1]);name=sys.argv[2];e=(p/(name+'.elf')).read_bytes();b=(p/(name+'.bin')).read_bytes()
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
cp "$document" "$out/README.md"
if [ "$mode" = resident ]; then
 cp build/resident/MANIFEST.json "$out/RESIDENT-MANIFEST.json"
 printf '%s\n' 'PREPARATION_ONLY; resident code copied, never executed; no CPU transition' > "$out/MODE.txt"
fi
if [ "$mode" = root-clone ]; then
 printf '%s\n' 'IDENTICAL_ROOT_CLONE; bounded CR3/stack round trip; no EFI root activation; Windows not called' > "$out/MODE.txt"
fi
if [ "$mode" = root-efi ]; then
 cp build/resident/MANIFEST.json "$out/RESIDENT-MANIFEST.json"
 printf '%s\n' 'IDENTICAL_ROOT_EFI; nine resident callbacks with synthetic descriptors; Windows not called' > "$out/MODE.txt"
fi
(cd "$out" && sha256sum *.bin *.elf > SHA256SUMS.txt)
