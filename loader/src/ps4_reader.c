#include "pwl_ps4_reader.h"
pwl_status_t pwl_ps4_protected_read(void *context,uint64_t address,void *out,size_t bytes)
{
#if defined(__x86_64__)
    uint16_t cs;uint64_t flags,pcb,onfault,thread;uint32_t lo,hi;
    __asm__ volatile("mov %%cs,%0":"=r"(cs)::"memory");
    if(cs&3)return PWL_ERR_UNSUPPORTED;
    __asm__ volatile("pushfq; popq %0":"=r"(flags)::"memory");
    if(!(flags&0x200) || (flags&0x400))return PWL_ERR_ACCESS_DENIED;
    const pwl_ps4_reader_t *r=context;
    if(!r || !out || !bytes || bytes>256 || !r->bounce || bytes>r->capacity ||
       (uintptr_t)r->bounce>=(UINT64_C(1)<<47) ||
       r->capacity>(UINT64_C(1)<<47)-(uintptr_t)r->bounce ||
       address<UINT64_C(0xffff800000000000) || bytes>UINT64_MAX-address ||
       r->kernel_base<UINT64_C(0xffff800000000000) || r->kernel_base%16384 ||
       r->kernel_base>UINT64_MAX-0x2bd790)return PWL_ERR_INVALID_ARGUMENT;
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000082));
    if(((uint64_t)lo|((uint64_t)hi<<32))!=r->kernel_base+0x1c0)return PWL_ERR_ACCESS_DENIED;
    __asm__ volatile("movq %%gs:0,%0":"=r"(thread));
    if(thread<UINT64_C(0xffff800000000000) || thread%8 || thread>UINT64_MAX-0x12c)
        return PWL_ERR_BAD_IMAGE;
    if(*(volatile uint32_t *)(uintptr_t)(thread+0x128) ||
       *(volatile uint16_t *)(uintptr_t)(thread+0xfc))return PWL_ERR_ACCESS_DENIED;
    __asm__ volatile("movq %%gs:0x20,%0":"=r"(pcb));
    if(pcb<UINT64_C(0xffff800000000000) || pcb%8 || pcb>UINT64_MAX-0xd8)
        return PWL_ERR_BAD_IMAGE;
    /* GS supplies the current kernel PCB. This exact layout is the same one
     * used before/after the observed copyout REP instructions. Not a guessed
     * pointer from a table or a photographed address. */
    onfault=*(volatile uint64_t *)(uintptr_t)(pcb+0xd0);
    if(onfault)return PWL_ERR_ACCESS_DENIED;
    int (*copyout)(const void *,void *,size_t)=
        (int (*)(const void *,void *,size_t))(uintptr_t)(r->kernel_base+0x2bd6a0);
    int result=copyout((const void *)(uintptr_t)address,r->bounce,bytes);
    if(result || *(volatile uint64_t *)(uintptr_t)(pcb+0xd0))return PWL_ERR_IO;
    unsigned char *d=out;const unsigned char *s=r->bounce;
    for(size_t i=0;i<bytes;i++)d[i]=s[i];
    return PWL_OK;
#else
    (void)context;(void)address;(void)out;(void)bytes;return PWL_ERR_UNSUPPORTED;
#endif
}
