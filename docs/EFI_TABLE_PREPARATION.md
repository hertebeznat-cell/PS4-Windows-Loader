# Resident EFI table preparation

`loader/src/efi_tables.c` prepares AMD64 System Table and Boot Services wire
layouts for a future resident firmware image. All addresses are 64-bit physical
destination addresses, independent of the preparation process's pointers.

The caller supplies the code/data extents and nine callback offsets, in order:
RaiseTPL, RestoreTPL, AllocatePages, FreePages, GetMemoryMap, ExitBootServices,
CalculateCrc32, CopyMem and SetMem. Offsets must lie inside the supplied code
extent. Code and data must be disjoint, within the lower four-level canonical
identity space; table storage must be sufficiently large and 8-byte aligned.
The module fills the correct Boot Services slots and computes both EFI CRC32s.
Validation reconstructs the expected tables and checks every byte, including
unused slots, so recomputing a CRC cannot hide an incorrect pointer.

This module is **table preparation only**. The separate
[resident preparation experiment](RESIDENT_PROBE.md) now supplies a linked code
image, Microsoft x64 ABI entries and workspace binding. Runtime Services,
console interfaces and remaining Boot Services are absent. These incomplete
tables must not be handed to Boot Manager. The original Workspace-Probe keeps
its synthetic code fixture; Resident-Probe never calls its code on the PS4.

Host tests exercise addresses above 4 GiB, the standard CRC32 check vector,
every single-byte table corruption, recomputed checksums with incorrect
callback/unused-slot pointers, invalid offsets, overlapping spans, truncated
storage and address overflow. The module is included in the closed freestanding
native development object and checked by the existing native-core CI steps.

The image build now derives offsets from linked symbols and audits the closed
code image. Table storage is part of the owned workspace data span. The returning
resident preparation experiment still needs hardware validation. No CPU
transition or Windows launch follows from successful table validation.
