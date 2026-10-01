#include "pwl_alias_map.h"
#define ADDR UINT64_C(0x000ffffffffff000)
#define NX (UINT64_C(1)<<63)
#define LIMIT UINT64_C(0x0010000000000000)
#define MAX_TABLES 4096U
static int canonical(uint64_t a) { return a < (UINT64_C(1)<<47) || a >= UINT64_C(0xffff800000000000); }
static uint64_t flags(const pwl_x64_alias_range_t *r)
{
    return 1 | (r->writable ? 2 : 0) | (r->executable ? 0 : NX) |
        ((r->pat_index & 1U) ? 8 : 0) | ((r->pat_index & 2U) ? 16 : 0) |
        ((r->pat_index & 4U) ? 128 : 0);
}
static int inputs(const pwl_x64_alias_range_t *r,size_t n,
                  const pwl_x64_table_page_t *t,size_t c)
{
    size_t i,j;
    if (!r || !n || n>4096 || !t || !c || c>MAX_TABLES) return 0;
    for(i=0;i<n;i++) {
        uint64_t v=r[i].virtual_address,p=r[i].physical_address,b=r[i].bytes;
        if (!b || (v|p|b)%4096 || !canonical(v) || b-1>UINT64_MAX-v ||
            !canonical(v+b-1) || ((v>>47)&1)!=(((v+b-1)>>47)&1) ||
            p>=LIMIT || b>LIMIT-p || r[i].writable>1 || r[i].executable>1 ||
            (r[i].writable && r[i].executable) || r[i].pat_index>7 ||
            (i && (r[i-1].virtual_address > v ||
                   r[i-1].bytes-1 >= v-r[i-1].virtual_address))) return 0;
        for(j=0;j<i;j++) {
            uint64_t q=r[j].physical_address;
            if(p < q+r[j].bytes && q < p+b &&
               (r[i].pat_index!=r[j].pat_index ||
                (r[i].writable && r[j].executable) ||
                (r[j].writable && r[i].executable))) return 0;
        }
    }
    for(i=0;i<c;i++) {
        uintptr_t v=(uintptr_t)t[i].entries;
        if(!v || v%sizeof(uint64_t) || v>UINTPTR_MAX-4096 ||
           !t[i].physical_address || t[i].physical_address%4096 ||
           t[i].physical_address>=LIMIT) return 0;
        for(j=0;j<i;j++) {
            uintptr_t q=(uintptr_t)t[j].entries;
            if(t[i].physical_address==t[j].physical_address ||
               (v<q+4096 && q<v+4096)) return 0;
        }
        /* A table frame must never be exposed through an executable alias. */
        for(j=0;j<n;j++) if(r[j].executable &&
            t[i].physical_address>=r[j].physical_address &&
            t[i].physical_address-r[j].physical_address<r[j].bytes) return 0;
    }
    return 1;
}
static size_t find(const pwl_x64_table_page_t *t,size_t n,uint64_t pa)
{ size_t i;for(i=0;i<n;i++) if(t[i].physical_address==pa) return i;return n; }
static int tree(const pwl_x64_alias_range_t *r,size_t n,
 const pwl_x64_table_page_t *t,size_t c,size_t idx,unsigned level,
 uint64_t prefix,unsigned char *seen,size_t *visited)
{
    static const unsigned shifts[]={39,30,21,12};size_t i,j;
    if(seen[idx]) return 0;
    seen[idx]=1; ++*visited;
    for(i=0;i<512;i++) {
        uint64_t e=t[idx].entries[i],v=prefix|((uint64_t)i<<shifts[level]);
        if(!e) continue;
        if(level<3) {
            size_t next=find(t,c,e&ADDR);
            if((e&~ADDR)!=3 || next==c ||
               !tree(r,n,t,c,next,level+1,v,seen,visited)) return 0;
        } else {
            int found=0;
            if(v&(UINT64_C(1)<<47)) v|=UINT64_C(0xffff000000000000);
            for(j=0;j<n;j++) if(v>=r[j].virtual_address &&
                v-r[j].virtual_address<r[j].bytes) {
                if(e!=((r[j].physical_address+v-r[j].virtual_address)|flags(&r[j]))) return 0;
                found=1;break;
            }
            if(!found) return 0;
        }
    }
    return 1;
}
pwl_status_t pwl_x64_alias_tables_validate(const pwl_x64_alias_range_t *r,
 size_t n,const pwl_x64_table_page_t *t,size_t c)
{
    unsigned char seen[MAX_TABLES]={0};size_t visited=0,i;uint64_t o;
    if(!inputs(r,n,t,c)) return PWL_ERR_INVALID_ARGUMENT;
    if(!tree(r,n,t,c,0,0,0,seen,&visited) || visited!=c) return PWL_ERR_BAD_IMAGE;
    for(i=0;i<n;i++) for(o=0;o<r[i].bytes;o+=4096) {
        pwl_x64_translation_t x;
        if(pwl_x64_translate(t,c,t[0].physical_address,r[i].virtual_address+o,&x)!=PWL_OK ||
           x.physical_address!=r[i].physical_address+o || x.writable!=r[i].writable ||
           x.executable!=r[i].executable || x.user || x.pat_index!=r[i].pat_index)
            return PWL_ERR_BAD_IMAGE;
    }
    return PWL_OK;
}
pwl_status_t pwl_x64_alias_tables_build(const pwl_x64_alias_range_t *r,size_t n,
 pwl_x64_table_page_t *t,size_t c,size_t *out)
{
    static const unsigned shifts[]={39,30,21};size_t i,j,used=1;uint64_t o;
    if(!out) return PWL_ERR_INVALID_ARGUMENT;
    *out=0;
    if(!inputs(r,n,t,c)) return PWL_ERR_INVALID_ARGUMENT;
    for(i=0;i<c;i++) for(j=0;j<512;j++) t[i].entries[j]=0;
    for(i=0;i<n;i++) for(o=0;o<r[i].bytes;o+=4096) {
        uint64_t v=r[i].virtual_address+o;size_t idx=0;unsigned l;
        for(l=0;l<3;l++) {
            uint64_t *e=&t[idx].entries[(v>>shifts[l])&511];
            if(!*e) {
                if(used==c) return PWL_ERR_BUFFER_TOO_SMALL;
                *e=t[used].physical_address|3;idx=used++;
            } else idx=find(t,used,*e&ADDR);
        }
        t[idx].entries[(v>>12)&511]=(r[i].physical_address+o)|flags(&r[i]);
    }
    if(pwl_x64_alias_tables_validate(r,n,t,used)!=PWL_OK) return PWL_ERR_BAD_IMAGE;
    *out=used;return PWL_OK;
}
