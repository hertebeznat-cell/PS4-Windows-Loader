#include "pwl_table_snapshot.h"
#define ADDR UINT64_C(0x000ffffffffff000)
static size_t find(const pwl_x64_table_page_t *t,size_t n,uint64_t pa)
{size_t i;for(i=0;i<n;i++)if(t[i].physical_address==pa)return i;return n;}
pwl_status_t pwl_x64_table_snapshot_capture(uint64_t root,pwl_table_read_fn read,
 void *context,pwl_x64_table_page_t *t,size_t capacity,size_t *out)
{
    unsigned char levels[PWL_SNAPSHOT_MAX_TABLES]={0};
    unsigned char processed[PWL_SNAPSHOT_MAX_TABLES]={0};
    uint64_t verify[512];size_t i,j,used=1;int progress;
    if(!out)return PWL_ERR_INVALID_ARGUMENT;
    *out=0;
    if(!read || !t || !capacity || capacity>PWL_SNAPSHOT_MAX_TABLES ||
       !root || (root&~ADDR))return PWL_ERR_INVALID_ARGUMENT;
    for(i=0;i<capacity;i++) {
        uintptr_t p=(uintptr_t)t[i].entries;
        if(!p || p%sizeof(uint64_t) || p>UINTPTR_MAX-4096)return PWL_ERR_INVALID_ARGUMENT;
        for(j=0;j<i;j++) {
            uintptr_t q=(uintptr_t)t[j].entries;
            if(p<q+4096 && q<p+4096)return PWL_ERR_INVALID_ARGUMENT;
        }
    }
    t[0].physical_address=root;levels[0]=1;
    pwl_status_t status=read(context,root,t[0].entries);
    if(status!=PWL_OK)return status;
    do {
        progress=0;
        for(i=0;i<used;i++) for(unsigned level=0;level<4;level++) {
            unsigned bit=1U<<level;
            if(!(levels[i]&bit) || (processed[i]&bit))continue;
            processed[i]|=(unsigned char)bit;progress=1;
            for(j=0;j<512;j++) {
                uint64_t e=t[i].entries[j];
                if(!(e&1))continue;
                if(level==0 && (e&128))return PWL_ERR_BAD_IMAGE;
                if(level==3)continue;
                if(e&128) {
                    unsigned shift=level==1 ? 30 : 21;
                    uint64_t mask=(UINT64_C(1)<<shift)-1;
                    if(e&ADDR&mask&~UINT64_C(4096))return PWL_ERR_BAD_IMAGE;
                    continue;
                }
                uint64_t pa=e&ADDR;
                if(!pa)return PWL_ERR_BAD_IMAGE;
                size_t next=find(t,used,pa);
                if(next==used) {
                    if(used==capacity)return PWL_ERR_BUFFER_TOO_SMALL;
                    t[next].physical_address=pa;
                    status=read(context,pa,t[next].entries);
                    if(status!=PWL_OK)return status;
                    ++used;
                }
                levels[next]|=(unsigned char)(1U<<(level+1));
            }
        }
    } while(progress);
    for(i=0;i<used;i++) {
        status=read(context,t[i].physical_address,verify);
        if(status!=PWL_OK)return status;
        for(j=0;j<512;j++)if(verify[j]!=t[i].entries[j])return PWL_ERR_BAD_IMAGE;
    }
    *out=used;return PWL_OK;
}
