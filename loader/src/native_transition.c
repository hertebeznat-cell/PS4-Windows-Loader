#include "pwl_native_transition.h"
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
pwl_status_t pwl_native_transition_plan_prepare(
 const pwl_native_workspace_t *w,const pwl_resident_image_t *image,
 const pwl_x64_table_page_t *before,size_t before_count,uint64_t before_root,
 const pwl_x64_alias_range_t *deps,size_t n,const pwl_owned_span_t *span,
 pwl_x64_table_page_t *tables,size_t capacity,pwl_native_transition_plan_t *plan)
{
    size_t i,j,count=0,used=0; pwl_status_t status;
    if(!plan) return PWL_ERR_INVALID_ARGUMENT;
    plan->range_count=0;plan->table_count=0;plan->root=0;
    if(!w || !span || !tables || !before || !before_count || before_count>4096 ||
       !deps || !n || n>64 || !capacity ||
       capacity>PWL_NATIVE_MAX_TABLES || !span->prepare_address ||
       (uintptr_t)span->prepare_address%sizeof(uint64_t) ||
       span->size!=capacity*PWL_PAGE_SIZE || !span->physical_address ||
       span->physical_address%PWL_PAGE_SIZE ||
       span->physical_address>UINT64_MAX-span->size ||
       (uintptr_t)span->prepare_address>UINTPTR_MAX-span->size ||
       pwl_native_resident_environment_validate(w,image)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    if(overlap(span->physical_address,span->size,w->arena.physical_address,w->arena.size) ||
       overlap((uintptr_t)span->prepare_address,span->size,w->arena.kernel_address,w->arena.size))
        return PWL_ERR_INVALID_ARGUMENT;
    for(i=0;i<before_count;i++) {
        if(!before[i].entries || overlap((uintptr_t)span->prepare_address,span->size,
             (uintptr_t)before[i].entries,PWL_PAGE_SIZE)) return PWL_ERR_INVALID_ARGUMENT;
    }
    for(i=0;i<w->mapping_count;i++) {
        const pwl_x64_identity_range_t *r=&w->mappings[i];
        plan->ranges[count++]=(pwl_x64_alias_range_t){r->base,r->base,r->size,r->writable,r->executable,0};
    }
    plan->ranges[count++]=(pwl_x64_alias_range_t){span->physical_address,span->physical_address,span->size,1,0,0};
    for(i=0;i<n;i++) {
        /* Dependencies are borrowed context, never aliases into owned arenas
         * or guards. All copied bytes must retain their recorded translation. */
        if(!deps[i].bytes || overlap(deps[i].physical_address,deps[i].bytes,w->arena.physical_address,w->arena.size) ||
           overlap(deps[i].physical_address,deps[i].bytes,span->physical_address,span->size))
            return PWL_ERR_INVALID_ARGUMENT;
        plan->ranges[count++]=deps[i];
    }
    for(i=1;i<count;i++) {
        pwl_x64_alias_range_t r=plan->ranges[i];j=i;
        while(j && plan->ranges[j-1].virtual_address>r.virtual_address) {
            plan->ranges[j]=plan->ranges[j-1];--j;
        }
        plan->ranges[j]=r;
    }
    for(i=0;i<capacity;i++) {
        tables[i].physical_address=span->physical_address+i*PWL_PAGE_SIZE;
        tables[i].entries=(uint64_t *)((unsigned char *)span->prepare_address+i*PWL_PAGE_SIZE);
    }
    status=pwl_x64_alias_tables_build(plan->ranges,count,tables,capacity,&used);
    if(status!=PWL_OK) return status;
    for(i=0;i<n;i++) {
        uint64_t offset;
        for(offset=0;offset<deps[i].bytes;offset+=PWL_PAGE_SIZE) {
            pwl_x64_translation_t x;
            status=pwl_x64_translate(before,before_count,before_root,deps[i].virtual_address+offset,&x);
            if(status!=PWL_OK || x.physical_address!=deps[i].physical_address+offset ||
               x.writable!=deps[i].writable || x.executable!=deps[i].executable ||
               x.user || x.pat_index!=deps[i].pat_index) return PWL_ERR_BAD_IMAGE;
        }
        pwl_transition_range_t bridge={deps[i].virtual_address,deps[i].bytes,deps[i].writable,deps[i].executable};
        status=pwl_x64_transition_mappings_validate(before,before_count,before_root,
            tables,used,span->physical_address,&bridge,1,NULL);
        if(status!=PWL_OK) return status;
    }
    plan->range_count=count;plan->table_count=used;plan->root=span->physical_address;
    return PWL_OK;
}
