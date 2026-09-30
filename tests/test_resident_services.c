#define _GNU_SOURCE
#include "resident_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static pwl_resident_image_t resident_image;

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
/* POSIX executable mappings permit conversion through memcpy without a C
 * object-pointer/function-pointer cast. Each callback uses the actual ms ABI.
 */
#define LOAD(type,name,index) type name; do { \
    void *entry=code+resident_image.callbacks[index]; \
    _Static_assert(sizeof(name)==sizeof(entry),"AMD64 pointers"); \
    memcpy(&name,&entry,sizeof(name)); } while(0)

static void run_copy(void)
{
    unsigned char *code=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(code!=MAP_FAILED && (uintptr_t)code>UINT32_MAX);
    pwl_resident_data_t d={0};
    pwl_phys_region_t region={UINT64_C(0x27a400000),65536,PWL_MEMORY_FREE};
    uint64_t cache=8,context=(uintptr_t)&d,pa=0,key=0;
    assert(pwl_fw_memory_init(&d.memory,&region,&cache,1,0x1234)==PWL_OK);
    d.tpl=4;
    memcpy(code,resident_bytes,sizeof(resident_bytes));
    memcpy(code+resident_image.binding_offset,&context,sizeof(context));
    assert(mprotect(code,4096,PROT_READ|PROT_EXEC)==0);
    LOAD(raise_fn,raise_tpl,0); LOAD(restore_fn,restore_tpl,1);
    LOAD(alloc_fn,allocate,2); LOAD(free_fn,release,3); LOAD(map_fn,map,4);
    LOAD(exit_fn,exit_boot,5); LOAD(crc_fn,crc,6); LOAD(copy_fn,copy,7); LOAD(set_fn,set,8);
    assert(raise_tpl(16)==4 && d.tpl==16);
    restore_tpl(4);assert(d.tpl==4);
    assert(raise_tpl(7)==4 && d.tpl==4);
    assert(allocate(PWL_ALLOCATE_ANY,2,2,&pa)==0 && pa==region.base);
    assert(allocate(PWL_ALLOCATE_ANY,2,0,&pa)==PWL_EFI_INVALID_PARAMETER);
    size_t size=0,ds=0;uint32_t version=0;
    assert(map(&size,NULL,&key,&ds,&version)==PWL_EFI_BUFFER_TOO_SMALL);
    assert(ds==40 && version==1 && size==80);
    pwl_efi_memory_descriptor_t descriptors[PWL_FW_MAX_DESCRIPTORS];
    size=sizeof(descriptors);
    assert(map(&size,descriptors,&key,&ds,&version)==0);
    assert(descriptors[0].physical_start==region.base && descriptors[0].type==2);
    assert(exit_boot(0x1234,key)==PWL_EFI_UNSUPPORTED && !d.memory.exited);
    assert(release(pa,2)==0);
    assert(release(pa,2)==PWL_EFI_NOT_FOUND);
    uint32_t result=0;
    assert(crc("123456789",9,&result)==0 && result==UINT32_C(0xcbf43926));
    assert(crc(NULL,9,&result)==PWL_EFI_INVALID_PARAMETER);
    char bytes[32]="123456789",expected[32]="123456789";
    copy(bytes+2,bytes,7);memmove(expected+2,expected,7);
    assert(memcmp(bytes,expected,sizeof(bytes))==0);
    copy(bytes,bytes+2,7);memmove(expected,expected+2,7);
    assert(memcmp(bytes,expected,sizeof(bytes))==0);
    set(bytes,sizeof(bytes),0xa5);
    for(size_t i=0;i<sizeof(bytes);i++)assert((unsigned char)bytes[i]==0xa5);
    assert(munmap(code,4096)==0);
}
int main(void)
{
    resident_image=resident_fixture();
    run_copy();run_copy();
    puts("resident callbacks: copied RX code above 4 GiB, Microsoft x64 ABI and nine services passed");
}
