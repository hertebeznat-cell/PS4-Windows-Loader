# Mapping continuity for an independent EFI root

The returning All30 result proves callbacks work under an identical root clone.
It does not prove that code required during an independent-root transition stays
mapped. `pwl_x64_transition_mappings_validate()` checks explicitly supplied
ranges in before/after snapshots without dereferencing any physical address.

`pwl_x64_translate()` walks four-level tables, including 4 KiB, 2 MiB and 1 GiB
leaves, both canonical halves and physical addresses above 4 GiB. Intermediate
RW, US and NX restrictions are accumulated. It decodes leaf PAT selection,
rejects missing pages, duplicate snapshot addresses, noncanonical addresses,
invalid PML4 large-page entries and reserved large-leaf address alignment bits.
The decoder uses the architectural 52-bit address field; actual CPU physical
width, large-page support, PKU/SMEP/SMAP and cache configuration must be checked
separately. Accessed/dirty bits in a captured snapshot are accepted. Failure
leaves the translation output unchanged.

Continuity checks require supervisor mappings to the same physical bytes and
PAT selection, with exact requested effective write/execute permissions. A
range crossing a 4 KiB subpage boundary compares both sides even when one root
uses a large leaf and the other uses small leaves. Range overflow, canonical
hole crossing and W+X requirements are rejected. `failed_range` identifies the
first failing dependency; SIZE_MAX means no individual failure was selected.

A platform adapter must capture stable tables and enumerate all transition
code/report ranges, GDT, IDT, TSS, handler code/data and exception stacks including
NMI and machine-check paths. This module cannot discover dependencies or prove
that enumeration complete. It supplies no physical ownership, table mutation,
CPU/device shutdown or recovery implementation. The generic development
`pwl_x64_address_call()` remains unwired to the console, and Stage 4.8 remains
preflight. No new console diagnostic is distributed with this change.

Host tests compare large/small high-address aliases above 4 GiB, inherited NX,
PAT mismatch, changed second-subpage translation, malformed/absent snapshots
and unchanged output on failure. The module is included in the syscall-free
freestanding core and the existing sanitizer test suite. No emulator is used.
