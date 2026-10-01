#include "pwl_entry_pipeline.h"
pwl_status_t pwl_native_entry_capture_prepare(
 const pwl_native_workspace_t *w,const pwl_resident_image_t *image,
 const pwl_x64_cpu_state_t *cpu,pwl_table_read_fn read,void *context,
 pwl_x64_table_page_t *snapshot,size_t snapshot_capacity,
 const pwl_x64_alias_range_t *deps,size_t n,const pwl_owned_span_t *span,
 pwl_x64_table_page_t *tables,size_t capacity,pwl_native_transition_plan_t *plan,
 pwl_efi_entry_context_t *entry,pwl_entry_pipeline_report_t *report)
{
    if(!report)return PWL_ERR_INVALID_ARGUMENT;
    *report=(pwl_entry_pipeline_report_t){0,PWL_ERR_INVALID_ARGUMENT,0};
    if(!plan || !entry)return report->status;
    plan->root=0;plan->range_count=0;plan->table_count=0;
    if(!cpu || pwl_x64_cpu_state_validate(cpu,cpu->cr3)!=PWL_OK)return report->status;
    report->stage=1;
    pwl_status_t status=pwl_x64_table_snapshot_capture(cpu->cr3,read,context,
        snapshot,snapshot_capacity,&report->snapshot_count);
    if(status==PWL_OK) {
        report->stage=2;
        status=pwl_native_transition_plan_prepare(w,image,snapshot,report->snapshot_count,
            cpu->cr3,deps,n,span,tables,capacity,plan);
    }
    if(status==PWL_OK) {
        report->stage=3;
        status=pwl_native_efi_entry_prepare(w,image,plan,tables,entry);
        if(status==PWL_OK)status=pwl_native_image_mapping_bind(w,image,plan,tables);
    }
    if(status!=PWL_OK) {plan->root=0;plan->range_count=0;plan->table_count=0;}
    else report->stage=4;
    report->status=status;return status;
}
