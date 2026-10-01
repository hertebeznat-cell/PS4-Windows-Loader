# Complete reachable table snapshots

`pwl_x64_table_snapshot_capture` copies all reachable page-table frames from a
four-level root through a caller-supplied physical page reader. It never turns a
physical address into a pointer. Shared frames are read once; processing is
tracked separately for each level, so recursive root slots terminate after the
four hardware levels. Large 1 GiB/2 MiB leaves and 4 KiB leaves are not followed
as tables. RAM contents and MMIO are not copied.

The caller supplies at most 4096 distinct 4 KiB output buffers. A missing buffer,
read failure, invalid root branch, malformed large leaf or capacity exhaustion
returns an error and used_out=0. Each collected table is reread and must compare
exactly, including Accessed/Dirty bits. Changed frames stop preparation. This
may refuse a live changing context and must not be weakened to assume stability.
Even two identical reads cannot prove atomicity: CPU affinity, concurrent page
updates and allocation lifetime must be controlled by the platform.

`pwl_native_entry_capture_prepare` now connects capture, independent resident
transition-plan construction and audited EFI argument preparation in one call.
The report records stage 1 (capture), 2 (plan), 3 (entry arguments), 4 (prepared)
and the captured table count. Any failure clears the plan's root/counts. It
validates the supplied CPU mode before invoking the reader. EFI arguments stay
unchanged on preparation failure.

Complete table capture is not complete exception-context capture. It describes
where bytes are mapped; it does not discover handler code/data extents, stack
ownership, GS references or all future memory accesses. The independent plan
still requires a complete explicitly supplied dependency manifest. It does not
inherit the full old address space automatically.

No production physical reader or quiescence adapter is provided here. The
production memory binding remains unsupported. Console entry does not call this
pipeline or switch CPU roots. Hardware photographs are not table bytes. No
complete Boot Manager binary is claimed, and no repeated passed test is required.

Host checks cover shared and recursive branches, large leaves, high aliases,
capacity failure, reader errors, changed pages and malformed inputs. Integration
checks run capture -> resident plan -> relocated EFI arguments, and verify that
an unsupported PCID mode fails before capture. Native-core compilation is
freestanding, with no unresolved imports.
