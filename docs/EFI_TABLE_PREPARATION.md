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

This is **table preparation only**. The nine function implementations, their
Microsoft x64 calling conventions and a relocatable resident code image are
not supplied by this module. Runtime Services, console interfaces and remaining
Boot Services are absent. These incomplete tables must not be handed to Boot
Manager. The workspace experiment remains synthetic and unchanged; no new
console binary or hardware run is requested by this change.

Host tests exercise addresses above 4 GiB, the standard CRC32 check vector,
every single-byte table corruption, recomputed checksums with incorrect
callback/unused-slot pointers, invalid offsets, overlapping spans, truncated
storage and address overflow. The module is included in the closed freestanding
native development object and checked by the existing native-core CI steps.

Next: build and audit the actual resident code image, derive offsets from its
linked symbols, reserve table storage in the workspace, verify all relocations
and mappings, then connect a returning preparation experiment. No CPU transition
or Windows launch follows from successful table validation.
