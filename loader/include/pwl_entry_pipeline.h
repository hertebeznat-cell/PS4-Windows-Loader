#ifndef PWL_ENTRY_PIPELINE_H
#define PWL_ENTRY_PIPELINE_H
#include "pwl_table_snapshot.h"
#include "pwl_efi_entry.h"
typedef struct pwl_entry_pipeline_report {
    unsigned stage;
    pwl_status_t status;
    size_t snapshot_count;
} pwl_entry_pipeline_report_t;
/* One preparation transaction: full old-root snapshot -> independent plan ->
 * audited EFI arguments. Complete dependencies, stable context and both buffer
 * owners remain platform requirements. No guessed reads, CPU switch or entry.
 * Caller buffers/context outputs are distinct; failed plans never have a root.
 * This API cannot certify the supplied dependency manifest is complete. */
pwl_status_t pwl_native_entry_capture_prepare(
    const pwl_native_workspace_t *workspace,const pwl_resident_image_t *image,
    const pwl_x64_cpu_state_t *cpu,pwl_table_read_fn read,void *read_context,
    pwl_x64_table_page_t *snapshot,size_t snapshot_capacity,
    const pwl_x64_alias_range_t *dependencies,size_t dependency_count,
    const pwl_owned_span_t *table_span,pwl_x64_table_page_t *tables,size_t capacity,
    pwl_native_transition_plan_t *plan,pwl_efi_entry_context_t *entry,
    pwl_entry_pipeline_report_t *report);
#endif
