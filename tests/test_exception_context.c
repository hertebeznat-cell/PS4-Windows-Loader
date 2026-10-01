#include "pwl_exception_context.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void put(unsigned char *p,uint64_t v,unsigned n)
{for(unsigned i=0;i<n;i++)p[i]=(unsigned char)(v>>(8*i));}
int main(void)
{
    unsigned char gdt[40]={0},idt[4096]={0},tss[104]={0};
    gdt[8+5]=0x9b;gdt[8+6]=0x20;
    unsigned char *d=gdt+16;uint64_t base=UINT64_C(0xffff800012340000);
    put(d,103,2);put(d+2,base,3);d[7]=(unsigned char)(base>>24);put(d+8,base>>32,4);d[5]=0x8b;
    pwl_x64_tss_descriptor_t td={0};
    assert(pwl_x64_tss_descriptor_decode(gdt,sizeof(gdt),16,&td)==PWL_OK && td.base==base && td.bytes==104 && td.busy);
    pwl_x64_tss_descriptor_t old=td;d[12]=1;
    assert(pwl_x64_tss_descriptor_decode(gdt,sizeof(gdt),16,&td)==PWL_ERR_BAD_IMAGE && !memcmp(&old,&td,sizeof(td)));d[12]=0;
    assert(pwl_x64_tss_descriptor_decode(gdt,31,16,&td)==PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_x64_tss_descriptor_decode(gdt,sizeof(gdt),20,&td)==PWL_ERR_INVALID_ARGUMENT);
    d[5]=0x9b;assert(pwl_x64_tss_descriptor_decode(gdt,sizeof(gdt),16,&td)==PWL_ERR_BAD_IMAGE);d[5]=0x89;
    unsigned char *gate=idt+14*16;uint64_t entry=base+0x1234;
    put(gate,entry,2);put(gate+2,8,2);put(gate+6,entry>>16,2);put(gate+8,entry>>32,4);gate[4]=2;gate[5]=0x8e;
    pwl_x64_idt_gate_t ig={0};assert(pwl_x64_idt_gate_decode(gdt,sizeof(gdt),idt,sizeof(idt),14,&ig)==PWL_OK && ig.entry==entry && ig.ist==2 && !ig.trap);
    gate[5]=0x8f;assert(pwl_x64_idt_gate_decode(gdt,sizeof(gdt),idt,sizeof(idt),14,&ig)==PWL_OK && ig.trap);
    gate[4]=8;assert(pwl_x64_idt_gate_decode(gdt,sizeof(gdt),idt,sizeof(idt),14,&ig)==PWL_ERR_BAD_IMAGE);gate[4]=0;
    gdt[8+6]=0x60;assert(pwl_x64_idt_gate_decode(gdt,sizeof(gdt),idt,sizeof(idt),14,&ig)==PWL_ERR_BAD_IMAGE);gdt[8+6]=0x20;
    gate[5]=0x0e;assert(pwl_x64_idt_gate_decode(gdt,sizeof(gdt),idt,sizeof(idt),14,&ig)==PWL_ERR_NOT_FOUND);gate[5]=0x8e;
    assert(pwl_x64_idt_gate_decode(gdt,sizeof(gdt),idt,14*16+15,14,&ig)==PWL_ERR_INVALID_ARGUMENT);
    put(tss+4,base+4096,8);put(tss+36+8,base+8192,8);put(tss+102,104,2);
    pwl_x64_tss_stacks_t stacks={0};assert(pwl_x64_tss_stacks_decode(tss,sizeof(tss),&stacks)==PWL_OK && stacks.rsp[0]==base+4096 && stacks.ist[1]==base+8192 && stacks.iomap_base==104);
    put(tss+36,UINT64_C(1)<<47,8);pwl_x64_tss_stacks_t previous=stacks;
    assert(pwl_x64_tss_stacks_decode(tss,sizeof(tss),&stacks)==PWL_ERR_BAD_IMAGE && !memcmp(&stacks,&previous,sizeof(stacks)));
    puts("exception context: AMD64 GDT/TSS/IDT decoding, stacks, reserved fields and malformed input passed");
}
