#include "pwl_native_call.h"
#include <string.h>
#define BIT(n) (UINT64_C(1)<<(n))
static void cpuid(unsigned leaf,unsigned sub,unsigned *a,unsigned *b,unsigned *c,unsigned *d)
{ __asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(leaf),"c"(sub)); }
pwl_status_t pwl_x64_fp_layout_read(pwl_fp_layout_t *out)
{
    if(!out)return PWL_ERR_INVALID_ARGUMENT;
    unsigned a,b,c,d,max;cpuid(0,0,&max,&b,&c,&d);
    if(max<1)return PWL_ERR_UNSUPPORTED;
    cpuid(1,0,&a,&b,&c,&d);
    if((d&((1U<<24)|(1U<<26)))!=((1U<<24)|(1U<<26)))return PWL_ERR_UNSUPPORTED;
    pwl_fp_layout_t result={512,3,PWL_FP_FXSAVE};
    if(c&(1U<<27)) {
        if(!(c&(1U<<26)) || max<13)return PWL_ERR_UNSUPPORTED;
        unsigned lo,hi;__asm__ volatile("xgetbv":"=a"(lo),"=d"(hi):"c"(0));
        result.mask=(uint64_t)lo|((uint64_t)hi<<32);
        cpuid(13,0,&a,&b,&c,&d);
        uint64_t supported=(uint64_t)a|((uint64_t)d<<32);
        if((result.mask&3)!=3 || (result.mask&~supported) || b<576 || b>PWL_FP_MAX_BYTES)
            return PWL_ERR_UNSUPPORTED;
        result.bytes=b;result.kind=PWL_FP_XSAVE;
    }
    *out=result;return PWL_OK;
}
pwl_status_t pwl_x64_native_cpu_validate(const pwl_x64_cpu_state_t *cpu,const pwl_fp_layout_t *fp)
{
    if(!cpu || !fp || pwl_x64_cpu_state_validate(cpu,cpu->cr3)!=PWL_OK ||
        (cpu->cr0&(BIT(1)|BIT(5)))!=(BIT(1)|BIT(5)) ||
        (cpu->cr0&(BIT(2)|BIT(29)|BIT(30))) || /* EM, NW, CD */
        !(cpu->cr4&BIT(9)) ||
        (cpu->cr4&(BIT(22)|BIT(23)|BIT(24))) || /* PKE, CET, PKS */
        (fp->mask!=3 && fp->mask!=7))return PWL_ERR_UNSUPPORTED;
    if(cpu->cr4&BIT(18)) {
        if(fp->kind!=PWL_FP_XSAVE || fp->bytes<576 || fp->bytes>PWL_FP_MAX_BYTES)
            return PWL_ERR_UNSUPPORTED;
    } else if(fp->kind!=PWL_FP_FXSAVE || fp->bytes!=512 || fp->mask!=3)
        return PWL_ERR_UNSUPPORTED;
    return PWL_OK;
}
static int extent(uint64_t base,uint64_t bytes)
{
    if(!base || !bytes || bytes-1>UINT64_MAX-base)return 0;
    uint64_t end=base+bytes-1;
    return (base<(UINT64_C(1)<<47) && end<(UINT64_C(1)<<47)) ||
        (base>=UINT64_C(0xffff800000000000) && end>=base);
}
static int overlap(uint64_t a,uint64_t n,uint64_t b,uint64_t m)
{ return a<=b ? b-a<n : a-b<m; }
static uint64_t code_address(int (*fn)(pwl_native_call_t *))
{ uint64_t a;_Static_assert(sizeof(fn)==sizeof(a),"function pointer");memcpy(&a,&fn,sizeof(a));return a; }
pwl_status_t pwl_native_call_prepare(const pwl_native_workspace_t *w,
 const pwl_resident_image_t *image,const pwl_native_transition_plan_t *plan,
 const pwl_x64_table_page_t *tables,const pwl_x64_cpu_state_t *cpu,
 const pwl_x64_table_page_t *before,size_t bn,const pwl_fp_layout_t *fp,
 const pwl_native_call_storage_t *storage,pwl_native_call_t **out)
{
    pwl_efi_entry_context_t entry;
    if(!out || !storage || pwl_x64_native_cpu_validate(cpu,fp)!=PWL_OK ||
       pwl_native_efi_entry_prepare(w,image,plan,tables,&entry)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    pwl_native_call_storage_t s=*storage;
    const pwl_native_data_t *data=w->data.prepare_address;
    if(data->image_mapping.root!=plan->root)return PWL_ERR_BAD_IMAGE;
    uint64_t va=(uintptr_t)s.control.prepare_address,pa=s.control.physical_address,bytes=s.control.size;
    uint64_t fp_offset=256,context_offset=fp_offset+((fp->bytes+63)&~UINT64_C(63));
    if(!extent(va,bytes) || !extent(pa,bytes) || (va&4095) || (pa&4095) || (bytes&4095) ||
       bytes>PWL_FP_MAX_BYTES+4096 || context_offset+sizeof(entry)>bytes ||
       !extent(s.old_stack_base,s.old_stack_bytes) || (s.old_stack_base&4095) ||
       (s.old_stack_bytes&4095) || s.old_stack_bytes<4096 || s.old_stack_bytes>1048576 ||
       overlap(pa,bytes,w->arena.physical_address,w->arena.size) ||
       overlap(pa,bytes,plan->root,plan->table_count*4096))return PWL_ERR_INVALID_ARGUMENT;
    uint64_t thunk=code_address(pwl_x64_native_call),adapter;
    int (*f)(void *)=pwl_x64_efi_entry_call;memcpy(&adapter,&f,sizeof(adapter));
    uint64_t thunk_bytes=(uintptr_t)pwl_x64_native_call_end-thunk;
    uint64_t adapter_bytes=(uintptr_t)pwl_x64_efi_entry_call_end-adapter;
    if(!thunk_bytes || thunk_bytes>8192 || !adapter_bytes || adapter_bytes>4096)
        return PWL_ERR_BAD_IMAGE;
    pwl_transition_range_t ranges[]={
        {thunk,thunk_bytes,0,1},{adapter,adapter_bytes,0,1},
        {va,bytes,1,0},{s.old_stack_base,s.old_stack_bytes,1,0},
        {w->stack.physical_address,w->stack.size,1,0}};
    for(size_t i=0;i<5;i++)for(size_t j=0;j<i;j++)
        if(overlap(ranges[i].virtual_address,ranges[i].bytes,ranges[j].virtual_address,ranges[j].bytes))
            return PWL_ERR_INVALID_ARGUMENT;
    pwl_status_t status=pwl_x64_transition_mappings_validate(before,bn,cpu->cr3,
        tables,plan->table_count,plan->root,ranges,5,NULL);
    if(status!=PWL_OK)return status;
    for(uint64_t offset=0;offset<bytes;offset+=4096) {
        pwl_x64_translation_t x;
        if(pwl_x64_translate(before,bn,cpu->cr3,va+offset,&x)!=PWL_OK ||
           x.physical_address!=pa+offset || x.pat_index)return PWL_ERR_BAD_IMAGE;
    }
    /* Disjoint VAs alone do not exclude destructive physical aliases. */
    for(size_t i=0;i<bn;i++)if(overlap(pa,bytes,before[i].physical_address,4096) ||
        overlap(plan->root,plan->table_count*4096,before[i].physical_address,4096))
        return PWL_ERR_INVALID_ARGUMENT;
    for(size_t i=0;i<5;i++)if(i!=2) {
        uint64_t offset=0;
        while(offset<ranges[i].bytes) {
            uint64_t a=ranges[i].virtual_address+offset;pwl_x64_translation_t x;
            if(pwl_x64_translate(before,bn,cpu->cr3,a,&x)!=PWL_OK)return PWL_ERR_BAD_IMAGE;
            uint64_t step=4096-(a&4095);if(step>ranges[i].bytes-offset)step=ranges[i].bytes-offset;
            if(overlap(pa,bytes,x.physical_address,step))return PWL_ERR_INVALID_ARGUMENT;
            offset+=step;
        }
    }
    /* The new stack cannot overwrite the suspended caller's stack through a
     * different alias. Code extents likewise cannot alias either stack. */
    for(size_t i=0;i<5;i++)for(size_t j=0;j<i;j++) {
        if(i==2 || j==2)continue;
        uint64_t aoff=0;
        while(aoff<ranges[i].bytes) {
            pwl_x64_translation_t a;uint64_t ava=ranges[i].virtual_address+aoff;
            if(pwl_x64_translate(before,bn,cpu->cr3,ava,&a)!=PWL_OK)return PWL_ERR_BAD_IMAGE;
            uint64_t an=4096-(ava&4095);if(an>ranges[i].bytes-aoff)an=ranges[i].bytes-aoff;
            uint64_t boff=0;
            while(boff<ranges[j].bytes) {
                pwl_x64_translation_t b;uint64_t bva=ranges[j].virtual_address+boff;
                if(pwl_x64_translate(before,bn,cpu->cr3,bva,&b)!=PWL_OK)return PWL_ERR_BAD_IMAGE;
                uint64_t n=4096-(bva&4095);if(n>ranges[j].bytes-boff)n=ranges[j].bytes-boff;
                if(overlap(a.physical_address,an,b.physical_address,n))return PWL_ERR_INVALID_ARGUMENT;
                boff+=n;
            }
            aoff+=an;
        }
    }
    pwl_native_call_t call={0};call.expected=*cpu;call.root=plan->root;
    call.stack_top=w->stack.physical_address+w->stack.size;
    call.callback=adapter;call.context=va+context_offset;
    call.fp_address=va+fp_offset;call.fp_bytes=fp->bytes;call.fp_mask=fp->mask;call.fp_kind=fp->kind;
    call.old_stack_base=s.old_stack_base;call.old_stack_bytes=s.old_stack_bytes;
    memcpy(s.control.prepare_address,&call,sizeof(call));
    memcpy((unsigned char *)s.control.prepare_address+context_offset,&entry,sizeof(entry));
    *out=s.control.prepare_address;return PWL_OK;
}
pwl_status_t pwl_native_call_result_validate(const pwl_native_call_t *c)
{
    if(!c || c->status || !c->returned || c->returned!=1 || c->fp_saved!=1 || c->fp_restored!=1 ||
       c->addresses.root_before!=c->expected.cr3 || c->addresses.root_entered!=c->root ||
       c->addresses.root_after!=c->expected.cr3 || !c->addresses.stack_before ||
       c->addresses.stack_after!=c->addresses.stack_before ||
       c->addresses.stack_entered!=c->stack_top ||
       c->cr0_entered!=(c->expected.cr0&~BIT(3)) ||
       c->cr4_entered!=(c->expected.cr4&~BIT(7)))return PWL_ERR_BAD_IMAGE;
    if(!c->fp_bytes || c->fp_bytes>PWL_FP_MAX_BYTES ||
       c->fp_address!=(uintptr_t)c+256 ||
       c->context!=(uintptr_t)c+256+((c->fp_bytes+63)&~UINT64_C(63)))return PWL_ERR_BAD_IMAGE;
    const pwl_efi_entry_context_t *e=(const void *)(uintptr_t)c->context;
    if(!e || e->returned!=1 || c->callback_status)return PWL_ERR_BAD_IMAGE;
    return PWL_OK;
}
