#ifndef PWL_HANDOFF_H
#define PWL_HANDOFF_H

#include "pwl.h"

#define PWL_PAGE_SIZE UINT64_C(4096)

/* Supplied by a future platform backend, never derived from mmap pointers. */
typedef enum pwl_memory_kind {
    PWL_MEMORY_RESERVED,
    PWL_MEMORY_FREE,
    PWL_MEMORY_LOADER_CODE,
    PWL_MEMORY_LOADER_DATA,
    PWL_MEMORY_MMIO
} pwl_memory_kind_t;

typedef struct pwl_phys_region {
    uint64_t base;
    uint64_t length;
    pwl_memory_kind_t kind;
} pwl_phys_region_t;

/* Physical placement only. This does not describe virtual mappings or CPU state. */
typedef struct pwl_handoff_layout {
    uint64_t page_table_root_pa;
    uint64_t image_pa;
    uint64_t image_size;
    uint64_t entry_offset;
    uint64_t stack_pa;
    uint64_t stack_size;
} pwl_handoff_layout_t;

/* Regions must be nonempty, sorted, disjoint, and 4 KiB aligned.
 * Gaps are unknown and cannot be used for allocations.
 */
pwl_status_t pwl_memory_map_validate(const pwl_phys_region_t *regions,
                                    size_t count);

/* Validate placement metadata only. PWL_OK never authorizes a CPU transition.
 * The backend must separately verify RAM ownership, all page tables, virtual
 * mappings, CPU/device state and firmware tables before any handoff.
 */
pwl_status_t pwl_handoff_layout_validate(const pwl_phys_region_t *regions,
                                        size_t count,
                                        const pwl_handoff_layout_t *layout);

/* Snapshot of a 4 KiB x86-64 page-table page, supplied by the platform.
 * The verifier accepts only four-level 4 KiB identity mappings for the
 * image, stack, and every supplied table page. It does not construct tables,
 * verify actual CR3, or control CPU/device state.
 */
typedef struct pwl_x64_table_page {
    uint64_t physical_address;
    const uint64_t *entries; /* Exactly 512 entries. */
} pwl_x64_table_page_t;

/* CPU register snapshot and table pages must come from the same quiescent
 * context. This checks the mode/root relationship, not the provenance or
 * stability of either input. Never use a photographed CR3 as table evidence.
 */
typedef struct pwl_x64_cpu_state {
    uint64_t cr0;
    uint64_t cr3;
    uint64_t cr4;
    uint64_t efer;
} pwl_x64_cpu_state_t;

pwl_status_t pwl_x64_cpu_state_validate(
    const pwl_x64_cpu_state_t *cpu, uint64_t expected_root_pa);

pwl_status_t pwl_x64_handoff_mappings_validate(
    const pwl_phys_region_t *regions, size_t region_count,
    const pwl_handoff_layout_t *layout,
    const pwl_x64_table_page_t *tables, size_t table_count);

/* UEFI 2.x EFI_MEMORY_DESCRIPTOR version 1, 40 bytes on x86-64. */
typedef struct pwl_efi_memory_descriptor {
    uint32_t type;
    uint32_t padding;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} pwl_efi_memory_descriptor_t;

/* Cacheability is supplied by the platform backend for every region.
 * Only one explicitly known baseline type is accepted per descriptor:
 * UC=1, WC=2, WT=4, WB=8, UCE=16. This conversion cannot discover it.
 * count_out is set before capacity is checked, as in a size query.
 */
pwl_status_t pwl_efi_descriptors_from_regions(
    const pwl_phys_region_t *regions, const uint64_t *cacheability,
    size_t count, pwl_efi_memory_descriptor_t *out, size_t capacity,
    size_t *count_out);

#endif
