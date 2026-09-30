# Returning identical-root experiment

Build with `sh tools/build_workspace_probe.sh SDK_DIRECTORY COMMIT root-clone`.
This is a separate returning diagnostic for the same 13.52 target already used
by the memory and resident probes. It does not activate the prepared EFI root.

One invocation checks the current CPU paging mode and verifies the same-build
extractor's direct-map construction bytes at `+0x57410..+0x57440`. It reads the
PML4/PDPT indices from `+0x1B2C394` and `+0x1B2C398`, requires indices within the
upper canonical map and a 32 GiB physical window, and confirms that the kernel
root pointer at object+0x20 equals its independently constructed direct-map
alias. Both ends of that kernel page must translate correctly.

The source to copy is then the **live CR3's** direct-map alias, not necessarily
the kernel pmap root. Both ends of the active root page must translate to live
CR3 and CR3+4095 before any source table contents are read. Failure refuses
before allocation. No source is selected by scanning object fields or guessing
an address mask. The assembly still checks live CR3 and all 512 entries
immediately before the transition.
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

## Active-root correction

The supplied capture with SHA-256
`566b5d65a4d94d2392fa85e6bb573478fb9cbc844a807e5eba18ac3793dc0116`
corroborates the formula at extractor+0x40 (`base+0x57410`):
`0xffff800000000000 | (dmpml4i << 39) | (dmpdpi << 30)`, followed
by addition of the physical address. Initialization at `+0x56235..+0x56272`
uses the same globals to publish kernel_pmap_store+0x20, while
`+0x560B1..+0x560D9` installs 32 PDPT entries for the direct map. Captured
indices 503/348 and kernel-root alias `0xfffffbd706c3c000` are static capture
evidence; they are never hardcoded as the live values.

The previous `error=23` proved only that the kernel-root PA differed from the
active CPU root, not that active CR3 was invalid. This revision resolves the
active root through the verified direct map and keeps the exact-translation
refusal. Host regression checks cover distinct kernel/process roots, physical
addresses above 4 GiB, invalid indices/ranges, and missing first/last-byte
translations. This revised resolution and root transition still require a
console result; compilation is not that evidence. Error 31 now denotes a
direct-map identity/range refusal; error 32 denotes changed extractor bytes.
