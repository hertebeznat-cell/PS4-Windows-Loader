#include "pwl_transition_map.h"
#define ADDR UINT64_C(0x000ffffffffff000)
#define NX (UINT64_C(1)<<63)
static int canonical(uint64_t a)
{ return a<(UINT64_C(1)<<47) || a>=UINT64_C(0xffff800000000000); }
static int inputs(const pwl_x64_table_page_t *t,size_t n,uint64_t root)
{
    if(!t || !n || n>PWL_X64_MAX_IDENTITY_TABLES || !root || (root&~ADDR))return 0;
    unsigned found=0;
    for(size_t i=0;i<n;i++) {
        if(!t[i].entries || !t[i].physical_address || (t[i].physical_address&~ADDR))return 0;
        if(t[i].physical_address==root)found=1;
        for(size_t j=0;j<i;j++)if(t[i].physical_address==t[j].physical_address)return 0;
    }
    return found;
}
static pwl_status_t translate(const pwl_x64_table_page_t *t,size_t n,
    uint64_t root,uint64_t va,pwl_x64_translation_t *out)
{
    const unsigned shifts[]={39,30,21,12};uint64_t next=root;
    pwl_x64_translation_t result={0,0,1,1,1,0};
    for(unsigned level=0;level<4;level++) {
        const uint64_t *page=NULL;
        for(size_t i=0;i<n;i++)if(t[i].physical_address==next)page=t[i].entries;
        if(!page)return PWL_ERR_NOT_FOUND;
        uint64_t e=page[(va>>shifts[level])&511];
        if(!(e&1))return PWL_ERR_NOT_FOUND;
        result.writable&=(e&2)!=0;result.user&=(e&4)!=0;
        result.executable&=(e&NX)==0;
        if(level==0 && (e&128))return PWL_ERR_BAD_IMAGE;
        if(level==3 || (e&128)) {
            uint64_t size=UINT64_C(1)<<shifts[level];
            /* Large-leaf PAT occupies bit 12, not an address bit. */
            if(level<3 && (e&ADDR&(size-1)&~UINT64_C(4096)))return PWL_ERR_BAD_IMAGE;
            result.physical_address=(e&ADDR&~(size-1))+(va&(size-1));
            result.leaf_bytes=size;
            result.pat_index=(unsigned)((e>>3)&3) |
                (unsigned)(((e>>(level==3 ? 7 : 12))&1)<<2);
            *out=result;return PWL_OK;
        }
        next=e&ADDR;
    }
    return PWL_ERR_BAD_IMAGE;
}
pwl_status_t pwl_x64_translate(const pwl_x64_table_page_t *t,size_t n,
    uint64_t root,uint64_t va,pwl_x64_translation_t *out)
{
    if(!out || !canonical(va) || !inputs(t,n,root))return PWL_ERR_INVALID_ARGUMENT;
    return translate(t,n,root,va,out);
}
pwl_status_t pwl_x64_transition_mappings_validate(
    const pwl_x64_table_page_t *before,size_t bn,uint64_t br,
    const pwl_x64_table_page_t *after,size_t an,uint64_t ar,
    const pwl_transition_range_t *ranges,size_t count,size_t *failed)
{
    if(failed)*failed=SIZE_MAX;
    if(!ranges || !count || count>4096 || !inputs(before,bn,br) || !inputs(after,an,ar))
        return PWL_ERR_INVALID_ARGUMENT;
    for(size_t i=0;i<count;i++) {
        const pwl_transition_range_t *r=&ranges[i];
        if(failed)*failed=i;
        if(!r->bytes || !canonical(r->virtual_address) ||
            r->bytes-1>UINT64_MAX-r->virtual_address ||
            !canonical(r->virtual_address+r->bytes-1) ||
            ((r->virtual_address>>47)&1)!=(((r->virtual_address+r->bytes-1)>>47)&1) ||
            r->writable>1 || r->executable>1 || (r->writable && r->executable))
            return PWL_ERR_INVALID_ARGUMENT;
        uint64_t offset=0;
        while(offset<r->bytes) {
            uint64_t va=r->virtual_address+offset;
            pwl_x64_translation_t a,b;
            if(translate(before,bn,br,va,&a)!=PWL_OK || translate(after,an,ar,va,&b)!=PWL_OK ||
                a.physical_address!=b.physical_address || a.user || b.user ||
                a.writable!=r->writable || b.writable!=r->writable ||
                a.executable!=r->executable || b.executable!=r->executable ||
                a.pat_index!=b.pat_index)return PWL_ERR_BAD_IMAGE;
            /* Bound each comparison to a hardware 4 KiB subpage, even if the
             * two snapshots use different leaf sizes. Linear PA then agrees. */
            uint64_t step=4096-(va&4095);
            if(step>r->bytes-offset)step=r->bytes-offset;
            offset+=step;
        }
    }
    if(failed)*failed=SIZE_MAX;
    return PWL_OK;
}
