#include "ps4.h"
#include "pwl_exception_context.h"
#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local"
#endif
struct table_register { uint16_t limit;uint64_t base; } __attribute__((packed));
static struct {
    unsigned stage,error;uint16_t cs,tr;uint64_t root,root_after,pat,gs,kernel_gs;
    struct table_register gdt,idt;
    pwl_x64_tss_descriptor_t tss;
    pwl_x64_tss_stacks_t stacks;
    pwl_x64_idt_gate_t gates[5];
    unsigned char gdt_copy[4096],idt_copy[4096],tss_copy[104];
} result;
static const unsigned vectors[]={2,8,13,14,18};
extern unsigned char _start[],__pwl_image_end[];
static int valid_span(uint64_t base,uint64_t bytes)
{
    return base>=UINT64_C(0xffff800000000000) && bytes && bytes-1<=UINT64_MAX-base;
}
static uint64_t msr(unsigned index)
{
    uint32_t lo,hi;__asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(index));
    return ((uint64_t)hi<<32)|lo;
}
static void copy_bytes(unsigned char *out,uint64_t address,size_t bytes)
{
    const volatile unsigned char *in=(const volatile unsigned char *)(uintptr_t)address;
    for(size_t i=0;i<bytes;i++)out[i]=in[i];
}
static int capture(struct thread *td,void *args)
{
    (void)td;(void)args;result.stage=1;
    __asm__ volatile("mov %0, cs":"=r"(result.cs));
    if(result.cs&3){result.error=1;return 0;}
    __asm__ volatile("sgdt %0; sidt %1; str %2":"=m"(result.gdt),"=m"(result.idt),"=m"(result.tr));
    __asm__ volatile("mov %0, cr3":"=r"(result.root));
    unsigned eax=1,ebx,ecx,edx;
    __asm__ volatile("cpuid":"+a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx));
    if(!(edx&(1U<<16))){result.error=2;return 0;}
    result.pat=msr(0x277);result.gs=msr(0xc0000101);result.kernel_gs=msr(0xc0000102);
    size_t gb=(size_t)result.gdt.limit+1,ib=(size_t)result.idt.limit+1;
    if(gb>sizeof(result.gdt_copy) || ib>sizeof(result.idt_copy) ||
        !valid_span(result.gdt.base,gb) || !valid_span(result.idt.base,ib)) {
        result.error=3;return 0;
    }
    result.stage=2;copy_bytes(result.gdt_copy,result.gdt.base,gb);copy_bytes(result.idt_copy,result.idt.base,ib);
    if(pwl_x64_tss_descriptor_decode(result.gdt_copy,gb,result.tr,&result.tss)!=PWL_OK ||
        !valid_span(result.tss.base,sizeof(result.tss_copy))){result.error=4;return 0;}
    result.stage=3;copy_bytes(result.tss_copy,result.tss.base,sizeof(result.tss_copy));
    if(pwl_x64_tss_stacks_decode(result.tss_copy,sizeof(result.tss_copy),&result.stacks)!=PWL_OK){result.error=5;return 0;}
    for(unsigned i=0;i<5;i++) {
        if(pwl_x64_idt_gate_decode(result.gdt_copy,gb,result.idt_copy,ib,vectors[i],&result.gates[i])!=PWL_OK ||
            result.gates[i].dpl || (result.gates[i].ist && !result.stacks.ist[result.gates[i].ist-1])) {
            result.error=10+vectors[i];return 0;
        }
    }
    struct table_register gdt,idt;uint16_t tr;
    __asm__ volatile("sgdt %0; sidt %1; str %2":"=m"(gdt),"=m"(idt),"=m"(tr));
    __asm__ volatile("mov %0, cr3":"=r"(result.root_after));
    if(gdt.base!=result.gdt.base || gdt.limit!=result.gdt.limit || idt.base!=result.idt.base ||
        idt.limit!=result.idt.limit || tr!=result.tr || result.root_after!=result.root){result.error=6;return 0;}
    result.stage=4;return 0;
}
int _main(void)
{
    initKernel();initLibc();
    printf_notification("PS4WL Context: entered %s",PS4WL_BUILD_ID);
    if(get_firmware()!=1352 || !is_jailbroken()) {
        printf_notification("PS4WL Context: environment check failed; stopped");return 1;
    }
    uintptr_t image_start,image_end;
    __asm__ volatile("lea %0, _start[rip]; lea %1, __pwl_image_end[rip]":"=r"(image_start),"=r"(image_end));
    size_t bytes=(size_t)(image_end-image_start);
    if(!bytes || mlock((void *)image_start,bytes)!=0){printf_notification("PS4WL Context: memory lock failed; stopped");return 1;}
    int rc=kexec(capture,NULL);int unlock=munlock((void *)image_start,bytes);
    printf_notification("PS4WL Context: rc=%d stage=%u error=%u unlock=%d",rc,result.stage,result.error,unlock);
    if(result.error || result.stage!=4 || rc)return 1;
    printf_notification("PS4WL Context: roots=%llx/%llx PAT=%llx",result.root,result.root_after,result.pat);
    printf_notification("PS4WL Context: GDT=%llx bytes=%u TR=%x",result.gdt.base,(unsigned)result.gdt.limit+1,result.tr);
    printf_notification("PS4WL Context: IDT=%llx bytes=%u",result.idt.base,(unsigned)result.idt.limit+1);
    printf_notification("PS4WL Context: TSS=%llx bytes=%llu busy=%u",result.tss.base,result.tss.bytes,result.tss.busy);
    printf_notification("PS4WL Context: GS=%llx KGS=%llx",result.gs,result.kernel_gs);
    for(unsigned i=0;i<3;i++)printf_notification("PS4WL Context: RSP%u=%llx",i,result.stacks.rsp[i]);
    for(unsigned i=0;i<7;i++)printf_notification("PS4WL Context: IST%u=%llx",i+1,result.stacks.ist[i]);
    for(unsigned i=0;i<5;i++)printf_notification("PS4WL Context: vector=%u entry=%llx CS=%x IST=%u",vectors[i],result.gates[i].entry,result.gates[i].selector,result.gates[i].ist);
    printf_notification("PS4WL Context: observation complete; Windows not called");
    return unlock ? 1 : 0;
}
