#include "pwl_native_workspace.h"
#include "workspace_report.h"
#ifdef PWL_RESIDENT_PROBE
#include "resident_fixture.h"
static pwl_resident_image_t resident_image;
#endif
#ifndef PWL_WORKSPACE_DIAGNOSTIC
#error "This adapter is only for the guarded returning workspace experiment"
#endif
/* Isolated diagnostic adapter. ps4_binding.c is NOT replaced in production. */
static pwl_ps4_memory_api_t expected;
static int active;
static pwl_native_workspace_t workspace;
static unsigned char media[512];
static pwl_native_request_t request;
#ifndef PWL_RESIDENT_PROBE
static const unsigned char code[]={0x31,0xc0,0xc3}; /* synthetic, never executed */
#endif
pwl_status_t pwl_ps4_memory_api_validate(const pwl_ps4_memory_api_t *a) {
 if(!active || !a || a->firmware!=1352 ||
    a->kernel_map!=expected.kernel_map || a->kernel_pmap!=expected.kernel_pmap ||
    a->alloc_contig!=expected.alloc_contig || a->free!=expected.free ||
    a->extract!=expected.extract)return PWL_ERR_UNSUPPORTED;
 return PWL_OK;
}
int pwl_workspace_experiment(void *map,void *pmap,pwl_workspace_alloc_fn alloc,
 pwl_workspace_free_fn release,pwl_workspace_extract_fn extract,volatile pwl_workspace_report_t *r) {
 if(!r || active || workspace.arena.kernel_address || !map || !pmap || !alloc || !release || !extract)return -1;
 expected=(pwl_ps4_memory_api_t){map,pmap,alloc,release,extract,1352,NULL};active=1;
 for(size_t i=0;i<sizeof(media);i++)media[i]=(unsigned char)(i*37);
#ifdef PWL_RESIDENT_PROBE
 resident_image=resident_fixture();
 request.firmware=resident_image.bytes;request.firmware_bytes=resident_image.size;
#else
 request.firmware=code;request.firmware_bytes=sizeof(code);
#endif
 request.disk_image=media;request.disk_bytes=sizeof(media);
 request.heap_bytes=65536;request.stack_bytes=65536;request.table_pages=16;
 request.image_handle=0x1234;request.boot_image=NULL;request.boot_image_bytes=0;
 r->stage=2;r->table_status=PWL_ERR_UNSUPPORTED;r->release_status=PWL_ERR_UNSUPPORTED;
 r->efi_status=PWL_ERR_UNSUPPORTED;
#ifdef PWL_RESIDENT_PROBE
 pwl_status_t status=pwl_native_workspace_prepare_resident(&expected,&request,&resident_image,&workspace);
#else
 pwl_status_t status=pwl_native_workspace_prepare(&expected,&request,&workspace);
#endif
 r->prepare_status=status;
 if(status!=PWL_OK) {active=0;return status;}
 r->stage=3;r->kva=workspace.arena.kernel_address;r->pa=workspace.arena.physical_address;
 r->bytes=workspace.arena.size;r->root=workspace.tables[0].physical_address;
 r->tables=(unsigned int)workspace.table_count;r->regions=(unsigned int)workspace.region_count;
 r->table_status=pwl_x64_identity_mappings_validate(workspace.mappings,workspace.mapping_count,
                                                  workspace.tables,workspace.table_count);
 r->copy_ok=1;
 const unsigned char *c=workspace.firmware.prepare_address,*d=workspace.media.prepare_address;
#ifdef PWL_RESIDENT_PROBE
 for(size_t i=0;i<resident_image.size;i++)
  if(i<resident_image.binding_offset || i>=resident_image.binding_offset+8)
   if(c[i]!=resident_bytes[i])r->copy_ok=0;
 uint64_t binding=0;
 for(unsigned i=0;i<8;i++)binding|=(uint64_t)c[resident_image.binding_offset+i]<<(8*i);
 if(binding!=workspace.data.physical_address)r->copy_ok=0;
 const pwl_native_data_t *data=workspace.data.prepare_address;
 pwl_efi_table_spec_t spec={workspace.firmware.physical_address,resident_image.size,
  workspace.data.physical_address+offsetof(pwl_native_data_t,efi),sizeof(data->efi),{0}};
 for(size_t i=0;i<PWL_EFI_PREPARED_CALLBACKS;i++)spec.callback_offsets[i]=resident_image.callbacks[i];
 r->efi_status=pwl_efi_tables_validate(&spec,&data->efi);
 if(r->efi_status!=PWL_OK)status=PWL_ERR_INVALID_ARGUMENT;
#else
 for(size_t i=0;i<sizeof(code);i++)if(c[i]!=code[i])r->copy_ok=0;
#endif
 for(size_t i=0;i<sizeof(media);i++)if(d[i]!=media[i])r->copy_ok=0;
 if(r->table_status!=PWL_OK || !r->copy_ok)status=PWL_ERR_INVALID_ARGUMENT;
 r->stage=4;
 r->release_status=pwl_native_workspace_release(&workspace);
 if(r->release_status==PWL_OK)r->stage=5;
 else status=r->release_status;
 active=0;return status;
}
