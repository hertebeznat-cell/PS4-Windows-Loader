#include "table_snapshot_io.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t base;
static unsigned mismatch,calls;
static unsigned long long extract(void *ctx,unsigned long long va)
{
    assert(ctx==&base);++calls;
    if(va<base || va-base>=(UINT64_C(32)<<30))return 0;
    return (va-base)+(mismatch && (va&4095)==4095 ? 4096 : 0);
}
int main(void)
{
    assert(pwl_x64_root_direct_address(439,412,4096,&base)==PWL_OK);
    base-=4096;
    pwl_console_table_reader_t r={0};
    pwl_x64_cpu_state_t cpu={UINT64_C(0x80010001),0xac5a000,0x20,0xd00};
    assert(pwl_console_table_reader_prepare(&base,extract,439,412,base+0xa4b4000,&cpu,&r)==PWL_OK);
    uint64_t address=1;
    assert(pwl_console_table_address(&r,UINT64_C(0x27a300000),&address)==PWL_OK && address==base+UINT64_C(0x27a300000));
    mismatch=1;uint64_t saved=address;
    assert(pwl_console_table_address(&r,0x1000,&address)==PWL_ERR_BAD_IMAGE && address==saved);
    mismatch=0;unsigned before=calls;
    assert(pwl_console_table_address(&r,UINT64_C(32)<<30,&address)==PWL_ERR_INVALID_ARGUMENT && calls==before);
    assert(pwl_console_table_address(&r,0x1001,&address)==PWL_ERR_INVALID_ARGUMENT);
    pwl_console_table_reader_t original=r;
    cpu.cr4|=UINT64_C(1)<<17;
    assert(pwl_console_table_reader_prepare(&base,extract,439,412,base+0xa4b4000,&cpu,&r)==PWL_ERR_INVALID_ARGUMENT && !memcmp(&r,&original,sizeof(r)));
    cpu.cr4=0x20;
    assert(pwl_console_table_reader_prepare(&base,extract,438,412,base+0xa4b4000,&cpu,&r)!=PWL_OK);
    puts("console table reader: dynamic alias, >4G, endpoint checks, invalid inputs and no guessed mapping passed");
}
