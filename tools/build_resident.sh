#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
compiler=${CC:-cc}
out=build/resident
mkdir -p "$out"
for unit in resident_services resident_events resident_clock resident_variables resident_images resident_image_map resident_graphics image_permissions main pe_loader firmware_memory firmware_files efi_tables freestanding; do
    "$compiler" -std=c11 -Os -Wall -Wextra -Wpedantic -Werror \
      -ffreestanding -fno-builtin -fno-stack-protector -fno-common \
      -fno-asynchronous-unwind-tables \
      -ffunction-sections -fdata-sections -mno-red-zone -mgeneral-regs-only \
      -fPIC -fvisibility=hidden -Iloader/include -c "loader/src/$unit.c" -o "$out/$unit.o"
done
"$compiler" -c loader/src/resident_binding.S -o "$out/binding.o"
"$compiler" -c loader/src/resident_image_jump.S -o "$out/image_jump.o"
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
  -u pwl_resident_get_variable -u pwl_resident_get_next_variable_name \
  -u pwl_resident_set_variable -u pwl_resident_query_variable_info \
  -u pwl_resident_set_timer -u pwl_resident_stall -u pwl_resident_set_watchdog_timer \
  -u pwl_resident_load_image -u pwl_resident_start_image -u pwl_resident_exit_image \
  -u pwl_resident_unload_image -u pwl_resident_image_map -u pwl_resident_boot_entry \
  -u pwl_resident_gop_query -u pwl_resident_gop_set -u pwl_resident_gop_blt \
  -u pwl_resident_text_reset -u pwl_resident_text_output -u pwl_resident_text_test \
  -u pwl_resident_text_query -u pwl_resident_text_set -u pwl_resident_text_attribute \
  -u pwl_resident_text_clear -u pwl_resident_text_position -u pwl_resident_text_cursor \
  "$out/resident_graphics.o" \
  "$out/resident_images.o" "$out/resident_image_map.o" "$out/image_permissions.o" "$out/image_jump.o" "$out/main.o" "$out/pe_loader.o" \
  "$out/resident_clock.o" "$out/resident_variables.o" "$out/resident_events.o" "$out/firmware_files.o" "$out/resident_services.o" "$out/firmware_memory.o" "$out/efi_tables.o" \
  "$out/freestanding.o" "$out/binding.o" -o "$out/resident.elf"
objcopy -O binary "$out/resident.elf" "$out/resident.bin"
objdump -d "$out/resident.elf" > "$out/DISASSEMBLY.txt"
python3 tools/check_resident.py "$out"
(cd "$out" && sha256sum resident.bin resident.elf > SHA256SUMS.txt)
