#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
compiler=${CC:-cc}
sanitizers=${1:-address,undefined}
mkdir -p build
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer \
    tests/test_probe_log.c -o build/test-probe-log
./build/test-probe-log
sh tools/build_resident.sh
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include -Ibuild/resident \
    -DPWL_PS4_MEMORY_HOST_TEST \
    loader/src/main.c loader/src/boot_source.c loader/src/pe_loader.c loader/src/paging.c \
    loader/src/ps4_binding.c loader/src/ps4_memory.c loader/src/firmware_media.c \
    loader/src/native_workspace.c loader/src/entry_pipeline.c loader/src/table_snapshot.c loader/src/cpu_state.c loader/src/efi_entry.c loader/src/native_transition.c loader/src/alias_map.c loader/src/transition_map.c loader/src/graphics.c loader/src/resident_workspace.c loader/src/resident_selftest.c \
    loader/src/efi_tables.c loader/src/firmware_memory.c loader/src/firmware_files.c loader/src/handoff.c \
    tests/test_resident_services.c -o build/test-resident-services
./build/test-resident-services
for unit in variables timers; do
    "$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
        -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include -Ibuild/resident \
        loader/src/efi_tables.c "tests/test_resident_$unit.c" -o "build/test-resident-$unit"
    "./build/test-resident-$unit"
done
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include -Ibuild/resident \
    loader/src/efi_tables.c loader/src/handoff.c loader/src/firmware_memory.c \
    loader/src/image_permissions.c tests/test_resident_images.c tests/resident_image_abi.S -o build/test-resident-images
./build/test-resident-images
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include -Ibuild/resident \
    loader/src/efi_tables.c loader/src/graphics.c loader/src/handoff.c loader/src/firmware_memory.c \
    tests/test_resident_graphics.c -o build/test-resident-graphics
./build/test-resident-graphics
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    loader/src/efi_tables.c tests/test_efi_tables.c -o build/test-efi-tables
./build/test-efi-tables
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    loader/src/ps4_binding.c loader/src/ps4_memory.c tests/test_ps4_binding.c \
    -o build/test-ps4-binding
./build/test-ps4-binding
# The fixture escape hatch must fail to compile into any freestanding object.
if "$compiler" -std=c11 -ffreestanding -DPWL_PS4_MEMORY_HOST_TEST \
    -Iloader/include -c loader/src/ps4_binding.c -o build/forbidden-host-binding.o \
    2>build/host-binding-rejection.txt; then
    echo 'Host memory fixture unexpectedly compiled freestanding' >&2
    exit 1
fi
grep -q 'Host memory fixtures must never' build/host-binding-rejection.txt
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    loader/src/handoff.c loader/src/firmware_memory.c loader/src/firmware_files.c loader/src/firmware_media.c \
    tests/test_firmware.c -o build/test-firmware
./build/test-firmware
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    loader/src/handoff.c loader/src/firmware_memory.c loader/src/firmware_files.c tests/test_pool.c \
    -o build/test-pool
./build/test-pool
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    loader/src/firmware_files.c tests/test_files.c -o build/test-files
./build/test-files
python3 tests/test_pack_files.py
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    loader/src/main.c loader/src/pe_loader.c tests/test_pe_loader.c \
    -o build/test-pe-loader
./build/test-pe-loader
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    -DPWL_PS4_MEMORY_HOST_TEST -Ibuild/resident \
    loader/src/main.c loader/src/boot_source.c loader/src/pe_loader.c \
    loader/src/handoff.c loader/src/paging.c loader/src/ps4_binding.c loader/src/ps4_memory.c \
    loader/src/firmware_memory.c loader/src/firmware_files.c loader/src/firmware_media.c loader/src/efi_tables.c \
    loader/src/graphics.c loader/src/resident_workspace.c loader/src/native_workspace.c loader/src/entry_pipeline.c loader/src/table_snapshot.c loader/src/cpu_state.c loader/src/efi_entry.c loader/src/native_transition.c loader/src/alias_map.c loader/src/transition_map.c \
    tests/test_native_workspace.c -o build/test-native-workspace
./build/test-native-workspace

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer -Iloader/include loader/src/cpu_state.c \
 loader/src/root_clone.c tests/test_root_clone.c -o build/test-root-clone
./build/test-root-clone
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer -Iloader/include -Ipayload loader/src/cpu_state.c \
 loader/src/root_clone.c tests/test_root_source.c -o build/test-root-source
./build/test-root-source
"$compiler" -c -m64 loader/src/root_clone_call.S -o build/root-clone-call-check.o

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer tests/test_raw_journal.c -o build/test-raw-journal
./build/test-raw-journal

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer tests/test_log_readback.c -o build/test-log-readback
./build/test-log-readback

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer tests/test_usb_identity.c -o build/test-usb-identity
./build/test-usb-identity

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer tests/test_verified_usb.c -o build/test-verified-usb
./build/test-verified-usb
"$compiler" -c -m64 loader/src/root_efi_call.S -o build/root-efi-call-check.o

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer -Iloader/include loader/src/transition_map.c \
 tests/test_transition_map.c -o build/test-transition-map
./build/test-transition-map

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer -Iloader/include loader/src/exception_context.c \
 tests/test_exception_context.c -o build/test-exception-context
./build/test-exception-context

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer -Iloader/include loader/src/alias_map.c \
 loader/src/transition_map.c tests/test_alias_map.c -o build/test-alias-map
./build/test-alias-map

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer -Iloader/include loader/src/efi_entry_call.S \
 tests/test_efi_entry_call.c -o build/test-efi-entry-call
./build/test-efi-entry-call

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer -Iloader/include loader/src/table_snapshot.c \
 loader/src/transition_map.c tests/test_table_snapshot.c -o build/test-table-snapshot
./build/test-table-snapshot

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer -DPWL_TABLE_READER_HOST_TEST -Iloader/include -Ipayload \
 loader/src/cpu_state.c loader/src/root_clone.c payload/table_snapshot_io.c \
 tests/test_table_reader.c -o build/test-table-reader
./build/test-table-reader
"$compiler" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -ffreestanding \
 -fno-builtin -fno-stack-protector -mno-red-zone -mgeneral-regs-only \
 -Iloader/include -Ipayload -c payload/table_snapshot_io.c -o build/console-table-reader.o
if "$compiler" -std=c11 -ffreestanding -DPWL_TABLE_READER_HOST_TEST \
 -Iloader/include -Ipayload -c payload/table_snapshot_io.c -o build/forbidden-table-reader.o \
 2>build/table-reader-rejection.txt; then
 echo 'Host reader fixture compiled freestanding' >&2; exit 1
fi
