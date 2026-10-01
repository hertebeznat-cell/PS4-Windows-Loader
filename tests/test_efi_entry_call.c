#define _GNU_SOURCE
#include "pwl_efi_entry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static uint64_t __attribute__((ms_abi,noinline)) fixture(uint64_t handle,uint64_t table)
{
    assert(handle==UINT64_C(0x123456789abcdef0));
    assert(table==UINT64_C(0x27a300010));
    return UINT64_C(0x800000000000000e);
}
int main(void)
{
    int (*original)(void *)=pwl_x64_efi_entry_call;
    uintptr_t start=0,target=0;uint64_t (*efi)(uint64_t,uint64_t) __attribute__((ms_abi))=fixture;
    _Static_assert(sizeof(original)==sizeof(start),"pointer sizes");
    memcpy(&start,&original,sizeof(start));memcpy(&target,&efi,sizeof(target));
    size_t bytes=(uintptr_t)pwl_x64_efi_entry_call_end-start;
    assert(bytes && bytes<4096);
    void *copy=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(copy!=MAP_FAILED);
    memcpy(copy,(const void *)start,bytes);assert(mprotect(copy,4096,PROT_READ|PROT_EXEC)==0);
    int (*call)(void *)=NULL;memcpy(&call,&copy,sizeof(call));
    pwl_efi_entry_context_t context={target,UINT64_C(0x123456789abcdef0),UINT64_C(0x27a300010),0,0};
    assert(call(&context)==0 && context.returned==1 && context.status==UINT64_C(0x800000000000000e));
    assert(context.entry==target && context.image_handle==UINT64_C(0x123456789abcdef0));
    assert(munmap(copy,4096)==0);
    puts("EFI entry: copied RX adapter, Microsoft AMD64 arguments and full 64-bit return passed");
    return 0;
}
