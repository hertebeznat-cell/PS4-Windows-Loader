#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=${1:?usage: build_kernel_dump.sh sdk-directory build-id}
build_id=${2:?build-id required}
case "$sdk" in /*) ;; *) sdk="$(pwd)/$sdk" ;; esac
python3 - "$sdk" <<'PY'
import sys
from pathlib import Path
p=Path(sys.argv[1])/'libPS4/source/payload_utils.c'
s=p.read_text()
a='copyout(&kernel_base, (uint64_t *)uaddr, 8);\n\n  return 0;'
b='return copyout(&kernel_base, (uint64_t *)uaddr, 8);'
if s.count(a)==1: p.write_text(s.replace(a,b))
elif s.count(b)!=1: raise SystemExit('SDK base-copy patch does not match')
PY
make -C "$sdk/libPS4" -j2
# Kernel callbacks must not use the red zone or compiler-generated SIMD.
gcc -I"$sdk/libPS4/include" -Os -std=c11 -ffunction-sections -fdata-sections \
 -fno-builtin -fno-stack-protector -nostartfiles -nostdlib -masm=intel \
 -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
 -c "$sdk/libPS4/source/payload_utils.c" -o "$sdk/libPS4/build/payload_utils.o"
ar rcs "$sdk/libPS4/libPS4.a" "$sdk/libPS4/build/payload_utils.o"
out=build/kernel-dump
mkdir -p "$out"
gcc -I"$sdk/libPS4/include" -Ipayload -Os -std=c11 -ffreestanding \
 -fno-builtin -fno-stack-protector -nostartfiles -nostdlib -Wall -Wextra -Werror \
 -masm=intel -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
 "-DPS4WL_BUILD_ID=\"$build_id\"" -c payload/kernel_dump.c -o "$out/dumper.o"
gcc "$sdk/libPS4/crt0.s" "$out/dumper.o" -nostartfiles -nostdlib -fpie -fPIC \
 -Wl,-T,"$sdk/libPS4/linker.x" -Wl,--build-id=none -Wl,--gc-sections -Wl,-z,noexecstack \
 -L"$sdk/libPS4" -lPS4 -o "$out/PS4WindowsLoader-Kernel-Dump.elf"
test -z "$(nm -u "$out/PS4WindowsLoader-Kernel-Dump.elf")"
objcopy --set-section-flags .bss=alloc,load,contents -O binary \
 "$out/PS4WindowsLoader-Kernel-Dump.elf" "$out/PS4WindowsLoader-Kernel-Dump.bin"
rm -f "$out/dumper.o"
printf '%s\n' "$build_id" > "$out/COMMIT.txt"
printf '%s\n' b7326416c23ce14639e9180a24093bdc5eedb579 > "$out/SDK_COMMIT.txt"
cp docs/KERNEL_DUMP.md "$out/README.md"
(cd "$out" && sha256sum PS4WindowsLoader-Kernel-Dump.bin PS4WindowsLoader-Kernel-Dump.elf > SHA256SUMS.txt)
