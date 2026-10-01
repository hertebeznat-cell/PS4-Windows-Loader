#include "pwl_cpu_environment.h"
static unsigned privilege(void)
{uint16_t cs;__asm__ volatile("mov %%cs,%0":"=r"(cs)::"memory");return cs&3;}
static uint64_t msr(unsigned id)
{uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(id));return (uint64_t)lo|((uint64_t)hi<<32);}
pwl_status_t pwl_x64_cpu_environment_read(pwl_cpu_environment_t *out)
{
    if(privilege())return PWL_ERR_UNSUPPORTED;
    if(!out)return PWL_ERR_INVALID_ARGUMENT;
    pwl_cpu_environment_t e={0};
    __asm__ volatile("movq %%cr0,%0":"=r"(e.cpu.cr0));
    __asm__ volatile("movq %%cr3,%0":"=r"(e.cpu.cr3));
    __asm__ volatile("movq %%cr4,%0":"=r"(e.cpu.cr4));
    __asm__ volatile("pushfq;popq %0":"=r"(e.flags)::"memory");
    e.cpu.efer=msr(0xc0000080);e.fs_base=msr(0xc0000100);
    e.gs_base=msr(0xc0000101);e.kernel_gs_base=msr(0xc0000102);e.pat=msr(0x277);
    __asm__ volatile("sgdt %0;sidt %1":"=m"(e.gdt),"=m"(e.idt));
    __asm__ volatile("mov %%cs,%0;mov %%ss,%1;mov %%ds,%2;mov %%es,%3":"=r"(e.cs),"=r"(e.ss),"=r"(e.ds),"=r"(e.es));
    __asm__ volatile("mov %%fs,%0;mov %%gs,%1;str %2;sldt %3":"=r"(e.fs),"=r"(e.gs),"=r"(e.tr),"=r"(e.ldt));
    __asm__ volatile("movq %%gs:0,%0;movq %%gs:0x20,%1":"=r"(e.thread),"=r"(e.pcb));
    unsigned a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    e.apic_id=b>>24;e.apic_base=msr(0x1b);
    if((e.apic_base&0xc00)==0xc00)e.apic_id=(uint32_t)msr(0x802);
    if(d&(1U<<12)) {
        e.mtrr_cap=msr(0xfe);unsigned count=(unsigned)(e.mtrr_cap&255);
        if(count>16)return PWL_ERR_UNSUPPORTED;
        e.mtrr_default=msr(0x2ff);
        for(unsigned i=0;i<count*2;i++)e.mtrr_variable[i]=msr(0x200+i);
        if(e.mtrr_cap&0x100) {
            e.mtrr_fixed[0]=msr(0x250);e.mtrr_fixed[1]=msr(0x258);e.mtrr_fixed[2]=msr(0x259);
            for(unsigned i=0;i<8;i++)e.mtrr_fixed[i+3]=msr(0x268+i);
        }
    }
    pwl_status_t s=pwl_x64_fp_layout_read(&e.fp);if(s!=PWL_OK)return s;
    s=pwl_x64_native_cpu_validate(&e.cpu,&e.fp);if(s!=PWL_OK)return s;
    *out=e;return PWL_OK;
}
pwl_status_t pwl_cpu_environment_equal(const pwl_cpu_environment_t *a,const pwl_cpu_environment_t *b)
{
    if(!a || !b)return PWL_ERR_INVALID_ARGUMENT;
    const pwl_cpu_environment_t x=*a,y=*b;
    /* Compare members rather than padding bytes in supplied records. */
    if(x.cpu.cr0!=y.cpu.cr0 || x.cpu.cr3!=y.cpu.cr3 || x.cpu.cr4!=y.cpu.cr4 ||
       x.cpu.efer!=y.cpu.efer || (x.flags&0x600)!=(y.flags&0x600) ||
       x.fp.bytes!=y.fp.bytes || x.fp.mask!=y.fp.mask || x.fp.kind!=y.fp.kind ||
       x.gdt.base!=y.gdt.base || x.gdt.limit!=y.gdt.limit ||
       x.idt.base!=y.idt.base || x.idt.limit!=y.idt.limit ||
       x.fs_base!=y.fs_base || x.gs_base!=y.gs_base || x.kernel_gs_base!=y.kernel_gs_base ||
       x.pat!=y.pat || x.thread!=y.thread || x.pcb!=y.pcb || x.apic_id!=y.apic_id ||
       x.apic_base!=y.apic_base || x.mtrr_cap!=y.mtrr_cap || x.mtrr_default!=y.mtrr_default ||
       x.cs!=y.cs || x.ss!=y.ss || x.ds!=y.ds || x.es!=y.es ||
       x.fs!=y.fs || x.gs!=y.gs || x.tr!=y.tr || x.ldt!=y.ldt)return PWL_ERR_BAD_IMAGE;
    for(size_t i=0;i<32;i++)if(x.mtrr_variable[i]!=y.mtrr_variable[i])return PWL_ERR_BAD_IMAGE;
    for(size_t i=0;i<11;i++)if(x.mtrr_fixed[i]!=y.mtrr_fixed[i])return PWL_ERR_BAD_IMAGE;
    return PWL_OK;
}
pwl_status_t pwl_native_call_checked(pwl_native_call_t *call,pwl_cpu_environment_t *before,
    pwl_cpu_environment_t *after)
{
    if(privilege())return PWL_ERR_UNSUPPORTED;
    if(!call || !before || !after)return PWL_ERR_INVALID_ARGUMENT;
    uintptr_t starts[]={(uintptr_t)call,(uintptr_t)before,(uintptr_t)after};
    size_t lengths[]={sizeof(*call),sizeof(*before),sizeof(*after)};
    for(size_t i=0;i<3;i++) {
        if(starts[i]>UINTPTR_MAX-lengths[i])return PWL_ERR_INVALID_ARGUMENT;
        for(size_t j=0;j<i;j++)if(starts[i]<starts[j]+lengths[j] &&
            starts[j]<starts[i]+lengths[i])return PWL_ERR_INVALID_ARGUMENT;
    }
    pwl_status_t s=pwl_x64_cpu_environment_read(before);if(s!=PWL_OK)return s;
    if(call->expected.cr0!=before->cpu.cr0 || call->expected.cr3!=before->cpu.cr3 ||
       call->expected.cr4!=before->cpu.cr4 || call->expected.efer!=before->cpu.efer ||
       call->fp_kind!=before->fp.kind || call->fp_mask!=before->fp.mask ||
       call->fp_bytes!=before->fp.bytes)return PWL_ERR_ACCESS_DENIED;
    int result=pwl_x64_native_call(call);
    s=pwl_x64_cpu_environment_read(after);if(s!=PWL_OK)return s;
    s=pwl_cpu_environment_equal(before,after);if(s!=PWL_OK)return s;
    if(result)return (pwl_status_t)result;
    return pwl_native_call_result_validate(call);
}
