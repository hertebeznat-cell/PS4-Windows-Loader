#define _GNU_SOURCE
#include "resident_fixture.h"
#include "pwl_resident_selftest.h"
#ifdef PWL_TEST_STACK
#include "pwl_stack_call.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static pwl_resident_image_t resident_image;
static unsigned checkpoint_count,stop_at;
static int checkpoint(unsigned call,void *context) {
    assert(context==(void *)1);checkpoint_count++;
    return call==stop_at;
}
#ifdef PWL_TEST_STACK
static struct {
    void *code;
    pwl_resident_data_t *data;
    pwl_resident_call_report_t report;
    uintptr_t low,high,observed;
} stack_test;
static int stack_callback(void *context) {
    assert(context==&stack_test);
    volatile uint64_t marker=0;
    stack_test.observed=(uintptr_t)&marker;
    assert(stack_test.observed>=stack_test.low && stack_test.observed<stack_test.high);
    return pwl_resident_calls_test(&resident_image,stack_test.code,stack_test.data,
        &stack_test.report,NULL,NULL);
}
#endif

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
typedef uint64_t (EFI *pool_alloc_fn)(unsigned,size_t,void **);
typedef uint64_t (EFI *pool_free_fn)(void *);
typedef uint64_t (EFI *install_fn)(uint64_t *,const pwl_efi_guid_t *,unsigned,void *);
typedef uint64_t (EFI *replace_fn)(uint64_t,const pwl_efi_guid_t *,void *,void *);
typedef uint64_t (EFI *remove_fn)(uint64_t,const pwl_efi_guid_t *,void *);
typedef uint64_t (EFI *handle_fn)(uint64_t,const pwl_efi_guid_t *,void **);
typedef uint64_t (EFI *locate_fn)(const pwl_efi_guid_t *,void *,void **);
typedef uint64_t (EFI *handles_fn)(unsigned,const pwl_efi_guid_t *,void *,size_t *,uint64_t *);
/* POSIX executable mappings permit conversion through memcpy without a C
 * object-pointer/function-pointer cast. Each callback uses the actual ms ABI.
 */
#define LOAD(type,name,index) type name; do { \
    void *entry=code+resident_image.callbacks[index]; \
    _Static_assert(sizeof(name)==sizeof(entry),"AMD64 pointers"); \
    memcpy(&name,&entry,sizeof(name)); } while(0)

static void run_protocols(unsigned char *code,pwl_resident_data_t *d)
{
    LOAD(install_fn,install,11);LOAD(replace_fn,replace,12);LOAD(remove_fn,remove,13);
    LOAD(handle_fn,handle,14);LOAD(handles_fn,handles,15);LOAD(locate_fn,locate,16);
    pwl_efi_guid_t a={{1}},b={{2}},unknown={{3}};
    uint64_t first=0,second=0,list[64];void *interface=NULL;
    int one=1,two=2;
    assert(install(&first,&a,0,&one)==0 && first);
    assert(install(&first,&a,0,&two)==PWL_EFI_INVALID_PARAMETER);
    assert(install(&first,&b,0,&two)==0);
    assert(install(&second,&a,0,&two)==0 && second!=first);
    assert(handle(first,&a,&interface)==0 && interface==&one);
    assert(handle(first,&unknown,&interface)==PWL_EFI_UNSUPPORTED && interface==&one);
    assert(handle(UINT64_MAX,&a,&interface)==PWL_EFI_INVALID_PARAMETER);
    assert(locate(&a,NULL,&interface)==0 && interface==&one);
    size_t size=0;
    assert(handles(0,NULL,NULL,&size,NULL)==PWL_EFI_BUFFER_TOO_SMALL && size==16);
    assert(handles(0,NULL,NULL,&size,list)==0 && list[0]==first && list[1]==second);
    assert(handles(2,&a,NULL,&size,list)==0 && size==16);
    assert(handles(2,&unknown,NULL,&size,list)==PWL_EFI_NOT_FOUND);
    assert(handles(1,NULL,NULL,&size,list)==PWL_EFI_INVALID_PARAMETER);
    assert(handles(1,NULL,&one,&size,list)==PWL_EFI_UNSUPPORTED);
    assert(replace(first,&a,&two,&one)==PWL_EFI_NOT_FOUND);
    assert(replace(first,&a,&one,&two)==0);
    assert(handle(first,&a,&interface)==0 && interface==&two);
    assert(remove(first,&a,&one)==PWL_EFI_NOT_FOUND);
    assert(remove(first,&a,&two)==0);
    uint64_t third=0;
    assert(install(&third,&unknown,0,NULL)==0 && third!=first && third!=second);
    assert(handle(third,&unknown,&interface)==0 && !interface);
    assert(remove(first,&b,&two)==0 && remove(second,&a,&two)==0);
    assert(remove(third,&unknown,NULL)==0);
    uint64_t all[PWL_RESIDENT_PROTOCOLS];
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++) {
        all[i]=0;assert(install(&all[i],&a,0,&one)==0);
    }
    uint64_t overflow=0;
    assert(install(&overflow,&a,0,&one)==PWL_EFI_OUT_OF_RESOURCES && !overflow);
    d->memory.exited=1;
    assert(handle(all[0],&a,&interface)==PWL_EFI_ACCESS_DENIED);
    assert(remove(all[0],&a,&one)==PWL_EFI_ACCESS_DENIED);
    d->memory.exited=0;
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++) assert(remove(all[i],&a,&one)==0);
    d->protocol_next_handle=UINT64_MAX;
    assert(install(&overflow,&a,0,&one)==PWL_EFI_OUT_OF_RESOURCES && !overflow);
    assert(locate(&a,NULL,&interface)==PWL_EFI_NOT_FOUND);
}

