#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=${1:?SDK directory required}
id=${2:?build id required}
case "$sdk" in /*) ;; *) sdk="$(pwd)/$sdk" ;; esac
sh tools/build_kernel_dump.sh "$sdk" "$id"
out=build/exception-context
mkdir -p "$out"
gcc -I"$sdk/libPS4/include" -Iloader/include -Ipayload -Os -std=c11 \
 -ffreestanding -fshort-wchar -fno-builtin -fno-stack-protector \
 -fno-tree-loop-distribute-patterns -nostartfiles -nostdlib -Wall -Wextra -Werror \
 -ffunction-sections -fdata-sections -masm=intel -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
 "-DPS4WL_BUILD_ID=\"$id\"" -c payload/exception_context_probe.c -o "$out/probe.o"
gcc -std=c11 -Os -ffreestanding -fno-builtin -fno-stack-protector \
 -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections \
 -mno-red-zone -mgeneral-regs-only -fpie -fPIC -Wall -Wextra -Werror \
 -Iloader/include -c loader/src/exception_context.c -o "$out/decode.o"
gcc -c -m64 payload/probe_start.S -o "$out/start.o"
gcc "$out/start.o" "$out/probe.o" "$out/decode.o" -nostartfiles -nostdlib -pie \
 -Wl,-T,payload/probe_linker.ld -Wl,--build-id=none -Wl,--gc-sections -Wl,-z,noexecstack \
 -L"$sdk/libPS4" -lPS4 -o "$out/PS4WindowsLoader-Exception-Context.elf"
test -z "$(nm -u "$out/PS4WindowsLoader-Exception-Context.elf")"
objdump -d "$out/probe.o" > "$out/CAPTURE-DISASSEMBLY.txt"
if grep -E '[[:space:]](lgdt|lidt|ltr|wrmsr|cli|sti)([[:space:]]|$)|mov.*,[[:space:]]*%cr[034]' "$out/CAPTURE-DISASSEMBLY.txt"; then
 echo 'Unexpected CPU control write in context observer' >&2;exit 1
fi
objcopy --set-section-flags .bss=alloc,load,contents -O binary \
 "$out/PS4WindowsLoader-Exception-Context.elf" "$out/PS4WindowsLoader-Exception-Context.bin"
python3 - "$out" <<'PY'
import struct,sys
from pathlib import Path
p=Path(sys.argv[1]);e=(p/'PS4WindowsLoader-Exception-Context.elf').read_bytes();b=(p/'PS4WindowsLoader-Exception-Context.bin').read_bytes()
assert struct.unpack_from('<H',e,16)[0]==3
start=struct.unpack_from('<Q',e,40)[0];size,count=struct.unpack_from('<HH',e,58)
for i in range(count):
 _,typ,flags,addr,off,length=struct.unpack_from('<IIQQQQ',e,start+i*size)
 if typ in (4,9) and length:raise SystemExit('Image requires relocations')
 if flags&2:
  if addr+length>len(b):raise SystemExit('Allocated section omitted')
  if typ==8 and b[addr:addr+length]!=bytes(length):raise SystemExit('BSS not zero-filled')
PY
printf '%s\n' "$id" > "$out/COMMIT.txt"
cp docs/EXCEPTION_CONTEXT.md "$out/README.md"
(cd "$out" && sha256sum *.bin *.elf > SHA256SUMS.txt)
