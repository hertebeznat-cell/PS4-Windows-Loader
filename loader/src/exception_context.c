#include "pwl_exception_context.h"
static uint64_t le(const unsigned char *p,unsigned n)
{ uint64_t v=0;for(unsigned i=0;i<n;i++)v|=(uint64_t)p[i]<<(8*i);return v; }
static int canonical(uint64_t a)
{ return a<(UINT64_C(1)<<47) || a>=UINT64_C(0xffff800000000000); }
pwl_status_t pwl_x64_tss_descriptor_decode(const void *gdt,size_t bytes,
    uint16_t selector,pwl_x64_tss_descriptor_t *out)
{
    size_t offset=selector&~7U;
    if(!gdt || !out || !offset || (selector&7) || offset>bytes || bytes-offset<16)
        return PWL_ERR_INVALID_ARGUMENT;
    const unsigned char *d=(const unsigned char *)gdt+offset;
    unsigned type=d[5]&15;
    if((d[5]&0x90)!=0x80 || (type!=9 && type!=11) ||
        (d[6]&0x60) || le(d+12,4))return PWL_ERR_BAD_IMAGE;
    pwl_x64_tss_descriptor_t result={0};
    result.base=le(d+2,3)|((uint64_t)d[7]<<24)|(le(d+8,4)<<32);
    uint64_t limit=le(d,2)|((uint64_t)(d[6]&15)<<16);
    if(d[6]&0x80)limit=(limit<<12)|4095;
    result.bytes=limit+1;result.busy=type==11;
    if(!result.base || !canonical(result.base) || result.bytes<104 ||
        result.bytes-1>UINT64_MAX-result.base || !canonical(result.base+result.bytes-1))
        return PWL_ERR_BAD_IMAGE;
    *out=result;return PWL_OK;
}
pwl_status_t pwl_x64_idt_gate_decode(const void *gdt,size_t gb,
    const void *idt,size_t ib,unsigned vector,pwl_x64_idt_gate_t *out)
{
    if(!gdt || !idt || !out || vector>255 || vector*16U>ib || ib-vector*16U<16)
        return PWL_ERR_INVALID_ARGUMENT;
    const unsigned char *d=(const unsigned char *)idt+vector*16U;
    unsigned type=d[5]&15;uint16_t selector=(uint16_t)le(d+2,2);
    size_t offset=selector&~7U;
    if(!(d[5]&0x80))return PWL_ERR_NOT_FOUND;
    if((d[5]&16) || (type!=14 && type!=15) || (d[4]&0xf8) || le(d+12,4) ||
        !offset || (selector&7) || offset>gb || gb-offset<8)return PWL_ERR_BAD_IMAGE;
    const unsigned char *code=(const unsigned char *)gdt+offset;
    if((code[5]&0x98)!=0x98 || !(code[6]&0x20) || (code[6]&0x40) ||
        ((code[5]>>5)&3)!=0)return PWL_ERR_BAD_IMAGE;
    pwl_x64_idt_gate_t result={0};
    result.entry=le(d,2)|(le(d+6,2)<<16)|(le(d+8,4)<<32);
    result.selector=selector;result.ist=d[4]&7;result.dpl=(d[5]>>5)&3;result.trap=type==15;
    if(!result.entry || !canonical(result.entry))return PWL_ERR_BAD_IMAGE;
    *out=result;return PWL_OK;
}
pwl_status_t pwl_x64_tss_stacks_decode(const void *tss,size_t bytes,pwl_x64_tss_stacks_t *out)
{
    if(!tss || !out || bytes<104)return PWL_ERR_INVALID_ARGUMENT;
    const unsigned char *d=tss;pwl_x64_tss_stacks_t result={0};
    for(unsigned i=0;i<3;i++) {
        result.rsp[i]=le(d+4+i*8,8);
        if(result.rsp[i] && !canonical(result.rsp[i]))return PWL_ERR_BAD_IMAGE;
    }
    for(unsigned i=0;i<7;i++) {
        result.ist[i]=le(d+36+i*8,8);
        if(result.ist[i] && !canonical(result.ist[i]))return PWL_ERR_BAD_IMAGE;
    }
    result.iomap_base=(uint16_t)le(d+102,2);
    *out=result;return PWL_OK;
}
