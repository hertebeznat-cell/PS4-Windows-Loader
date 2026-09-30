#ifndef PWL_ROOT_CLONE_SOURCE_H
#define PWL_ROOT_CLONE_SOURCE_H
#include "pwl_root_clone.h"
#include "root_clone_report.h"
/* No table contents are read here. Validate each alias through the supplied
 * translator before the privileged caller copies the resolved root page. */
static int pwl_root_clone_source(void *pmap,pwl_workspace_extract_fn extract,
 uint64_t active,uint32_t dm_pml4,uint32_t dm_pdpt,uint64_t kernel_source,
 volatile pwl_root_clone_report_t *r)
{
 if (!pmap || !extract || !r) return 11;
 r->source=0;r->source_pa=0;r->direct_base=0;
 uint64_t kernel_pa=extract(pmap,kernel_source);r->kernel_source_pa=kernel_pa;
 uint64_t kernel_alias=0,source=0;
 if (pwl_x64_root_direct_address(dm_pml4,dm_pdpt,kernel_pa,&kernel_alias)!=PWL_OK ||
     kernel_alias!=kernel_source || extract(pmap,kernel_source+4095)!=kernel_pa+4095 ||
     pwl_x64_root_direct_address(dm_pml4,dm_pdpt,active,&source)!=PWL_OK)
  return 11;
 r->direct_base=source-active;r->source=source;
 uint64_t source_pa=extract(pmap,source);r->source_pa=source_pa;
 if (source_pa!=active || extract(pmap,source+4095)!=active+4095) return 3;
 return 0;
}
#endif
