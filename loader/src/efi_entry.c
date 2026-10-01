#include "pwl_efi_entry.h"
pwl_status_t pwl_native_efi_entry_prepare(const pwl_native_workspace_t *w,
 const pwl_resident_image_t *image,const pwl_native_transition_plan_t *plan,
 const pwl_x64_table_page_t *tables,pwl_efi_entry_context_t *out)
{
    size_t i;
    if(!w || !plan || !tables || !out || !w->boot_image.entry_address ||
       !plan->root || !plan->table_count || plan->table_count>PWL_NATIVE_MAX_TABLES ||
       !plan->range_count || plan->range_count>PWL_TRANSITION_MAX_RANGES ||
       plan->root!=tables[0].physical_address ||
       pwl_native_resident_environment_validate(w,image)!=PWL_OK ||
       pwl_x64_alias_tables_validate(plan->ranges,plan->range_count,tables,plan->table_count)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    /* Retain every resident mapping, not merely the application entry page. */
    for(i=0;i<w->mapping_count;i++) {
        const pwl_x64_identity_range_t *r=&w->mappings[i];uint64_t offset;
        for(offset=0;offset<r->size;offset+=PWL_PAGE_SIZE) {
            pwl_x64_translation_t x;
            if(pwl_x64_translate(tables,plan->table_count,plan->root,r->base+offset,&x)!=PWL_OK ||
               x.physical_address!=r->base+offset || x.writable!=r->writable ||
               x.executable!=r->executable || x.user || x.pat_index)
                return PWL_ERR_BAD_IMAGE;
        }
    }
    pwl_efi_entry_context_t result={w->boot_image.entry_address,
        ((const pwl_native_data_t *)w->data.prepare_address)->memory.image_handle,
        ((const pwl_native_data_t *)w->data.prepare_address)->loaded_image.system_table,0,0};
    *out=result;return PWL_OK;
}
