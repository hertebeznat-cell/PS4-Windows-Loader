# Original ACPI graph preparation

Implemented in `acpi.c`, `acpi_configuration.c` and `native_acpi.c`; connected
to the native environment and final-root EFI entry audits. Preparation only,
with native host tests, no emulator and no new console execution result.

## Capture and discovery

`pwl_acpi_source_t` supplies a platform reader and sorted, disjoint readable
RAM extents. Every requested byte must lie within these extents BEFORE invoking
the reader. Adjacent extents can cover one table; unknown gaps are refused.
The platform must provide fault-contained reads and a stable preparation
context. This layer cannot make an unchecked kernel dereference recoverable.
No direct-map mask, firmware-derived address, MMIO read, or implicit read of
physical 0xe0000 is performed. `pwl_acpi_find_rsdp` searches only an explicit
span of at most 2 MiB, on 16-byte boundaries, without reading past that span.

`pwl_acpi_capture` supports RSDP revisions 0 and 2, validates the legacy and
extended checksums, and follows both nonzero RSDT/XSDT pointers. Other RSDP
revisions/extended sizes are refused. It captures every root child, requires a
single FADT physical object across both roots, and follows the preferred
nonzero X_DSDT/X_FIRMWARE_CTRL rather than silently falling back from a bad
extended address. A missing FACS is allowed only with HARDWARE_REDUCED_ACPI.
Both the legacy 116-byte FADT and extended FADTs of at least 148 bytes are
accepted; incomplete pointer fields are refused. These checks do not certify
the rest of FADT fields, MADT subtables, AML methods or Windows compatibility.

Limits are 64 objects, 1 MiB per table and 4 MiB total scratch capacity.
Snapshots retain original PAs, table byte lengths and aligned scratch offsets.
SDT checksums, full RSDP checksums, headers, bounds and graph closure are audited.
FACS has its own signature/length and 64-byte physical alignment; it has no SDT
checksum. Conflicting/overlapping physical objects, scratch overlap, root
cycles, root-listed DSDT/FACS, unrelated extra snapshot objects and inconsistent
header/full reads are refused. Shared root references reuse the same object.
Failure clears the result; scratch bytes may contain an incomplete capture.
Storage, metadata and reader inputs must be distinct and remain stable.

`pwl_acpi_snapshot_recheck` compares every captured byte with the original RAM
again in bounded chunks, including the FACS. Any observed change is a refusal.
Passing is evidence about those reads, not a mechanism that pins the source or
prevents a later firmware update.

## Mapping, lifetime and publication

`pwl_acpi_ranges` derives sorted, coalesced identity spans of original table
pages. It never fills gaps. Pages are RO/NX, except any page shared with FACS is
RW/NX. All mappings are supervisor and PAT index zero. A size query reports the
required range count; a short output changes no range bytes. The independent
mapping audit walks each original page and verifies its PA, effective W/NX,
supervisor permission and PAT selection.

The platform must retain original table RAM through the returning call, retain
scratch audit bytes until workspace release, and prove effective WB caching.
The new-root plan must already include these original identity mappings. The
existing transition dependency API preserves supplied old-root aliases; it
does not itself create previously absent original-table identity mappings.
ACPI table ranges are not the complete machine dependency inventory.

`pwl_native_acpi_publish` checks the current EFI entry environment, source
recheck, final root and reservation space before any mutation. It refuses
overlap with the owned arena (including tail padding), transition table frames
or ANY existing EFI memory descriptor. On success it inserts sorted type-10
ACPI NVS descriptors, with WB baseline and no manager allocation ownership,
for all original table pages. No unknown/free RAM is created. Map key advances
and an old issued key is invalidated. Reserving every table as NVS is deliberately
conservative for this returning path; no ACPI reclaim policy is enabled.

Publication uses the correct ACPI 1.0 or 2.0 configuration GUID and ORIGINAL
RSDP PA, the resident configuration array PA, and a new SystemTable CRC.
No preparation KVA or scratch address becomes an EFI pointer. The environment
audit checks exact GUID/address, unused configuration slots, source snapshot
graph and non-freeable NVS coverage. The EFI entry audit independently checks
the final graph mappings, so a configuration pointer alone cannot pass entry.
This can coexist with fixed GOP/text publication and its CRC normalization.

In particular, the copied FACS is an audit snapshot. Its Global Lock, waking
vectors and other shared state are never cloned into an active relocated
firmware object. Firmware and OSPM must continue to address the same original
FACS. Original AML and other table-embedded addresses remain unchanged: MMIO,
OperationRegion, other pointed buffers, firmware methods and wake/device
ownership are additional platform dependencies. This implementation cannot
promise they work under Windows just because the graph checksums are valid.

## Validation and remaining connection

`test_acpi.c` exercises both RSDP revisions/roots, shared children, a preferred
DSDT above 4 GiB with an ignored legacy pointer, FACS, hardware-reduced FACS
absence, corrupt/changed/oversized/truncated/cyclic/out-of-range input, short
output, and final mapping refusal. `test_native_workspace.c` covers EFI
publication with the fixed console, missing mappings, changed FACS, exact
configuration CRC/address, memory key update, non-freeable reservations and
unchanged state on refusal. ASan and UBSan remain enabled.

Freestanding builds include this backend and have no undefined imports. The
existing console build variants retain their previous mode; neither ACPI nor
Microsoft entry is enabled merely by linking the code. The real PS4 physical
reader, current table location, retained-source ownership, complete inventory
and device/runtime handoff are still absent. Historical upstream E820 entries
explicitly attributed to FW 1.01 are not evidence for FW 13.52.

Primary specifications: [ACPI 6.6 software model](https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html)
and [Microsoft ACPI table requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/acpi-system-description-tables).
