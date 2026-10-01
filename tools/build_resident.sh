#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
compiler=${CC:-cc}
out=build/resident
mkdir -p "$out"
for unit in resident_services resident_events firmware_memory firmware_files efi_tables; do
    "$compiler" -std=c11 -Os -Wall -Wextra -Wpedantic -Werror \
      -ffreestanding -fno-builtin -fno-stack-protector -fno-common \
      -fno-asynchronous-unwind-tables \
      -ffunction-sections -fdata-sections -mno-red-zone -mgeneral-regs-only \
      -fPIC -fvisibility=hidden -Iloader/include -c "loader/src/$unit.c" -o "$out/$unit.o"
done
"$compiler" -c loader/src/resident_binding.S -o "$out/binding.o"
# Public callbacks are roots, including those unused by other code.
ld --no-undefined --no-relax --gc-sections -T loader/resident.ld \
  -u pwl_resident_raise_tpl -u pwl_resident_restore_tpl \
  -u pwl_resident_allocate_pages -u pwl_resident_free_pages \
  -u pwl_resident_get_memory_map -u pwl_resident_exit_boot_services \
  -u pwl_resident_calculate_crc32 -u pwl_resident_copy_mem -u pwl_resident_set_mem \
  -u pwl_resident_allocate_pool -u pwl_resident_free_pool \
  -u pwl_resident_install_protocol -u pwl_resident_reinstall_protocol \
  -u pwl_resident_uninstall_protocol -u pwl_resident_handle_protocol \
  -u pwl_resident_locate_handle -u pwl_resident_locate_protocol \
  -u pwl_resident_open_volume -u pwl_resident_file_open -u pwl_resident_file_close \
  -u pwl_resident_file_delete -u pwl_resident_file_read -u pwl_resident_file_write \
  -u pwl_resident_file_get_position -u pwl_resident_file_set_position \
  -u pwl_resident_file_get_info -u pwl_resident_file_set_info -u pwl_resident_file_flush \
  -u pwl_resident_open_protocol -u pwl_resident_close_protocol -u pwl_resident_unsupported \
  -u pwl_resident_create_event -u pwl_resident_signal_event -u pwl_resident_close_event \
  -u pwl_resident_check_event -u pwl_resident_create_event_ex -u pwl_resident_wait_for_event \
  -u pwl_resident_locate_handle_buffer -u pwl_resident_protocols_per_handle \
  -u pwl_resident_open_protocol_information -u pwl_resident_install_configuration_table \
  "$out/resident_events.o" "$out/firmware_files.o" "$out/resident_services.o" "$out/firmware_memory.o" "$out/efi_tables.o" \
  "$out/binding.o" -o "$out/resident.elf"
objcopy -O binary "$out/resident.elf" "$out/resident.bin"
objdump -d "$out/resident.elf" > "$out/DISASSEMBLY.txt"
python3 tools/check_resident.py "$out"
(cd "$out" && sha256sum resident.bin resident.elf > SHA256SUMS.txt)
