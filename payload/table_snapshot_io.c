#include "table_snapshot_io.h"
#define ADDR UINT64_C(0x000ffffffffff000)
pwl_status_t pwl_console_table_address(const pwl_console_table_reader_t *r,
 uint64_t pa,uint64_t *out)
{
    uint64_t va;
    if(!r || !r->pmap || !r->extract || !out || !pa || (pa&~ADDR) ||
       pwl_x64_root_direct_address(r->direct_pml4,r->direct_pdpt,pa,&va)!=PWL_OK ||
       va>UINT64_MAX-4095)return PWL_ERR_INVALID_ARGUMENT;
    if(r->extract(r->pmap,va)!=pa || r->extract(r->pmap,va+4095)!=pa+4095)
        return PWL_ERR_BAD_IMAGE;
    *out=va;return PWL_OK;
}
pwl_status_t pwl_console_table_reader_prepare(void *pmap,pwl_workspace_extract_fn extract,
 uint32_t pml4,uint32_t pdpt,uint64_t kernel_alias,const pwl_x64_cpu_state_t *cpu,
 pwl_console_table_reader_t *out)
{
    uint64_t resolved;
    if(!out || !pmap || !extract || !cpu ||
       kernel_alias<UINT64_C(0xffff800000000000) || kernel_alias%4096 ||
       kernel_alias>UINT64_MAX-4095 ||
       pwl_x64_cpu_state_validate(cpu,cpu->cr3)!=PWL_OK)return PWL_ERR_INVALID_ARGUMENT;
    pwl_console_table_reader_t r={pmap,extract,pml4,pdpt,cpu->cr3};
    uint64_t kernel_pa=extract(pmap,kernel_alias);
    pwl_status_t status=pwl_console_table_address(&r,kernel_pa,&resolved);
    if(status!=PWL_OK || resolved!=kernel_alias)return PWL_ERR_BAD_IMAGE;
    status=pwl_console_table_address(&r,cpu->cr3,&resolved);
    if(status!=PWL_OK)return status;
    *out=r;return PWL_OK;
}
#ifndef PWL_TABLE_READER_HOST_TEST
pwl_status_t pwl_console_table_read(void *context,uint64_t pa,uint64_t out[512])
{
    const pwl_console_table_reader_t *r=context;uint64_t root,va,after;uint16_t cs;
    if(!out || !r)return PWL_ERR_INVALID_ARGUMENT;
    __asm__ volatile("mov %%cs,%0":"=r"(cs));
    if(cs&3)return PWL_ERR_ACCESS_DENIED;
    __asm__ volatile("mov %%cr3,%0":"=r"(root));
    if(root!=r->expected_root)return PWL_ERR_BAD_IMAGE;
    pwl_status_t status=pwl_console_table_address(r,pa,&va);
    if(status!=PWL_OK)return status;
    const volatile uint64_t *source=(const volatile uint64_t *)(uintptr_t)va;
    for(size_t i=0;i<512;i++)out[i]=source[i];
    __asm__ volatile("mov %%cr3,%0":"=r"(root));
    if(root!=r->expected_root)return PWL_ERR_BAD_IMAGE;
    status=pwl_console_table_address(r,pa,&after);
    if(status!=PWL_OK || after!=va)return PWL_ERR_BAD_IMAGE;
    return PWL_OK;
}
pwl_status_t pwl_console_table_snapshot_capture(pwl_console_table_reader_t *reader,
 pwl_x64_table_page_t *tables,size_t capacity,size_t *used_out)
{
    if(!reader) {
        if(used_out)*used_out=0;
        return PWL_ERR_INVALID_ARGUMENT;
    }
    return pwl_x64_table_snapshot_capture(reader->expected_root,
        pwl_console_table_read,reader,tables,capacity,used_out);
}
#else
#if !__STDC_HOSTED__
#error Host table-reader fixture must never compile freestanding
#endif
#endif
