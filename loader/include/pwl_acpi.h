#ifndef PWL_ACPI_H
#define PWL_ACPI_H
#include "pwl_alias_map.h"

#define PWL_ACPI_MAX_TABLES 64U
#define PWL_ACPI_MAX_TABLE_BYTES (1024U * 1024U)
#define PWL_ACPI_MAX_SNAPSHOT_BYTES (4U * 1024U * 1024U)
typedef struct pwl_acpi_extent { uint64_t address, bytes; } pwl_acpi_extent_t;
typedef pwl_status_t (*pwl_acpi_read_fn)(void *,uint64_t,void *,size_t);
typedef struct pwl_acpi_source {
    pwl_acpi_read_fn read;
    void *context;
    /* Actual readable RAM extents, sorted and disjoint. Not inferred from a
     * firmware version, historical E820 list or an arbitrary pointer. */
    const pwl_acpi_extent_t *extents;
    size_t extent_count;
} pwl_acpi_source_t;
enum pwl_acpi_kind { PWL_ACPI_RSDP, PWL_ACPI_SDT, PWL_ACPI_FACS };
typedef struct pwl_acpi_table {
    uint64_t physical_address;
    uint32_t bytes, offset, kind;
} pwl_acpi_table_t;
typedef struct pwl_acpi_snapshot {
    const unsigned char *storage; /* Preparation bytes only; never an EFI pointer. */
    size_t bytes, count;
    pwl_acpi_table_t tables[PWL_ACPI_MAX_TABLES];
} pwl_acpi_snapshot_t;

/* Optional discovery within an explicitly supplied, at most 2 MiB RAM span.
 * No implicit reads at 0xe0000 or through a guessed direct map. */
pwl_status_t pwl_acpi_find_rsdp(const pwl_acpi_source_t *source,
    uint64_t start,uint64_t bytes,uint64_t *address);
/* Snapshot RSDP, both supplied roots, all root children, FADT's preferred
 * DSDT/FACS. Bound every read before calling the platform reader; verify
 * checksums and lengths, reject overlapping tables and inconsistent headers.
 * Caller keeps source stable, supplies fault-contained reads, and distinct
 * writable storage/output. On failure out is zero; storage may be partial.
 * Neither copied AML nor copied FACS is activated. Original PAs are retained.
 */
pwl_status_t pwl_acpi_capture(const pwl_acpi_source_t *source,uint64_t rsdp,
    void *storage,size_t capacity,pwl_acpi_snapshot_t *out);
pwl_status_t pwl_acpi_snapshot_validate(const pwl_acpi_snapshot_t *snapshot);
/* Compare the whole graph again with its original physical bytes. This
 * detects changes during preparation; it does not pin RAM or stop firmware. */
pwl_status_t pwl_acpi_snapshot_recheck(const pwl_acpi_source_t *source,
    const pwl_acpi_snapshot_t *snapshot);
/* Sorted, merged identity mappings of the ORIGINAL table pages. Shared FACS
 * pages are writable; other pages are RO. All NX, supervisor, PAT index 0.
 * Required output capacity is supplied without modifying ranges on shortage.
 * Platform must retain these RAM pages and establish WB effective caching.
 * Embedded AML/MMIO/other physical references remain platform dependencies.
 */
pwl_status_t pwl_acpi_ranges(const pwl_acpi_snapshot_t *snapshot,
    pwl_x64_alias_range_t *ranges,size_t capacity,size_t *count);
pwl_status_t pwl_acpi_mappings_validate(const pwl_acpi_snapshot_t *snapshot,
    const pwl_x64_table_page_t *tables,size_t count,uint64_t root);
#endif
