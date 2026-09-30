#include "pwl_resident_selftest.h"
#define EFI __attribute__((ms_abi))
typedef uint64_t (EFI *raise_fn)(uint64_t);
typedef void (EFI *restore_fn)(uint64_t);
typedef uint64_t (EFI *alloc_fn)(unsigned,unsigned,uint64_t,uint64_t *);
typedef uint64_t (EFI *free_fn)(uint64_t,uint64_t);
typedef uint64_t (EFI *map_fn)(size_t *,pwl_efi_memory_descriptor_t *,uint64_t *,size_t *,uint32_t *);
typedef uint64_t (EFI *exit_fn)(uint64_t,uint64_t);
typedef uint64_t (EFI *crc_fn)(const void *,size_t,uint32_t *);
typedef void (EFI *copy_fn)(void *,const void *,size_t);
typedef void (EFI *set_fn)(void *,size_t,unsigned char);
#define LOAD(type,name,index) type name; do { \
    void *entry=(unsigned char *)code+b->callbacks[index]; \
    _Static_assert(sizeof(name)==sizeof(entry),"AMD64 pointer ABI"); \
    for(size_t i=0;i<sizeof(name);i++) \
        ((unsigned char *)&name)[i]=((const unsigned char *)&entry)[i]; \
} while(0)
#define BEFORE(n) do { \
    r->last_call=(n); \
    if(checkpoint && checkpoint((n),context))return PWL_ERR_IO; \
} while(0)
#define PASSED(n) (r->passed_mask|=1U<<((n)-1))

pwl_status_t pwl_resident_calls_test(const pwl_resident_image_t *b,void *code,
    pwl_resident_data_t *d,pwl_resident_call_report_t *r,
    int (*checkpoint)(unsigned,void *),void *context)
{
    if(!b || !code || !d || !r || pwl_resident_image_validate(b)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    *r=(pwl_resident_call_report_t){0};
    /* Verify the executable copy, except its deliberate context binding. */
    const unsigned char *copy=code,*source=b->bytes;
    for(size_t i=0;i<b->size;i++)
        if(i<b->binding_offset || i>=b->binding_offset+8)
            if(copy[i]!=source[i])return PWL_ERR_BAD_IMAGE;
    uint64_t binding=0;
    for(unsigned i=0;i<8;i++)binding|=(uint64_t)copy[b->binding_offset+i]<<(8*i);
    if(binding!=(uint64_t)(uintptr_t)d)return PWL_ERR_INVALID_ARGUMENT;
    pwl_phys_region_t region={UINT64_C(0x100000000),65536,PWL_MEMORY_FREE};
    uint64_t cache=8,pa=0,key=0;
    if(pwl_fw_memory_init(&d->memory,&region,&cache,1,0x1234)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    d->tpl=4;
    LOAD(raise_fn,raise_tpl,0);LOAD(restore_fn,restore_tpl,1);
    LOAD(alloc_fn,allocate,2);LOAD(free_fn,release,3);LOAD(map_fn,map,4);
    LOAD(exit_fn,exit_boot,5);LOAD(crc_fn,crc,6);LOAD(copy_fn,copy_mem,7);LOAD(set_fn,set_mem,8);
    BEFORE(1);if(raise_tpl(16)!=4 || d->tpl!=16)return PWL_ERR_BAD_IMAGE;PASSED(1);
    BEFORE(2);restore_tpl(4);if(d->tpl!=4)return PWL_ERR_BAD_IMAGE;PASSED(2);
    BEFORE(3);
    if(allocate(PWL_ALLOCATE_ANY,2,2,&pa)!=0 || pa!=region.base)return PWL_ERR_BAD_IMAGE;
    PASSED(3);
    /* GetMemoryMap precedes FreePages so the changed key can also be checked. */
    BEFORE(5);
    size_t size=0,ds=0;uint32_t version=0;
    if(map(&size,NULL,&key,&ds,&version)!=PWL_EFI_BUFFER_TOO_SMALL || size!=80 || ds!=40 || version!=1)
        return PWL_ERR_BAD_IMAGE;
    pwl_efi_memory_descriptor_t descriptors[2];size=sizeof(descriptors);
    if(map(&size,descriptors,&key,&ds,&version)!=0 || size!=80 ||
       descriptors[0].type!=2 || descriptors[0].physical_start!=region.base ||
       descriptors[0].number_of_pages!=2 || descriptors[1].type!=7)
        return PWL_ERR_BAD_IMAGE;
    PASSED(5);
    BEFORE(4);if(release(pa,2)!=0 || d->memory.key==key)return PWL_ERR_BAD_IMAGE;PASSED(4);
    BEFORE(6);r->exit_status=exit_boot(0x1234,key);
    if(r->exit_status!=PWL_EFI_UNSUPPORTED || d->memory.exited)return PWL_ERR_BAD_IMAGE;
    PASSED(6);
    BEFORE(7);uint32_t result=0;
    if(crc("123456789",9,&result)!=0 || result!=UINT32_C(0xcbf43926))return PWL_ERR_BAD_IMAGE;
    PASSED(7);
    BEFORE(8);char bytes[16]="123456789";
    copy_mem(bytes+2,bytes,7);
    const char expected[]="121234567";
    for(size_t i=0;i<sizeof(expected);i++)if(bytes[i]!=expected[i])return PWL_ERR_BAD_IMAGE;
    copy_mem(bytes,bytes+2,7);
    for(size_t i=0;i<7;i++)if(bytes[i]!=(char)('1'+i))return PWL_ERR_BAD_IMAGE;
    PASSED(8);
    BEFORE(9);set_mem(bytes,sizeof(bytes),0xa5);
    for(size_t i=0;i<sizeof(bytes);i++)if((unsigned char)bytes[i]!=0xa5)return PWL_ERR_BAD_IMAGE;
    PASSED(9);
    return PWL_OK;
}
