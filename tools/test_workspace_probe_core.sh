#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
if gcc -std=c11 -fsyntax-only -ffreestanding -DPWL_WORKSPACE_DIAGNOSTIC \
 -DPWL_WORKSPACE_HOST_TEST -Iloader/include -Ipayload payload/workspace_probe_core.c \
 2>build/workspace-host-rejection.txt; then
 echo 'Workspace host fixture unexpectedly compiled freestanding' >&2
 exit 1
fi
grep -q 'Workspace host fixtures must never' build/workspace-host-rejection.txt
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -DPWL_WORKSPACE_DIAGNOSTIC -DPWL_WORKSPACE_HOST_TEST -Iloader/include -Ipayload \
 payload/workspace_probe_core.c tests/test_workspace_probe_core.c \
 loader/src/main.c loader/src/pe_loader.c loader/src/handoff.c loader/src/paging.c \
 loader/src/ps4_memory.c loader/src/firmware_memory.c loader/src/firmware_files.c loader/src/firmware_media.c \
 loader/src/native_workspace.c -o build/test-workspace-probe-core
./build/test-workspace-probe-core
