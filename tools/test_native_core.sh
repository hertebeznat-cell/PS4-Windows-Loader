#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
compiler=${CC:-cc}
sanitizers=${1:-address,undefined}
mkdir -p build
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    loader/src/handoff.c loader/src/firmware_memory.c loader/src/firmware_media.c \
    tests/test_firmware.c -o build/test-firmware
./build/test-firmware
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
    -fsanitize="$sanitizers" -fno-omit-frame-pointer -Iloader/include \
    loader/src/handoff.c loader/src/paging.c loader/src/ps4_memory.c \
    loader/src/firmware_memory.c loader/src/firmware_media.c loader/src/native_workspace.c \
    tests/test_native_workspace.c -o build/test-native-workspace
./build/test-native-workspace
