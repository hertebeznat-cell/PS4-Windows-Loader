#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
compiler=${CC:-cc}
sh tools/build_resident.sh
# Deliberate stack switching is tested separately from ASan, which needs its
# own fiber API. The normal service and workspace tests keep ASan/UBSan.
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -Iloader/include \
 tests/test_stack_call.c loader/src/stack_call.S -o build/test-stack-call
./build/test-stack-call
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -Iloader/include -Ibuild/resident \
 -DPWL_PS4_MEMORY_HOST_TEST -DPWL_TEST_STACK \
 loader/src/main.c loader/src/pe_loader.c loader/src/paging.c \
 loader/src/ps4_binding.c loader/src/ps4_memory.c loader/src/firmware_media.c \
 loader/src/native_workspace.c loader/src/graphics.c loader/src/resident_workspace.c loader/src/resident_selftest.c \
 loader/src/efi_tables.c loader/src/firmware_memory.c loader/src/firmware_files.c loader/src/handoff.c \
 loader/src/stack_call.S tests/test_resident_services.c -o build/test-resident-stack
./build/test-resident-stack
