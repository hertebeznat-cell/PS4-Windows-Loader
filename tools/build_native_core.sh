#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
compiler=${CC:-cc}
output=${1:-build/native-core}
mkdir -p "$output"
for unit in main acpi acpi_configuration native_acpi native_call entry_pipeline table_snapshot efi_entry boot_source native_transition alias_map transition_map exception_context pe_loader handoff paging root_clone cpu_state ps4_binding ps4_profile ps4_reader ps4_memory firmware_memory firmware_files firmware_media efi_tables graphics resident_workspace resident_selftest native_workspace freestanding; do
    "$compiler" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
        -ffreestanding -fno-builtin -fno-stack-protector -fno-common \
        -fno-asynchronous-unwind-tables -mno-red-zone -mgeneral-regs-only -fPIE \
        -Iloader/include -c "loader/src/$unit.c" -o "$output/$unit.o"
done
"$compiler" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
    -ffreestanding -fno-builtin -fno-stack-protector -fno-common \
    -fno-asynchronous-unwind-tables -mno-red-zone -mgeneral-regs-only -fPIE \
    -Iloader/include -Ipayload -c payload/table_snapshot_io.c -o "$output/table_snapshot_io.o"
"$compiler" -c -m64 loader/src/address_call.S -o "$output/address_call.o"
"$compiler" -c -m64 loader/src/efi_entry_call.S -o "$output/efi_entry_call.o"
"$compiler" -c -m64 loader/src/native_call.S -o "$output/native_call_asm.o"
python3 tools/check_native_call.py "$output/native_call_asm.o"
ld -r "$output/acpi_configuration.o" "$output/acpi.o" "$output/native_acpi.o" "$output/native_call.o" "$output/native_call_asm.o" "$output/root_clone.o" "$output/table_snapshot_io.o" "$output/address_call.o" "$output/efi_entry_call.o" "$output/entry_pipeline.o" "$output/table_snapshot.o" "$output/efi_entry.o" "$output/main.o" "$output/boot_source.o" "$output/native_transition.o" "$output/alias_map.o" "$output/transition_map.o" "$output/exception_context.o" "$output/pe_loader.o" "$output/handoff.o" "$output/paging.o" "$output/cpu_state.o" \
    "$output/ps4_reader.o" "$output/ps4_profile.o" "$output/ps4_binding.o" "$output/ps4_memory.o" "$output/firmware_memory.o" "$output/firmware_files.o" "$output/firmware_media.o" \
    "$output/efi_tables.o" "$output/graphics.o" "$output/resident_workspace.o" "$output/resident_selftest.o" "$output/native_workspace.o" "$output/freestanding.o" -o "$output/ps4wl-native-core.o"
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
printf '%s\n' 'RELOCATABLE DEVELOPMENT OBJECT; CONTROLLED RETURNING TRANSITION INCLUDED; NOT CONNECTED TO CONSOLE ENTRY' > "$output/MODE.txt"
git rev-parse HEAD > "$output/COMMIT.txt"
if [ -n "$(git status --porcelain --untracked-files=normal)" ]; then
    printf '%s\n' 'DIRTY: object includes local changes beyond COMMIT.txt' > "$output/SOURCE-STATE.txt"
else
    printf '%s\n' 'CLEAN' > "$output/SOURCE-STATE.txt"
fi
cp docs/NATIVE_BACKEND.md "$output/README.md"
(cd "$output" && sha256sum ps4wl-native-core.o > SHA256SUMS.txt)
