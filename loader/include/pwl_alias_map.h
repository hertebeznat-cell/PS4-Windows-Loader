#ifndef PWL_ALIAS_MAP_H
#define PWL_ALIAS_MAP_H
#include "pwl_transition_map.h"
/* Explicit, sorted, disjoint virtual ranges. Physical aliases are permitted
 * only with identical PAT indices and without writable/executable aliasing.
 * Caller owns every table and supplies its actual physical address. No CPU
 * activation, physical dereference, allocation or implicit mapping occurs. */
typedef struct pwl_x64_alias_range {
    uint64_t virtual_address, physical_address, bytes;
    unsigned writable, executable, pat_index;
} pwl_x64_alias_range_t;
pwl_status_t pwl_x64_alias_tables_build(const pwl_x64_alias_range_t *ranges,
    size_t range_count, pwl_x64_table_page_t *tables, size_t capacity,
    size_t *used_out);
/* Rejects extra mappings, shared/cyclic branches, unreachable pages and any
 * permissions/cache bits different from the manifest. For unused tables only:
 * hardware Accessed/Dirty bits are intentionally not accepted. */
pwl_status_t pwl_x64_alias_tables_validate(const pwl_x64_alias_range_t *ranges,
    size_t range_count, const pwl_x64_table_page_t *tables, size_t used);
#endif
