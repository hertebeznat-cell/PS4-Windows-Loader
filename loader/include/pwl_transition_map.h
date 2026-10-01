#ifndef PWL_TRANSITION_MAP_H
#define PWL_TRANSITION_MAP_H
#include "pwl_handoff.h"
typedef struct pwl_x64_translation {
    uint64_t physical_address,leaf_bytes;
    unsigned writable,executable,user,pat_index;
} pwl_x64_translation_t;
/* Immutable same-context table snapshots, four-level paging and NX enabled.
 * Supports 4 KiB, 2 MiB, 1 GiB leaves and high canonical aliases. Success only
 * describes translation, not RAM ownership, CPU support or effective MTRRs.
 * Output is unchanged on failure. No physical address is dereferenced. */
pwl_status_t pwl_x64_translate(const pwl_x64_table_page_t *tables,size_t count,
    uint64_t root,uint64_t address,pwl_x64_translation_t *out);
typedef struct pwl_transition_range {
    uint64_t virtual_address,bytes;
    unsigned writable,executable;
} pwl_transition_range_t;
/* Verify explicitly supplied dependencies retain supervisor mappings to the
 * same physical bytes and PAT selection under both roots. Requires exact
 * effective W/NX permissions; rejects W+X requirements. The caller must supply
 * a complete dependency list (including NMI/MCE/GDT/IDT/TSS/stacks). It cannot
 * discover that list, prove snapshot stability, or authorize a CPU switch. */
pwl_status_t pwl_x64_transition_mappings_validate(
    const pwl_x64_table_page_t *before,size_t before_count,uint64_t before_root,
    const pwl_x64_table_page_t *after,size_t after_count,uint64_t after_root,
    const pwl_transition_range_t *ranges,size_t range_count,size_t *failed_range);
#endif
