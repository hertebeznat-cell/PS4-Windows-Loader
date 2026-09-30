#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
compiler=${CC:-cc}
out=build/resident
mkdir -p "$out"
for unit in resident_services firmware_memory efi_tables; do
    "$compiler" -std=c11 -Os -Wall -Wextra -Wpedantic -Werror \
      -ffreestanding -fno-builtin -fno-stack-protector -fno-common \
      -fno-asynchronous-unwind-tables \
      -ffunction-sections -fdata-sections -mno-red-zone -mgeneral-regs-only \
      -fPIC -fvisibility=hidden -Iloader/include -c "loader/src/$unit.c" -o "$out/$unit.o"
done
"$compiler" -c loader/src/resident_binding.S -o "$out/binding.o"
# All nine public callbacks are roots, including those unused by other code.
ld --no-undefined --no-relax --gc-sections -T loader/resident.ld \
  -u pwl_resident_raise_tpl -u pwl_resident_restore_tpl \
  -u pwl_resident_allocate_pages -u pwl_resident_free_pages \
  -u pwl_resident_get_memory_map -u pwl_resident_exit_boot_services \
  -u pwl_resident_calculate_crc32 -u pwl_resident_copy_mem -u pwl_resident_set_mem \
  "$out/resident_services.o" "$out/firmware_memory.o" "$out/efi_tables.o" \
  "$out/binding.o" -o "$out/resident.elf"
objcopy -O binary "$out/resident.elf" "$out/resident.bin"
objdump -d "$out/resident.elf" > "$out/DISASSEMBLY.txt"
python3 tools/check_resident.py "$out"
(cd "$out" && sha256sum resident.bin resident.elf > SHA256SUMS.txt)
