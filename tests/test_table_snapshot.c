#include "pwl_table_snapshot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t source[5][512],storage[8][512];
static unsigned reads[5],mutate,fail;
static pwl_status_t read_page(void *ctx,uint64_t pa,uint64_t out[512])
{
    assert(ctx==source);
    assert(pa>=0x1000 && pa<=0x5000 && !(pa%4096));
    size_t i=(size_t)(pa/4096-1);
    if(fail && i==2)return PWL_ERR_IO;
    ++reads[i];memcpy(out,source[i],4096);
    if(mutate && i==1 && reads[i]>1)out[0]^=32;
    return PWL_OK;
}
int main(void)
{
    pwl_x64_table_page_t t[8];size_t used=99;
    for(size_t i=0;i<8;i++)t[i]=(pwl_x64_table_page_t){0,storage[i]};
    source[0][0]=0x2003;source[0][256]=0x2003; /* shared branch */
    source[1][0]=0x3003;source[1][1]=UINT64_C(0x40000081); /* 1G leaf */
    source[2][0]=0x4003;source[2][1]=UINT64_C(0x27a400081); /* 2M leaf */
    source[3][0]=UINT64_C(0x27a300001);
    assert(pwl_x64_table_snapshot_capture(0x1000,read_page,source,t,8,&used)==PWL_OK && used==4);
    for(unsigned i=0;i<4;i++)assert(reads[i]==2);
    pwl_x64_translation_t x;
    assert(pwl_x64_translate(t,used,0x1000,UINT64_C(0xffff800000000031),&x)==PWL_OK && x.physical_address==UINT64_C(0x27a300031));
    assert(pwl_x64_table_snapshot_capture(0x1000,read_page,source,t,3,&used)==PWL_ERR_BUFFER_TOO_SMALL && !used);
    memset(reads,0,sizeof(reads));mutate=1;
    assert(pwl_x64_table_snapshot_capture(0x1000,read_page,source,t,8,&used)==PWL_ERR_BAD_IMAGE && !used);
    mutate=0;fail=1;
    assert(pwl_x64_table_snapshot_capture(0x1000,read_page,source,t,8,&used)==PWL_ERR_IO && !used);
    fail=0;source[0][511]=0x1003; /* recursive slot: depth bounded by levels */
    assert(pwl_x64_table_snapshot_capture(0x1000,read_page,source,t,8,&used)==PWL_OK && used==4);
    source[0][511]=0;source[0][0]|=128;
    assert(pwl_x64_table_snapshot_capture(0x1000,read_page,source,t,8,&used)==PWL_ERR_BAD_IMAGE && !used);
    source[0][0]&=~UINT64_C(128);t[1].entries=t[0].entries;
    assert(pwl_x64_table_snapshot_capture(0x1000,read_page,source,t,8,&used)==PWL_ERR_INVALID_ARGUMENT);
    puts("full table snapshots: shared/recursive branches, large leaves, bounded reads and changed-page refusal passed");
}
