#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
compiler=${CC:-cc}
output=${1:-build/native-core}
mkdir -p "$output"
for unit in main pe_loader handoff paging cpu_state ps4_binding ps4_memory firmware_memory firmware_media efi_tables resident_workspace resident_selftest native_workspace freestanding; do
    "$compiler" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
        -ffreestanding -fno-builtin -fno-stack-protector -fno-common \
        -fno-asynchronous-unwind-tables -mno-red-zone -mgeneral-regs-only -fPIE \
        -Iloader/include -c "loader/src/$unit.c" -o "$output/$unit.o"
done
"$compiler" -c -m64 loader/src/address_call.S -o "$output/address_call.o"
ld -r "$output/address_call.o" "$output/main.o" "$output/pe_loader.o" "$output/handoff.o" "$output/paging.o" "$output/cpu_state.o" \
    "$output/ps4_binding.o" "$output/ps4_memory.o" "$output/firmware_memory.o" "$output/firmware_media.o" \
    "$output/efi_tables.o" "$output/resident_workspace.o" "$output/resident_selftest.o" "$output/native_workspace.o" "$output/freestanding.o" -o "$output/ps4wl-native-core.o"
nm -u "$output/ps4wl-native-core.o" > "$output/UNDEFINED-SYMBOLS.txt"
if [ -s "$output/UNDEFINED-SYMBOLS.txt" ]; then
    cat "$output/UNDEFINED-SYMBOLS.txt" >&2
    exit 1
fi
objdump -d "$output/ps4wl-native-core.o" > "$output/DISASSEMBLY.txt"
if grep -E '[[:space:]](syscall|sysenter|int[[:space:]]+\$0x80)([[:space:]]|$)' "$output/DISASSEMBLY.txt"; then
    echo 'Unexpected process entry instruction in native core' >&2
    exit 1
fi
printf '%s\n' 'RELOCATABLE DEVELOPMENT OBJECT; NOT A CONSOLE PAYLOAD; NO CPU TRANSITION' > "$output/MODE.txt"
git rev-parse HEAD > "$output/COMMIT.txt"
if [ -n "$(git status --porcelain --untracked-files=normal)" ]; then
    printf '%s\n' 'DIRTY: object includes local changes beyond COMMIT.txt' > "$output/SOURCE-STATE.txt"
else
    printf '%s\n' 'CLEAN' > "$output/SOURCE-STATE.txt"
fi
cp docs/NATIVE_BACKEND.md "$output/README.md"
(cd "$output" && sha256sum ps4wl-native-core.o > SHA256SUMS.txt)
