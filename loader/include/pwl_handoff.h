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

#endif
