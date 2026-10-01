#define _GNU_SOURCE
#include "pwl_native_call.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
int pwl_test_fp_roundtrip(void *,void *,uint64_t,uint64_t);
int main(void)
{
    /* Actual copied assembly executes on the host processor, without an
     * emulator. CPL3 must reject even an unreadable argument before CLI/CR. */
    int (*original)(pwl_native_call_t *)=pwl_x64_native_call;
    uintptr_t start;memcpy(&start,&original,sizeof(start));
    size_t length=(uintptr_t)pwl_x64_native_call_end-start;
    assert(length && length<8192);
    void *code=mmap(NULL,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    void *guard=mmap(NULL,4096,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(code!=MAP_FAILED && guard!=MAP_FAILED);
    memcpy(code,(const void *)start,length);assert(mprotect(code,8192,PROT_READ|PROT_EXEC)==0);
    int (*copied)(pwl_native_call_t *);memcpy(&copied,&code,sizeof(copied));
    assert(copied(guard)==PWL_ERR_ACCESS_DENIED);
    assert(copied(NULL)==PWL_ERR_ACCESS_DENIED);
    pwl_native_call_t record;memset(&record,0x55,sizeof(record));
    pwl_native_call_t previous=record;
    assert(copied(&record)==PWL_ERR_ACCESS_DENIED && !memcmp(&record,&previous,sizeof(record)));
    assert(munmap(code,8192)==0 && munmap(guard,4096)==0);

    pwl_x64_cpu_state_t cpu={UINT64_C(0x8005003b),0x1000,0x406f0,0xd01};
    pwl_fp_layout_t fp={832,7,PWL_FP_XSAVE};
    assert(pwl_x64_native_cpu_validate(&cpu,&fp)==PWL_OK); /* Original TS/PGE accepted. */
    for(unsigned bit=0;bit<32;bit++) {
        pwl_x64_cpu_state_t bad=cpu;
        if(bit==2 || bit==29 || bit==30)bad.cr0|=UINT64_C(1)<<bit;
        else if(bit==12 || bit==17 || bit==22 || bit==23 || bit==24)bad.cr4|=UINT64_C(1)<<bit;
        else continue;
        assert(pwl_x64_native_cpu_validate(&bad,&fp)!=PWL_OK);
    }
    cpu.cr4&=~UINT64_C(0x40000);
    assert(pwl_x64_native_cpu_validate(&cpu,&fp)!=PWL_OK);
    fp=(pwl_fp_layout_t){512,3,PWL_FP_FXSAVE};
    assert(pwl_x64_native_cpu_validate(&cpu,&fp)==PWL_OK);
    fp.mask=7;assert(pwl_x64_native_cpu_validate(&cpu,&fp)!=PWL_OK);

    /* Shared primitives save/restore real x87 control/data, MXCSR, XMM0/15
     * and enabled YMM state. The full host mask is used in XSAVE tests. */
    pwl_fp_layout_t live;
    assert(pwl_x64_fp_layout_read(&live)==PWL_OK);
    for(unsigned mode=0;mode<2;mode++) {
        pwl_fp_layout_t layout=mode ? live : (pwl_fp_layout_t){512,3,PWL_FP_FXSAVE};
        size_t bytes=(size_t)((layout.bytes+63)&~UINT64_C(63));
        void *saved=aligned_alloc(64,bytes),*old=aligned_alloc(64,bytes);
        assert(saved && old);memset(saved,0,bytes);memset(old,0,bytes);
        assert(pwl_test_fp_roundtrip(saved,old,layout.mask,layout.kind)==1);
        free(saved);free(old);
    }
    assert(pwl_x64_fp_layout_read(NULL)==PWL_ERR_INVALID_ARGUMENT);
    puts("native call: copied CPL3 refusal before reads; real FXSAVE/XSAVE x87/SSE/AVX roundtrip passed; no CPU switch tested");
    return 0;
}
