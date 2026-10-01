#ifndef PWL_TABLE_SNAPSHOT_H
#define PWL_TABLE_SNAPSHOT_H
#include "pwl_transition_map.h"
#define PWL_SNAPSHOT_MAX_TABLES 4096U
/* Platform callback copies exactly one owned/reachable table page from its
 * verified physical mapping. Core never derives a pointer from a PA. Caller
 * establishes a stable, quiescent four-level CPU context before capture. */
typedef pwl_status_t (*pwl_table_read_fn)(void *context,uint64_t physical_address,
                                       uint64_t out[512]);
/* Collect ALL reachable table frames, including shared/recursive branches,
 * without enumerating leaf RAM or reading MMIO. Caller supplies distinct 4 KiB
 * buffers in tables[].entries; physical_address fields are outputs. Two reads
 * of every frame must match exactly. This detects mutations, not global
 * atomicity: unchanged rereads alone cannot certify snapshot stability.
 * On failure used_out is zero; buffers are partial and never executable. */
pwl_status_t pwl_x64_table_snapshot_capture(uint64_t root,
    pwl_table_read_fn read,void *context,pwl_x64_table_page_t *tables,
    size_t capacity,size_t *used_out);
#endif
