# Returning resident EFI preparation experiment

This experiment replaces the synthetic three-byte firmware fixture with the
linked position-independent resident service image. It copies that image into
owned memory, patches its single state binding to the owned data PA, prepares
AMD64 EFI tables, verifies callback addresses/CRCs, checks identity mappings and
copy integrity, then releases the original allocation and returns.

**It does not execute resident code on the PS4, activate CR3, enter Microsoft
code or launch Windows.** The disk remains a synthetic read-only fixture.
The production memory-binding gate remains unchanged. This returning diagnostic
uses the same guarded experimental allocation adapter as Workspace-Probe.

## Checks performed before publication

The resident image is linked at zero with RIP-relative internal references and
one explicit eight-byte state binding. Build checks reject unresolved imports,
remaining relocations, writable data/BSS and unexpected CPU/process entry
instructions. Callback offsets are extracted from executable linked symbols.
The image CRC and metadata are checked before allocation. Host tests copy the
actual linked image into a new RX mapping above 4 GiB and call all nine exported
entries using Microsoft x64 calling conventions. Workspace tests separately
verify the physical destination binding, tables, RX/RW-NX mappings and cleanup.

The nine entries are RaiseTPL, RestoreTPL, AllocatePages, FreePages,
GetMemoryMap, ExitBootServices, CalculateCrc32, CopyMem and SetMem. TPL currently
tracks serialized preparation state only; it does not manage interrupts/events.
ExitBootServices deliberately returns EFI_UNSUPPORTED without retiring memory:
the complete platform handoff is not implemented. Remaining services, Runtime
Services and console interfaces are absent. The tables are incomplete and must
not be presented to Boot Manager.

## Single hardware preparation test

USB journal version 2 creates and syncs `PS4WL_RESIDENT.LOG` on USB0 or USB1
before checking the environment or locking memory. It writes `ENTERED` and
`STARTING_TEST` checkpoints before entering the experiment. An early rejection
records its reason; a test that does not return leaves the last checkpoint.
If neither USB accepts a synced entry, the experiment is not started. Screen
notifications show the operation stage and errno for both USB paths. Write,
sync and close failures are returned as logging failures rather than success.

Hardware behavior for this new build is unverified. The larger-memory test
was reported successful in the prior conversation; this new resident variant
still needs its own returning result. A hang or crash remains possible.

1. Use the dedicated USB test medium already used for the returning probes.
2. Send `PS4WindowsLoader-Resident-Probe.bin` once using the existing method.
3. Retrieve `PS4WL_RESIDENT.LOG` from the USB, or photograph notifications if
   logging failed.
4. Expected: `rc=0 stage=5 error=0`, `prep=0 tables_status=0 release=0`,
   `copies=1`, `efi_status=0 mode=PREPARATION_ONLY code_called=0`.
5. If it hangs, do not repeat the run. Preserve the result for diagnosis.

`stage=5` means this preparation diagnostic returned; it is not Windows boot.
No new Windows files are required and the completed memory/workspace tests do
not need repeating. Build IDs and checksums accompany the artifact.

Build: `sh tools/build_workspace_probe.sh SDK_DIRECTORY COMMIT resident`.
