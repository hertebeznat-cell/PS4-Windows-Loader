#include "pwl_alias_map.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t pages[16][512];
int main(void)
{
    pwl_x64_table_page_t t[16];size_t i,used=99;
    const uint64_t high=UINT64_C(0xffffff8000000000),pa=UINT64_C(0x27a300000);
    pwl_x64_alias_range_t r[]={
        {0x100000,pa,8192,0,1,0},
        {high,pa,8192,0,1,0},
        {high+0x4000,pa+0x4000,8192,1,0,4},
        {UINT64_C(0xfffffffffffff000),pa+0x8000,4096,0,0,7}
    };
    for(i=0;i<16;i++) t[i]=(pwl_x64_table_page_t){UINT64_C(0x300000000)+i*4096,pages[i]};
    assert(pwl_x64_alias_tables_build(r,4,t,16,&used)==PWL_OK);
    assert(used>=8 && used<=16);
    assert(pwl_x64_alias_tables_validate(r,4,t,used)==PWL_OK);
    pwl_x64_translation_t x;
    assert(pwl_x64_translate(t,used,t[0].physical_address,high+4113,&x)==PWL_OK);
    assert(x.physical_address==pa+4113 && x.executable && !x.writable && !x.user);
    assert(pwl_x64_translate(t,used,t[0].physical_address,high+0x6000,&x)!=PWL_OK);
    assert(pwl_x64_translate(t,used,t[0].physical_address,high+0x4000,&x)==PWL_OK && x.pat_index==4 && x.writable && !x.executable);
    assert(pwl_x64_translate(t,used,t[0].physical_address,UINT64_MAX,&x)==PWL_OK && x.pat_index==7);
    /* The entire mapping manifest is checked, not just required entry points. */
    uint64_t saved=pages[0][0]; pages[0][1]=saved;
    assert(pwl_x64_alias_tables_validate(r,4,t,used)==PWL_ERR_BAD_IMAGE);
    pages[0][1]=0; pages[0][0]|=32;
    assert(pwl_x64_alias_tables_validate(r,4,t,used)==PWL_ERR_BAD_IMAGE);
    pages[0][0]=saved;
    assert(pwl_x64_alias_tables_validate(r,4,t,used+1)==PWL_ERR_BAD_IMAGE);
    assert(pwl_x64_alias_tables_build(r,4,t,3,&used)==PWL_ERR_BUFFER_TOO_SMALL && used==0);
    r[1].writable=1;r[1].executable=0;
    assert(pwl_x64_alias_tables_build(r,4,t,16,&used)==PWL_ERR_INVALID_ARGUMENT && used==0);
    r[1].writable=0;r[1].executable=1;r[1].pat_index=1;
    assert(pwl_x64_alias_tables_build(r,4,t,16,&used)==PWL_ERR_INVALID_ARGUMENT);
    r[1].pat_index=0;r[2].virtual_address=high+4096;
    assert(pwl_x64_alias_tables_build(r,4,t,16,&used)==PWL_ERR_INVALID_ARGUMENT);
    r[2].virtual_address=high+0x4000;r[2].physical_address=UINT64_C(0x0010000000000000);
    assert(pwl_x64_alias_tables_build(r,4,t,16,&used)==PWL_ERR_INVALID_ARGUMENT);
    r[2].physical_address=pa+0x4000;r[3].bytes=8192;
    assert(pwl_x64_alias_tables_build(r,4,t,16,&used)==PWL_ERR_INVALID_ARGUMENT);
    r[3].bytes=4096;t[1].entries=t[0].entries;
    assert(pwl_x64_alias_tables_build(r,4,t,16,&used)==PWL_ERR_INVALID_ARGUMENT);
    t[1].entries=pages[1];t[1].physical_address=pa;
    assert(pwl_x64_alias_tables_build(r,4,t,16,&used)==PWL_ERR_INVALID_ARGUMENT);
    t[1].physical_address=UINT64_C(0x300001000);
    for(unsigned pat=0;pat<8;pat++) {
        r[2].pat_index=pat;
        assert(pwl_x64_alias_tables_build(r,4,t,16,&used)==PWL_OK);
        assert(pwl_x64_translate(t,used,t[0].physical_address,high+0x4000,&x)==PWL_OK && x.pat_index==pat);
    }
    /* Splitting a range across a 2 MiB branch must allocate a second PT. */
    pwl_x64_alias_range_t boundary={high+0x1ff000,pa+0x10000,8192,1,0,0};
    assert(pwl_x64_alias_tables_build(&boundary,1,t,16,&used)==PWL_OK && used==5);
    assert(pwl_x64_translate(t,used,t[0].physical_address,high+0x200011,&x)==PWL_OK && x.physical_address==pa+0x11011);
    pwl_transition_range_t bridge={boundary.virtual_address,boundary.bytes,1,0};
    assert(pwl_x64_transition_mappings_validate(t,used,t[0].physical_address,t,used,t[0].physical_address,&bridge,1,NULL)==PWL_OK);
    boundary.virtual_address=(UINT64_C(1)<<47)-4096;
    assert(pwl_x64_alias_tables_build(&boundary,1,t,16,&used)==PWL_ERR_INVALID_ARGUMENT);
    boundary.virtual_address=high;boundary.executable=1;
    assert(pwl_x64_alias_tables_build(&boundary,1,t,16,&used)==PWL_ERR_INVALID_ARGUMENT);
    puts("alias mappings: high/low, >4G, guards, PAT and refusal checks passed");
    return 0;
}
