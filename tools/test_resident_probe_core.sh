#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sh tools/build_resident.sh
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -DPWL_WORKSPACE_DIAGNOSTIC -DPWL_WORKSPACE_HOST_TEST -DPWL_RESIDENT_PROBE -Iloader/include -Ipayload -Ibuild/resident \
 payload/workspace_probe_core.c tests/test_workspace_probe_core.c \
 loader/src/main.c loader/src/pe_loader.c loader/src/handoff.c loader/src/paging.c \
 loader/src/ps4_memory.c loader/src/firmware_memory.c loader/src/firmware_files.c loader/src/firmware_media.c \
 loader/src/efi_tables.c loader/src/acpi.c loader/src/acpi_configuration.c loader/src/transition_map.c loader/src/graphics.c loader/src/resident_workspace.c loader/src/native_workspace.c \
 -o build/test-resident-probe-core
./build/test-resident-probe-core
