#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=${1:?SDK directory required}
case "$sdk" in /*) ;; *) sdk="$(pwd)/$sdk" ;; esac
compiler=${CC:-cc}
sh tools/build_native_core.sh build/boot-source
"$compiler" -I"$sdk/libPS4/include" -Iloader/include -Ipayload \
    -std=c11 -Os -ffreestanding -fshort-wchar -fno-builtin \
    -fno-stack-protector -mno-red-zone -mgeneral-regs-only -fpie -fPIC \
    -Wall -Wextra -Werror -c payload/boot_source_io.c \
    -o build/boot-source/console-io.o
# Keep process I/O outside the syscall-free resident/native object. The future
# console entry links this adapter only in its preparation context.
cp docs/BOOT_SOURCE.md build/boot-source/BOOT_SOURCE.md
printf '%s\n' 'PREPARATION LIBRARY; NO CONSOLE ENTRY OR CPU TRANSITION' > build/boot-source/MODE.txt
