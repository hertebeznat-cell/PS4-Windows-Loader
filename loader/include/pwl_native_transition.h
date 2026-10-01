#ifndef PWL_NATIVE_TRANSITION_H
#define PWL_NATIVE_TRANSITION_H
#include "pwl_native_workspace.h"
#include "pwl_alias_map.h"
#define PWL_TRANSITION_MAX_RANGES (8U + PWL_PE_MAX_RANGES + 64U)
typedef struct pwl_native_transition_plan {
    pwl_x64_alias_range_t ranges[PWL_TRANSITION_MAX_RANGES];
    size_t range_count, table_count;
    uint64_t root;
} pwl_native_transition_plan_t;
/* Build a separate, inactive root retaining the audited resident workspace's
 * identity ranges and explicit old-context dependencies. Caller supplies a
 * distinct owned contiguous table span: its ownership/lifetime is a platform
 * contract, not inferred from prepare_address. The old workspace is unchanged.
 * Both workspaces must remain owned throughout any later use. Table contents
 * may be partial on failure; plan counts/root stay zero. No CPU entry. */
pwl_status_t pwl_native_transition_plan_prepare(
    const pwl_native_workspace_t *workspace,const pwl_resident_image_t *image,
    const pwl_x64_table_page_t *before,size_t before_count,uint64_t before_root,
    const pwl_x64_alias_range_t *dependencies,size_t dependency_count,
    const pwl_owned_span_t *table_span,pwl_x64_table_page_t *tables,
    size_t capacity,pwl_native_transition_plan_t *plan);
#endif
