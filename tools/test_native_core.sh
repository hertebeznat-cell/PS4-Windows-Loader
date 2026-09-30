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
    loader/src/main.c loader/src/pe_loader.c loader/src/paging.c \
    loader/src/ps4_binding.c loader/src/ps4_memory.c loader/src/firmware_media.c \
    loader/src/native_workspace.c loader/src/resident_workspace.c loader/src/resident_selftest.c \
    loader/src/efi_tables.c loader/src/firmware_memory.c loader/src/handoff.c \
    tests/test_resident_services.c -o build/test-resident-services
./build/test-resident-services
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
    loader/src/handoff.c loader/src/firmware_memory.c loader/src/firmware_media.c \
    tests/test_firmware.c -o build/test-firmware
./build/test-firmware
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    loader/src/main.c loader/src/pe_loader.c tests/test_pe_loader.c \
    -o build/test-pe-loader
./build/test-pe-loader
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    -DPWL_PS4_MEMORY_HOST_TEST -Ibuild/resident \
    loader/src/main.c loader/src/pe_loader.c \
    loader/src/handoff.c loader/src/paging.c loader/src/ps4_binding.c loader/src/ps4_memory.c \
    loader/src/firmware_memory.c loader/src/firmware_media.c loader/src/efi_tables.c \
    loader/src/resident_workspace.c loader/src/native_workspace.c \
    tests/test_native_workspace.c -o build/test-native-workspace
./build/test-native-workspace

"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror -fsanitize="$sanitizers" \
 -fno-omit-frame-pointer -Iloader/include loader/src/cpu_state.c \
 loader/src/root_clone.c tests/test_root_clone.c -o build/test-root-clone
./build/test-root-clone
"$compiler" -c -m64 loader/src/root_clone_call.S -o build/root-clone-call-check.o
