#ifndef TEST_ACPI_FIXTURE_H
#define TEST_ACPI_FIXTURE_H
#include "pwl_acpi.h"
#include <assert.h>
#include <string.h>
#define ACPI_BASE UINT64_C(0x200000)
#define ACPI_HIGH UINT64_C(0x240000000)
typedef struct acpi_fixture {
    unsigned char low[40960],high[4096];
    pwl_acpi_extent_t extents[2];
    unsigned reads, fail_read, mutate_header;
    uint64_t permitted_end;
} acpi_fixture_t;
static void acpi_put32(unsigned char *p,uint32_t n)
{ for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i)); }
static void acpi_put64(unsigned char *p,uint64_t n)
{ for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(n>>(8*i)); }
static void acpi_sum(unsigned char *p,size_t bytes,size_t at)
{ p[at]=0;unsigned char sum=0;for(size_t i=0;i<bytes;i++)sum=(unsigned char)(sum+p[i]);p[at]=(unsigned char)(0-sum); }
static void acpi_sdt(unsigned char *p,const char *name,size_t bytes)
{ memset(p,0,bytes);memcpy(p,name,4);acpi_put32(p+4,(uint32_t)bytes);p[8]=3;memcpy(p+10,"TEST  ",6);acpi_sum(p,bytes,9); }
static void acpi_fixture_init(acpi_fixture_t *f,unsigned revision)
{
    memset(f,0,sizeof(*f));
    f->extents[0]=(pwl_acpi_extent_t){ACPI_BASE,sizeof(f->low)};
    f->extents[1]=(pwl_acpi_extent_t){ACPI_HIGH,sizeof(f->high)};
    unsigned char *r=f->low+16,*rsdt=f->low+4096,*xsdt=f->low+8192,*fadt=f->low+12288;
    memcpy(r,"RSD PTR ",8);memcpy(r+9,"TEST  ",6);r[15]=(unsigned char)revision;
    acpi_put32(r+16,(uint32_t)(ACPI_BASE+4096));
    if(revision==2) { acpi_put32(r+20,36);acpi_put64(r+24,ACPI_BASE+8192); }
    acpi_sum(r,20,8);if(revision==2)acpi_sum(r,36,32);
    acpi_sdt(rsdt,"RSDT",48);acpi_put32(rsdt+36,(uint32_t)(ACPI_BASE+12288));
    acpi_put32(rsdt+40,(uint32_t)(ACPI_BASE+28672));acpi_put32(rsdt+44,(uint32_t)(ACPI_BASE+32768));acpi_sum(rsdt,48,9);
    acpi_sdt(xsdt,"XSDT",60);acpi_put64(xsdt+36,ACPI_BASE+12288);
    acpi_put64(xsdt+44,ACPI_BASE+28672);acpi_put64(xsdt+52,ACPI_BASE+32768);acpi_sum(xsdt,60,9);
    size_t length=revision==2?276:116;
    acpi_sdt(fadt,"FACP",length);acpi_put32(fadt+36,(uint32_t)(ACPI_BASE+24576));
    acpi_put32(fadt+40,(uint32_t)(ACPI_BASE+20480));
    if(revision==2) { acpi_put64(fadt+132,ACPI_BASE+24576);acpi_put64(fadt+140,ACPI_HIGH); }
    acpi_sum(fadt,length,9);
    acpi_sdt(f->low+20480,"DSDT",72);acpi_sdt(f->high,"DSDT",96);
    unsigned char *facs=f->low+24576;memcpy(facs,"FACS",4);acpi_put32(facs+4,64);
    acpi_put32(facs+8,0x12345678);facs[32]=2;
    acpi_sdt(f->low+28672,"APIC",44);acpi_sdt(f->low+32768,"SSDT",52);
}
static pwl_status_t acpi_fixture_read(void *context,uint64_t address,void *buffer,size_t bytes)
{
    acpi_fixture_t *f=context;f->reads++;
    if(f->permitted_end)assert(address+bytes<=f->permitted_end);
    if(f->fail_read && f->reads==f->fail_read)return PWL_ERR_IO;
    const unsigned char *p=NULL;
    if(address>=ACPI_BASE && address-ACPI_BASE<=sizeof(f->low) && bytes<=sizeof(f->low)-(size_t)(address-ACPI_BASE))
        p=f->low+(size_t)(address-ACPI_BASE);
    if(address>=ACPI_HIGH && address-ACPI_HIGH<=sizeof(f->high) && bytes<=sizeof(f->high)-(size_t)(address-ACPI_HIGH))
        p=f->high+(size_t)(address-ACPI_HIGH);
    assert(p);memcpy(buffer,p,bytes);
    if(f->mutate_header && address==ACPI_BASE+12288 && bytes>36)((unsigned char *)buffer)[8]^=1;
    return PWL_OK;
}
static pwl_acpi_source_t acpi_fixture_source(acpi_fixture_t *f)
{ return (pwl_acpi_source_t){acpi_fixture_read,f,f->extents,2}; }
#endif
