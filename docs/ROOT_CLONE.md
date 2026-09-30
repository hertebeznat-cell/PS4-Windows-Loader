# Returning identical-root experiment

Build with `sh tools/build_workspace_probe.sh SDK_DIRECTORY COMMIT root-clone`.
This is a separate returning diagnostic for the same 13.52 target already used
by the memory and resident probes. It does not activate the prepared EFI root.

One invocation checks the current CPU paging mode, obtains the existing
kernel-pmap root candidate at the same-build field offset `0x20`, and verifies
its physical translation against the live CR3. A candidate whose PA differs
from live CR3 is refused: no alternate candidates or physical-address masks
are guessed. This field's meaning is not assumed merely from its offset.

Only after that match, the experiment allocates 32 KiB, verifies all physical
page translations, copies the complete 4 KiB root, and verifies the snapshot.
The first 16 KiB hold the root; the last 16 KiB provide a temporary stack. The
stack has no unmapped guard pages. All mappings in the root, including kernel
and exception dependencies, remain identical and refer to the existing host
page tables. Existing ownership is retained; no shared lower table is edited.

Immediately before switching, assembly masks ordinary interrupts, checks the
live root again and compares all 512 original and cloned entries. A changed
root or snapshot causes return without switching. The entry then selects the
cloned root and temporary stack, checks a stack push/pop, reads the entered
root, restores the original root and stack, restores flags and returns. No
callback, SDK call or allocator runs under the cloned root. Code and exception
mappings are retained from the source root; this is not the separate resident
address environment needed by Windows Boot Manager.

The copy and input gates are tested on the development computer. The assembly
is compiled and inspected, but no emulator is used and it has not executed on
PS4. A passing build is not hardware execution evidence. The console test may
crash. Do not repeat a hung run.

## Single console invocation

- Connect the dedicated USB and send `PS4WindowsLoader-Root-Clone.bin` once.
- Return notification photos with the active root, source PA, CPU state and
  transition results. `PS4WL_TRANSITION.LOG` is optional.
- Expected successful return: `rc=0 stage=5 error=0`, then `status=0 error=0
  switched=1 restored=1 released=1 mode=IDENTICAL_ROOT_CLONE windows_called=0`.
- If the candidate does not match live CR3: `transition error=3 switched=0`.
  This is an intentional refusal before allocation/switching. Return the notifications; do not repeat or substitute an address.

The root-clone build now aligns its entry stack explicitly and writes
`RAW_ENTERED` through direct open/write/fsync/close system calls, before SDK
initialization, formatting and notifications. The small entry function keeps
large notification/result frames out of this first stage. It handles partial
writes and EINTR without libc errno and records `KERNEL_LIBRARY_READY` and
`LIBC_READY` separately. USB journal failures no longer stop this diagnostic; notifications carry the
CPU state, source translation and root/stack round-trip results. The source root
and switching logic are unchanged by this journal fix. Absence of the previous
log alone does not establish which startup operation failed.

A USB STARTING_TEST checkpoint is attempted before the kernel callback;
no USB calls are possible while inside the bounded transition. If it hangs,
a successfully persisted checkpoint can be STARTING_TEST. Persistence has
not been established on the user's removable drive. Successful
return logs both root and stack addresses and releases the original owner.
Invalid returned ownership is retained rather than freeing a guessed address.

Passing proves only that an identical-map root switch and stack round trip
returned. It does not prove exception recovery after a fault, a full OS
handoff, independent EFI mappings or Windows readiness.

## Evidence from the console

The previous console invocation returned `rc=0 stage=2 error=23`, with
`KVA=0 PA=0`. This is the source-translation mismatch refusal before
allocation and root switching, not a successful switch. Earlier workspace,
resident callback and process-stack checks remain valid for their tested
scopes. The mismatch gate remains mandatory; notifications now expose the
addresses needed to assess the refusal without a removable-drive log.
