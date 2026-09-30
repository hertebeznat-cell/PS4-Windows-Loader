#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=${1:?SDK directory required}
id=${2:?commit id required}
case "$sdk" in /*) ;; *) sdk="$(pwd)/$sdk" ;; esac
make -C "$sdk/libPS4" -j2
sh tools/build_resident.sh
mode=${3:-calls}
defs=""
name=PS4WindowsLoader-Resident-Calls
document=docs/RESIDENT_CALLS.md
case "$mode" in
 calls) out=build/resident-calls ;;
 stack)
  out=build/resident-stack;defs="-DPWL_STACK_PROBE"
  name=PS4WindowsLoader-Resident-Stack;document=docs/RESIDENT_STACK.md ;;
 *) echo 'Unknown resident call mode' >&2;exit 1 ;;
esac
mkdir -p "$out"
gcc -I"$sdk/libPS4/include" -Iloader/include -Ipayload -Ibuild/resident \
 -Os -std=c11 -fshort-wchar -ffreestanding -fno-builtin -fno-stack-protector -nostartfiles -nostdlib \
 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -masm=intel \
 -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
 $defs "-DPS4WL_BUILD_ID=\"$id\"" -c payload/resident_call_probe.c -o "$out/probe.o"
objects=""
for unit in main pe_loader handoff paging ps4_binding ps4_memory firmware_memory firmware_media efi_tables resident_workspace native_workspace resident_selftest freestanding; do
 gcc -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector \
  -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables \
  -mno-red-zone -mgeneral-regs-only -fpie -fPIC -Wall -Wextra -Werror \
  -Iloader/include -c "loader/src/$unit.c" -o "$out/$unit.o"
 objects="$objects $out/$unit.o"
done
gcc -c loader/src/stack_call.S -o "$out/stack_call.o"
ld -r $objects "$out/stack_call.o" -o "$out/core.o"
objcopy --redefine-sym memcpy=pwl_native_memcpy --redefine-sym memset=pwl_native_memset \
 --redefine-sym memmove=pwl_native_memmove "$out/core.o"
gcc "$sdk/libPS4/crt0.s" "$out/probe.o" "$out/core.o" -nostartfiles -nostdlib -pie \
 -Wl,-T,payload/probe_linker.ld -Wl,--build-id=none -Wl,--gc-sections -Wl,-z,noexecstack \
 -L"$sdk/libPS4" -lPS4 -o "$out/$name.elf"
test -z "$(nm -u "$out/$name.elf")"
if nm "$out/$name.elf" | grep -E ' (kexec|pwl_workspace_experiment|pwl_ps4_arena_acquire)$'; then
 echo 'Unexpected privileged experiment retained in process test' >&2; exit 1
fi
objcopy --set-section-flags .bss=alloc,load,contents -O binary \
 "$out/$name.elf" "$out/$name.bin"
python3 - "$out" "$name" <<'PY'
import struct,sys
from pathlib import Path
p=Path(sys.argv[1]);name=sys.argv[2];e=(p/(name+'.elf')).read_bytes();b=(p/(name+'.bin')).read_bytes()
if struct.unpack_from('<H',e,16)[0]!=3:raise SystemExit('Raw process test requires PIE/DYN linking')
o=struct.unpack_from('<Q',e,40)[0];sz,n=struct.unpack_from('<HH',e,58)
for i in range(n):
 _,typ,flags,addr,off,size=struct.unpack_from('<IIQQQQ',e,o+i*sz)
 if typ in (4,9) and size:raise SystemExit('Raw test must not require relocations')
 if flags&2:
  if addr+size>len(b):raise SystemExit('Allocated section omitted')
  if typ==8 and b[addr:addr+size]!=bytes(size):raise SystemExit('BSS not zero-filled')
PY
rm -f "$out"/*.o
printf '%s\n' "$id" > "$out/COMMIT.txt"
printf '%s\n' "PROCESS_TEST mode=$mode; synthetic memory descriptors; no CPU transition" > "$out/MODE.txt"
cp "$document" "$out/README.md"
cp build/resident/MANIFEST.json "$out/RESIDENT-MANIFEST.json"
(cd "$out" && sha256sum *.bin *.elf > SHA256SUMS.txt)
