#ifndef PWL_NATIVE_WORKSPACE_H
#define PWL_NATIVE_WORKSPACE_H

#include "pwl_ps4_memory.h"
#include "pwl_firmware.h"

#define PWL_NATIVE_MAX_TABLES 128U
#define PWL_NATIVE_REGION_COUNT 8U

typedef struct pwl_native_request {
    /* Resident firmware blob only; copying does NOT relocate PE/ELF code. */
    const void *firmware;
    size_t firmware_bytes;
    const void *disk_image;
    size_t disk_bytes;
    uint64_t heap_bytes;
    uint64_t stack_bytes;
    size_t table_pages;
    uint64_t image_handle;
} pwl_native_request_t;

typedef struct pwl_native_data {
    pwl_fw_memory_t memory;
    pwl_fw_media_t media;
} pwl_native_data_t;

/* Preparation-side owner. Its pointers must never be handed to EFI.
 * Only the physical addresses identify objects in the new identity context.
 */
typedef struct pwl_native_workspace {
    pwl_ps4_arena_t arena;
    pwl_owned_span_t firmware, data, tables_span, stack, media, heap;
    pwl_phys_region_t regions[PWL_NATIVE_REGION_COUNT];
    pwl_x64_identity_range_t mappings[6];
    pwl_x64_table_page_t tables[PWL_NATIVE_MAX_TABLES];
    size_t table_count;
} pwl_native_workspace_t;

/* Performs allocation -> physical verification -> resident copies -> memory
 * and media initialization -> independent page-table construction. Stack guard
 * pages are owned/reserved but unmapped. All failures unwind the kernel owner.
 * Does NOT switch CR3, install an EFI System Table, call code, relocate a blob,
 * or provide a complete machine memory map. Data pointers/callback relocation,
 * AP/IRQ/DMA state, PAT/MTRR and recoverable CPU transition are still required.
 */
pwl_status_t pwl_native_workspace_prepare(const pwl_ps4_memory_api_t *api,
                                          const pwl_native_request_t *request,
                                          pwl_native_workspace_t *workspace);
void pwl_native_workspace_release(pwl_native_workspace_t *workspace);

#endif