static void run_copy(void)
{
    size_t code_bytes=(sizeof(resident_bytes)+4095U)&~(size_t)4095U;
    unsigned char *code=mmap(NULL,code_bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(code!=MAP_FAILED && (uintptr_t)code>UINT32_MAX);
    pwl_resident_data_t d={0};
    pwl_phys_region_t region={UINT64_C(0x27a400000),65536,PWL_MEMORY_FREE};
    uint64_t cache=8,context=(uintptr_t)&d,pa=0,key=0;
    assert(pwl_fw_memory_init(&d.memory,&region,&cache,1,0x1234)==PWL_OK);
    d.tpl=4;
    memcpy(code,resident_bytes,sizeof(resident_bytes));
    memcpy(code+resident_image.binding_offset,&context,sizeof(context));
    assert(mprotect(code,code_bytes,PROT_READ|PROT_EXEC)==0);
    LOAD(raise_fn,raise_tpl,0); LOAD(restore_fn,restore_tpl,1);
    LOAD(alloc_fn,allocate,2); LOAD(free_fn,release,3); LOAD(map_fn,map,4);
    LOAD(exit_fn,exit_boot,5); LOAD(crc_fn,crc,6); LOAD(copy_fn,copy,7); LOAD(set_fn,set,8);
    LOAD(pool_alloc_fn,pool_alloc,9); LOAD(pool_free_fn,pool_free,10);
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
    void *pool=NULL;
    uint64_t old_key=d.memory.key;
    assert(pool_alloc(2,4097,&pool)==0 && (uintptr_t)pool==region.base);
    assert(d.memory.key!=old_key && ((uintptr_t)pool&7)==0);
    assert(release((uintptr_t)pool,1)==PWL_EFI_INVALID_PARAMETER);
    assert(pool_free((unsigned char *)pool+8)==PWL_EFI_INVALID_PARAMETER);
    assert(pool_free(pool)==0 && pool_free(pool)==PWL_EFI_INVALID_PARAMETER);
    assert(pool_alloc(2,0,&pool)==0 && pool_free(pool)==0);
    assert(pool_alloc(2,1,NULL)==PWL_EFI_INVALID_PARAMETER);
    run_protocols(code,&d);
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
    pwl_resident_call_report_t report={0};
    assert(pwl_resident_calls_test(&resident_image,code,&d,&report,checkpoint,(void *)1)==PWL_OK);
    assert(report.passed_mask==0x1ff && report.last_call==9 && checkpoint_count==9);
    assert(report.exit_status==PWL_EFI_UNSUPPORTED);
    stop_at=7;checkpoint_count=0;
    assert(pwl_resident_calls_test(&resident_image,code,&d,&report,checkpoint,(void *)1)==PWL_ERR_IO);
    assert(report.passed_mask==0x3f && report.last_call==7 && checkpoint_count==7);
    stop_at=0;checkpoint_count=0;
    pwl_resident_image_t bad=resident_image;bad.crc32^=1;
    assert(pwl_resident_calls_test(&bad,code,&d,&report,checkpoint,(void *)1)==PWL_ERR_INVALID_ARGUMENT);
    assert(checkpoint_count==0);
#ifdef PWL_TEST_STACK
    size_t guard=16384,stack_bytes=1024*1024;
    unsigned char *stack=mmap(NULL,stack_bytes+2*guard,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(stack!=MAP_FAILED);
    assert(mprotect(stack+guard,stack_bytes,PROT_READ|PROT_WRITE)==0);
    stack_test.code=code;stack_test.data=&d;
    stack_test.low=(uintptr_t)(stack+guard);stack_test.high=stack_test.low+stack_bytes;
    pwl_stack_report_t restored={0};
    assert(pwl_stack_call((void *)stack_test.high,stack_callback,&stack_test,&restored)==PWL_OK);
    assert(stack_test.report.passed_mask==0x1ff && stack_test.report.last_call==9);
    assert(restored.before==restored.after && restored.entered==stack_test.high);
    assert(munmap(stack,stack_bytes+2*guard)==0);
#endif
    assert(munmap(code,code_bytes)==0);
}
int main(void)
{
    resident_image=resident_fixture();
    run_copy();run_copy();
    puts("resident callbacks: copied RX code above 4 GiB, Microsoft x64 ABI and nine services passed");
}
