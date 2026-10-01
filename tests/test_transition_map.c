#include "pwl_transition_map.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define NX (UINT64_C(1)<<63)
static uint64_t old_pages[4][512],new_pages[4][512];
int main(void)
{
    pwl_x64_table_page_t a[4],b[4];
    for(unsigned i=0;i<4;i++) {
        a[i]=(pwl_x64_table_page_t){0x1000+i*4096,old_pages[i]};
        b[i]=(pwl_x64_table_page_t){0x9000+i*4096,new_pages[i]};
    }
    uint64_t va=UINT64_C(0xffff800000000000),pa=UINT64_C(0x240000000);
    old_pages[0][256]=0x2003;old_pages[1][0]=0x3003;old_pages[2][0]=pa|0x81;
    new_pages[0][256]=0xa003;new_pages[1][0]=0xb003;new_pages[2][0]=0xc003;
    new_pages[3][0]=pa|1;new_pages[3][1]=(pa+4096)|1;
    pwl_x64_translation_t out={0};
    assert(pwl_x64_translate(a,4,0x1000,va+123,&out)==PWL_OK);
    assert(out.physical_address==pa+123 && out.leaf_bytes==2097152 && out.executable && !out.writable && !out.user);
    pwl_transition_range_t r={va+4090,12,0,1};size_t failed=0;
    assert(pwl_x64_transition_mappings_validate(a,4,0x1000,b,4,0x9000,&r,1,&failed)==PWL_OK && failed==SIZE_MAX);
    new_pages[3][1]+=4096;
    assert(pwl_x64_transition_mappings_validate(a,4,0x1000,b,4,0x9000,&r,1,&failed)==PWL_ERR_BAD_IMAGE && failed==0);
    new_pages[3][1]-=4096;
    old_pages[0][256]|=NX;
    assert(pwl_x64_translate(a,4,0x1000,va,&out)==PWL_OK && !out.executable);
    assert(pwl_x64_transition_mappings_validate(a,4,0x1000,b,4,0x9000,&r,1,NULL)==PWL_ERR_BAD_IMAGE);
    old_pages[0][256]&=~NX;
    old_pages[2][0]|=4096; /* PAT, not a misaligned address. */
    assert(pwl_x64_translate(a,4,0x1000,va,&out)==PWL_OK && out.pat_index==4);
    assert(pwl_x64_transition_mappings_validate(a,4,0x1000,b,4,0x9000,&r,1,NULL)==PWL_ERR_BAD_IMAGE);
    old_pages[2][0]|=8192;
    pwl_x64_translation_t saved=out;
    assert(pwl_x64_translate(a,4,0x1000,va,&out)==PWL_ERR_BAD_IMAGE && !memcmp(&saved,&out,sizeof(out)));
    old_pages[1][0]=pa|0x81;
    assert(pwl_x64_translate(a,4,0x1000,va+0x12345,&out)==PWL_OK && out.leaf_bytes==1073741824 && out.physical_address==pa+0x12345);
    old_pages[0][256]|=128;
    assert(pwl_x64_translate(a,4,0x1000,va,&out)==PWL_ERR_BAD_IMAGE);
    old_pages[0][256]=0x2003;
    assert(pwl_x64_translate(a,4,0x1001,va,&out)==PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_x64_translate(a,4,0x1000,UINT64_C(1)<<47,&out)==PWL_ERR_INVALID_ARGUMENT);
    b[1].physical_address=b[0].physical_address;
    assert(pwl_x64_translate(b,4,0x9000,va,&out)==PWL_ERR_INVALID_ARGUMENT);
    b[1].physical_address=0xa000;
    old_pages[1][0]=0x3003;old_pages[2][0]=pa|0x81;
    r.virtual_address=UINT64_MAX-1;r.bytes=4;
    assert(pwl_x64_transition_mappings_validate(a,4,0x1000,b,4,0x9000,&r,1,NULL)==PWL_ERR_INVALID_ARGUMENT);
    r=(pwl_transition_range_t){va,8,1,1};
    assert(pwl_x64_transition_mappings_validate(a,4,0x1000,b,4,0x9000,&r,1,NULL)==PWL_ERR_INVALID_ARGUMENT);
    new_pages[3][0]|=NX;r=(pwl_transition_range_t){va,8,0,0};old_pages[2][0]|=NX;
    assert(pwl_x64_transition_mappings_validate(a,4,0x1000,b,4,0x9000,&r,1,NULL)==PWL_OK);
    puts("transition maps: high aliases, large leaves, inherited permissions, subpage comparison and refusal passed");
    return 0;
}
