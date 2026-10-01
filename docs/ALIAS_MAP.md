# Independent virtual mapping construction

`pwl_x64_alias_tables_build` constructs four-level 4 KiB tables from an explicit
manifest: virtual address, physical address, size, writable/executable flags and
PAT index. Low identity addresses and high canonical aliases can share a root.
No source kernel branches are copied. Every table frame is supplied by its owner;
its physical address is not inferred from a C pointer. Construction never reads
physical memory or activates the root.

Ranges are aligned, sorted and disjoint in virtual space. Physical aliases are
allowed only with matching PAT indices and without executable/writable aliases
of the same bytes. A table frame cannot be mapped executable. Unlisted addresses
remain absent, allowing explicit gaps for guarded stacks. Cache indices describe
the caller's active PAT; matching indices alone do not establish MTRR coherence.

The independent verifier rejects unexpected leaves, wrong frames, permissions or
PAT, malformed branches, shared/cyclic tables and unreachable supplied pages.
It also walks every requested page to detect missing mappings. It is intended
for newly prepared, inactive tables and rejects hardware Accessed/Dirty bits.
On any construction failure, used_out is zero; partial contents must not be used.
Inputs rejected before construction leave the supplied table bytes unchanged.

This is a construction API, not a platform memory backend or launch command.
The platform must supply owned table frames, stable translations and a complete
manifest of code, data, descriptor tables, exception handlers and stack extents.
A photographed handler entry is insufficient to determine all its dependencies.
The transition continuity verifier must then compare the required old/new
mappings before an independently justified CPU transition. No new console test
is needed to exercise this software-only builder.

## Resident preparation integration

`pwl_native_transition_plan_prepare` audits the existing resident environment,
combines its identity mappings with explicit old-context dependency aliases and
constructs a separate root in a distinct caller-owned table span. All source
workspace regions, including its guard frames, are forbidden physical targets
for borrowed dependencies. The new table span is mapped writable/NX with PAT
index zero; the platform must establish that index zero is WB for these owned
allocations. Source snapshot buffers may not overlap the destination span.

Each dependency is checked against the supplied old table snapshots, including
physical bytes and effective permissions/PAT, and then compared under both roots.
Only a successful construction publishes nonzero plan counts and root. The old
workspace and its tables are retained unchanged. No extra allocation or release
is performed by this API: both owners must remain alive. The caller must publish
the second table allocation in its firmware memory map before eventual entry.
The dependency list's completeness and snapshot stability remain platform
requirements; this function does not discover handler or GS data extents.

Host integration tests cover preserved resident code/data permissions, high
aliases, addresses above 4 GiB, both absent stack guards, unchanged old tables,
cache/physical mismatch refusal and insufficient table capacity. The normal
console entry remains preflight-only. This is no new executable console file.
