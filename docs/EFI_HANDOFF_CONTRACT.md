# Contract for a future native EFI handoff

Status: design and portable validation only. No kernel backend, privileged entry,
recovery path or Windows boot is implemented by this contract. The published
Stage 4.8 payload remains preflight only.

The returning hardware context probe observed CR0 `0x8005003B`, CR3
`0x0B28B000`, CR4 `0x406F0` and EFER `0xD01` in the PS4 kernel callback.
These establish active long-mode paging and NX at that instant, but do not
verify the pointed-to tables, mappings, page ownership or a future handoff.
`pwl_x64_cpu_state_validate` now checks a supplied CPU snapshot against an
expected root and the mode assumed by the four-level table verifier. The
photographed snapshot passes that mode check with root `0x0B28B000`; this
does not provide the contents of that root or tie any later snapshot to it.

## Evidence required from a platform backend

1. An independently established physical memory map, including reservations,
   MMIO and ownership. User-process `mmap` addresses and the Linux kexec map
   marked for older firmware cannot substitute for this evidence.
2. Exclusive, resident physical pages for the relocated EFI image, stack,
   page tables and all firmware-owned structures. Preserve the EFI System Table,
   Boot/Runtime Services code and data, BCD/media backing, and any callback
   state throughout the calls that use them.
3. Actual page-table contents and the active processor mode and CR3, verified
   against those contents. All referenced memory must have coherent cache
   attributes. Enable NX before relying on NX mappings. Establish executable
   image pages and writable, non-executable stack/table pages; ensure no
   firmware callbacks jump to unmapped user-process code.
4. A defined state for all cores, interrupts, descriptor tables, machine
   state, devices and DMA. The backend must preserve or explicitly transfer
   ownership of each dependency. A separately tested way to recover from a
   failed transition is required before experimenting with Boot Manager entry.
5. A consistent UEFI memory map and configuration tables tied to that
   physical ownership. Boot Services allocation/map-key behavior and
   ExitBootServices must use the same map and remain valid across callbacks.

The x64 EFI entry receives an image handle and a pointer to the EFI System
Table using the platform's EFI calling convention. The entry must see a valid
stack and the firmware structures under the *active* mappings; a successful
PE relocation or callback in a PS4 user process does not establish this.

## Existing runtime audit

The pinned `ps4-linux-loader` runtime at commit
`f70f43a60a973b3662daeb29c1f115d95408db93` contains
`linux/ps4-kexec-common/linux_boot.c` and `linux_thunk.S`. Their path prepares
Linux `boot_params`, uses a memory map annotated for older firmware, changes
page tables, segments, processor/device state and low physical memory, then
jumps to Linux startup with no demonstrated return path. It cannot be called
as the EFI entry or used as a ready-made recoverable PS4 firmware backend.
The CI artifact links the runtime library, not this kexec transition.

## Portable checks available now

`pwl_handoff_layout_validate` checks the supplied physical placement;
`pwl_x64_handoff_tables_build` constructs four-level 4 KiB identity mappings
for the image, stack and allocated table pages in explicitly supplied physical
pages, then runs the independent snapshot verifier. It never installs CR3.
`pwl_x64_handoff_mappings_validate` checks snapshots of four-level x86-64
4 KiB tables for identity mapped image, stack and every supplied table page.
It requires supervisor pages, executable image, writable NX stack/table pages,
and rejects large pages. The test fixture in `tests/test_paging.c` covers
mismatched addresses, NX mistakes, user pages, large pages and bad snapshots.

The snapshots and the claimed physical regions are inputs, not independently
verified facts. A pass cannot prove that the CPU uses this CR3, that the
contents are stable, that every callback or platform address is mapped, or
that the claimed RAM is owned. Do not turn on Boot Manager entry based on this
validator. Next integration gate: a target-specific, read-only physical map
and page-table inventory with an independently verifiable recovery mechanism.
