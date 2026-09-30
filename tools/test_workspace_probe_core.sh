#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -DPWL_WORKSPACE_DIAGNOSTIC -Iloader/include -Ipayload \
 payload/workspace_probe_core.c tests/test_workspace_probe_core.c \
 loader/src/main.c loader/src/pe_loader.c loader/src/handoff.c loader/src/paging.c \
 loader/src/ps4_memory.c loader/src/firmware_memory.c loader/src/firmware_files.c loader/src/firmware_media.c \
 loader/src/native_workspace.c -o build/test-workspace-probe-core
./build/test-workspace-probe-core
