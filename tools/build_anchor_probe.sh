#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
runtime=${1:?usage: build_anchor_probe.sh runtime-directory [build-id]}
case "$runtime" in /*) ;; *) runtime="$(pwd)/$runtime" ;; esac
build_id=${2:-local}
output=build/anchor-probe
mkdir -p "$output"
gcc -Wall -Wextra -Werror -isystem "$runtime/freebsd-headers" \
    -nostdinc -nostdlib -fno-stack-protector -fno-builtin \
    -mno-red-zone -mgeneral-regs-only -static -fPIE -ffreestanding \
    "-DPS4WL_BUILD_ID=\"$build_id\"" \
    "$runtime/lib/lib.a" payload/anchor_probe.c payload/anchor_capture.S \
    -Wl,-gc-sections -o "$output/PS4WindowsLoader-Anchor-Probe.elf"
objcopy "$output/PS4WindowsLoader-Anchor-Probe.elf" \
    --only-section .text --only-section .data --only-section .bss --only-section .rodata \
    --set-section-flags .bss=alloc,load,contents -O binary \
    "$output/PS4WindowsLoader-Anchor-Probe.bin"
objdump -d --disassemble=pwl_anchor_capture "$output/PS4WindowsLoader-Anchor-Probe.elf" \
    > "$output/CALLBACK-DISASSEMBLY.txt"
PWL_CAPTURE_PREFIX=pwl_anchor python3 tools/check_context_link.py \
    "$output/PS4WindowsLoader-Anchor-Probe.elf" "$output/PS4WindowsLoader-Anchor-Probe.bin"
printf '%s\n' "$build_id" > "$output/COMMIT.txt"
git -C "$runtime" rev-parse HEAD > "$output/PS4_RUNTIME_COMMIT.txt"
cp docs/ANCHOR_PROBE.md "$output/README.md"
(cd "$output" && sha256sum PS4WindowsLoader-Anchor-Probe.bin PS4WindowsLoader-Anchor-Probe.elf > SHA256SUMS.txt && sha256sum -c SHA256SUMS.txt)
