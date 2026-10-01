#include "pwl_acpi.h"
#define LIMIT (UINT64_C(1)<<47)
static uint32_t u32(const unsigned char *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint64_t u64(const unsigned char *p)
{ return u32(p)|((uint64_t)u32(p+4)<<32); }
static int equal(const unsigned char *a,const unsigned char *b,size_t n)
{ for(size_t i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1; }
static int signature(const unsigned char *p,const char *s,size_t n)
{ return equal(p,(const unsigned char *)s,n); }
static int checksum(const unsigned char *p,size_t n)
{ unsigned char sum=0;for(size_t i=0;i<n;i++)sum=(unsigned char)(sum+p[i]);return !sum; }
static int source_valid(const pwl_acpi_source_t *s)
{
    if(!s || !s->read || !s->extents || !s->extent_count || s->extent_count>256)return 0;
    for(size_t i=0;i<s->extent_count;i++) {
        uint64_t a=s->extents[i].address,n=s->extents[i].bytes;
        if(!a || a>=LIMIT || !n || n>LIMIT-a ||
           (i && s->extents[i-1].address+s->extents[i-1].bytes>a))return 0;
    }
    return 1;
}
static pwl_status_t read_bytes(const pwl_acpi_source_t *s,uint64_t pa,void *out,size_t n)
{
    if(!pa || !n || pa>=LIMIT || n>LIMIT-pa)return PWL_ERR_BAD_IMAGE;
    uint64_t cursor=pa,end=pa+n;
    for(size_t i=0;i<s->extent_count && cursor<end;i++) {
        uint64_t a=s->extents[i].address,b=a+s->extents[i].bytes;
        if(cursor<a)break;
        if(cursor<b)cursor=b<end?b:end;
    }
    if(cursor!=end)return PWL_ERR_ACCESS_DENIED;
    return s->read(s->context,pa,out,n);
}
static pwl_status_t rsdp_read(const pwl_acpi_source_t *s,uint64_t pa,
    unsigned char out[36],size_t *bytes)
{
    pwl_status_t status=read_bytes(s,pa,out,20);
    if(status!=PWL_OK)return status;
    if(!signature(out,"RSD PTR ",8) || !checksum(out,20))return PWL_ERR_BAD_IMAGE;
    if(out[15]==0) { *bytes=20;return PWL_OK; }
    if(out[15]!=2)return PWL_ERR_UNSUPPORTED;
    unsigned char first[20];for(size_t i=0;i<20;i++)first[i]=out[i];
    status=read_bytes(s,pa,out,36);
    if(status!=PWL_OK)return status;
    if(!equal(first,out,20) || u32(out+20)!=36 || !checksum(out,36))return PWL_ERR_BAD_IMAGE;
    *bytes=36;return PWL_OK;
}
pwl_status_t pwl_acpi_find_rsdp(const pwl_acpi_source_t *s,uint64_t start,
    uint64_t bytes,uint64_t *out)
{
    if(!out || !source_valid(s) || !start || start>=LIMIT || bytes<20 ||
       bytes>2U*1024U*1024U || bytes>LIMIT-start)return PWL_ERR_INVALID_ARGUMENT;
    uint64_t p=(start+15)&~UINT64_C(15),end=start+bytes;
    for(;p<=end && end-p>=20;p+=16) {
        unsigned char candidate[36];size_t n;
        pwl_status_t first=read_bytes(s,p,candidate,20);
        if(first!=PWL_OK)return first;
        if(!signature(candidate,"RSD PTR ",8) || !checksum(candidate,20) ||
           (candidate[15]!=0 && end-p<36))continue;
        pwl_status_t status=rsdp_read(s,p,candidate,&n);
        if(status==PWL_OK && n<=end-p) { *out=p;return PWL_OK; }
        if(status!=PWL_OK && status!=PWL_ERR_BAD_IMAGE && status!=PWL_ERR_UNSUPPORTED)
            return status;
    }
    return PWL_ERR_NOT_FOUND;
}
static size_t find(const pwl_acpi_snapshot_t *s,uint64_t pa)
{ size_t i;for(i=0;i<s->count;i++)if(s->tables[i].physical_address==pa)break;return i; }
static const unsigned char *contents(const pwl_acpi_snapshot_t *s,size_t i)
{ return s->storage+s->tables[i].offset; }
static int sdt_signature(const unsigned char *p)
{
    for(size_t i=0;i<4;i++)if(!((p[i]>='A' && p[i]<='Z') ||
        (p[i]>='0' && p[i]<='9') || p[i]=='_'))return 0;
    return 1;
}
static uint64_t fadt_pointer(const unsigned char *p,size_t n,unsigned extended,unsigned legacy)
{ uint64_t address=n>=extended+8?u64(p+extended):0;return address?address:u32(p+legacy); }
static pwl_status_t add(const pwl_acpi_source_t *source,pwl_acpi_snapshot_t *s,
    size_t capacity,uint64_t pa,unsigned kind,size_t *index)
{
    size_t found=find(s,pa);
    if(found<s->count) {
        if(s->tables[found].kind!=kind)return PWL_ERR_BAD_IMAGE;
        *index=found;return PWL_OK;
    }
    if(s->count==PWL_ACPI_MAX_TABLES)return PWL_ERR_OUT_OF_RESOURCES;
    unsigned char header[36];size_t n=0,header_bytes;
    pwl_status_t status;
    if(kind==PWL_ACPI_RSDP) {
        status=rsdp_read(source,pa,header,&n);header_bytes=n;
    } else {
        header_bytes=kind==PWL_ACPI_FACS?8:36;
        status=read_bytes(source,pa,header,header_bytes);
        if(status!=PWL_OK)return status;
        n=u32(header+4);
        if(n<(kind==PWL_ACPI_FACS?64U:36U) || n>PWL_ACPI_MAX_TABLE_BYTES ||
           (kind==PWL_ACPI_FACS && (!signature(header,"FACS",4) || pa%64)) ||
           (kind==PWL_ACPI_SDT && (!sdt_signature(header) || signature(header,"FACS",4))))
            return PWL_ERR_BAD_IMAGE;
    }
    if(status!=PWL_OK)return status;
    if(pa>=LIMIT || n>LIMIT-pa)return PWL_ERR_BAD_IMAGE;
    for(size_t i=0;i<s->count;i++) {
        uint64_t a=s->tables[i].physical_address;
        if(pa<a+s->tables[i].bytes && a<pa+n)return PWL_ERR_BAD_IMAGE;
    }
    size_t offset=(s->bytes+63)&~(size_t)63;
    if(offset>capacity || n>capacity-offset)return PWL_ERR_BUFFER_TOO_SMALL;
    unsigned char *destination=(unsigned char *)(uintptr_t)s->storage+offset;
    status=read_bytes(source,pa,destination,n);
    if(status!=PWL_OK)return status;
    if(!equal(header,destination,header_bytes) ||
       (kind!=PWL_ACPI_FACS && !checksum(destination,n)))return PWL_ERR_BAD_IMAGE;
    *index=s->count;s->tables[s->count++]=(pwl_acpi_table_t){pa,(uint32_t)n,(uint32_t)offset,kind};
    s->bytes=offset+n;return PWL_OK;
}
static pwl_status_t root_capture(const pwl_acpi_source_t *source,pwl_acpi_snapshot_t *s,
    size_t capacity,uint64_t pa,unsigned width,size_t *fadt)
{
    size_t root;
    pwl_status_t status=add(source,s,capacity,pa,PWL_ACPI_SDT,&root);
    if(status!=PWL_OK)return status;
    const unsigned char *p=contents(s,root);size_t n=s->tables[root].bytes;
    if(!signature(p,width==8?"XSDT":"RSDT",4) || (n-36)%width || n==36)
        return PWL_ERR_BAD_IMAGE;
    for(size_t offset=36;offset<n;offset+=width) {
        size_t child;uint64_t address=width==8?u64(p+offset):u32(p+offset);
        status=add(source,s,capacity,address,PWL_ACPI_SDT,&child);
        if(status!=PWL_OK)return status;
        const unsigned char *c=contents(s,child);
        if(signature(c,"XSDT",4) || signature(c,"RSDT",4) || signature(c,"DSDT",4))
            return PWL_ERR_BAD_IMAGE;
        if(signature(c,"FACP",4)) {
            if(*fadt!=PWL_ACPI_MAX_TABLES && *fadt!=child)return PWL_ERR_BAD_IMAGE;
            *fadt=child;
        }
    }
    return PWL_OK;
}
pwl_status_t pwl_acpi_capture(const pwl_acpi_source_t *source,uint64_t rsdp,
    void *storage,size_t capacity,pwl_acpi_snapshot_t *out)
{
    if(!out)return PWL_ERR_INVALID_ARGUMENT;
    *out=(pwl_acpi_snapshot_t){0};
    if(!source_valid(source) || !storage || !capacity ||
       capacity>PWL_ACPI_MAX_SNAPSHOT_BYTES || (uintptr_t)storage>UINTPTR_MAX-capacity)
        return PWL_ERR_INVALID_ARGUMENT;
    pwl_acpi_snapshot_t result={0};result.storage=storage;
    size_t index,fadt=PWL_ACPI_MAX_TABLES;
    pwl_status_t status=add(source,&result,capacity,rsdp,PWL_ACPI_RSDP,&index);
    if(status!=PWL_OK)return status;
    const unsigned char *r=contents(&result,0);
    uint64_t rsdt=u32(r+16),xsdt=result.tables[0].bytes==36?u64(r+24):0;
    if(!rsdt && !xsdt)return PWL_ERR_BAD_IMAGE;
    if(rsdt)status=root_capture(source,&result,capacity,rsdt,4,&fadt);
    if(status==PWL_OK && xsdt)status=root_capture(source,&result,capacity,xsdt,8,&fadt);
    if(status!=PWL_OK)return status;
    if(fadt==PWL_ACPI_MAX_TABLES)return PWL_ERR_BAD_IMAGE;
    const unsigned char *f=contents(&result,fadt);size_t n=result.tables[fadt].bytes;
    if(n!=116 && n<148)return PWL_ERR_BAD_IMAGE;
    uint64_t dsdt=fadt_pointer(f,n,140,40),facs=fadt_pointer(f,n,132,36);
    status=add(source,&result,capacity,dsdt,PWL_ACPI_SDT,&index);
    if(status!=PWL_OK)return status;
    if(!signature(contents(&result,index),"DSDT",4))return PWL_ERR_BAD_IMAGE;
    if(facs)status=add(source,&result,capacity,facs,PWL_ACPI_FACS,&index);
    else if(!(u32(f+112)&(1U<<20)))return PWL_ERR_BAD_IMAGE;
    if(status==PWL_OK)status=pwl_acpi_snapshot_validate(&result);
    if(status==PWL_OK)*out=result;
    return status;
}
static int root_validate(const pwl_acpi_snapshot_t *s,uint64_t pa,unsigned width,
    unsigned char seen[PWL_ACPI_MAX_TABLES],size_t *fadt)
{
    size_t index=find(s,pa);
    if(index==s->count || s->tables[index].kind!=PWL_ACPI_SDT)return 0;
    const unsigned char *p=contents(s,index);size_t n=s->tables[index].bytes;
    if(!signature(p,width==8?"XSDT":"RSDT",4) || n==36 || (n-36)%width)return 0;
    seen[index]=1;
    for(size_t offset=36;offset<n;offset+=width) {
        size_t child=find(s,width==8?u64(p+offset):u32(p+offset));
        if(child==s->count || s->tables[child].kind!=PWL_ACPI_SDT)return 0;
        const unsigned char *c=contents(s,child);
        if(signature(c,"RSDT",4) || signature(c,"XSDT",4) || signature(c,"DSDT",4))return 0;
        seen[child]=1;
        if(signature(c,"FACP",4)) {
            if(*fadt!=PWL_ACPI_MAX_TABLES && *fadt!=child)return 0;
            *fadt=child;
        }
    }
    return 1;
}
pwl_status_t pwl_acpi_snapshot_validate(const pwl_acpi_snapshot_t *s)
{
    if(!s || !s->storage || !s->bytes || s->bytes>PWL_ACPI_MAX_SNAPSHOT_BYTES ||
       !s->count || s->count>PWL_ACPI_MAX_TABLES ||
       (uintptr_t)s->storage>UINTPTR_MAX-s->bytes)return PWL_ERR_INVALID_ARGUMENT;
    for(size_t i=0;i<s->count;i++) {
        const pwl_acpi_table_t *t=&s->tables[i];
        if(!t->physical_address || t->physical_address>=LIMIT ||
           !t->bytes || t->bytes>PWL_ACPI_MAX_TABLE_BYTES || t->bytes>LIMIT-t->physical_address ||
           t->offset>s->bytes || t->bytes>s->bytes-t->offset || t->offset%64)
            return PWL_ERR_BAD_IMAGE;
        for(size_t j=0;j<i;j++) {
            const pwl_acpi_table_t *q=&s->tables[j];
            if((t->physical_address<q->physical_address+q->bytes && q->physical_address<t->physical_address+t->bytes) ||
               (t->offset<q->offset+q->bytes && q->offset<t->offset+t->bytes))return PWL_ERR_BAD_IMAGE;
        }
        const unsigned char *p=contents(s,i);
        if(t->kind==PWL_ACPI_RSDP) {
            if(i || (t->bytes!=20 && t->bytes!=36) || !signature(p,"RSD PTR ",8) ||
               !((t->bytes==20 && p[15]==0) || (t->bytes==36 && p[15]==2 && u32(p+20)==36)) ||
               !checksum(p,20) || !checksum(p,t->bytes))return PWL_ERR_BAD_IMAGE;
        } else if(t->kind==PWL_ACPI_FACS) {
            if(t->bytes<64 || t->physical_address%64 || !signature(p,"FACS",4) ||
               u32(p+4)!=t->bytes)return PWL_ERR_BAD_IMAGE;
        } else if(t->kind==PWL_ACPI_SDT) {
            if(t->bytes<36 || !sdt_signature(p) || signature(p,"FACS",4) ||
               u32(p+4)!=t->bytes || !checksum(p,t->bytes))return PWL_ERR_BAD_IMAGE;
        } else return PWL_ERR_BAD_IMAGE;
    }
    if(s->tables[0].kind!=PWL_ACPI_RSDP)return PWL_ERR_BAD_IMAGE;
    unsigned char seen[PWL_ACPI_MAX_TABLES]={1};size_t fadt=PWL_ACPI_MAX_TABLES;
    const unsigned char *r=contents(s,0);uint64_t rsdt=u32(r+16),xsdt=s->tables[0].bytes==36?u64(r+24):0;
    if((!rsdt && !xsdt) || (rsdt && !root_validate(s,rsdt,4,seen,&fadt)) ||
       (xsdt && !root_validate(s,xsdt,8,seen,&fadt)) || fadt==PWL_ACPI_MAX_TABLES)
        return PWL_ERR_BAD_IMAGE;
    const unsigned char *f=contents(s,fadt);size_t n=s->tables[fadt].bytes;
    if(n!=116 && n<148)return PWL_ERR_BAD_IMAGE;
    size_t dsdt=find(s,fadt_pointer(f,n,140,40));
    if(dsdt==s->count || s->tables[dsdt].kind!=PWL_ACPI_SDT ||
       !signature(contents(s,dsdt),"DSDT",4))return PWL_ERR_BAD_IMAGE;
    seen[dsdt]=1;
    uint64_t facs_pa=fadt_pointer(f,n,132,36);
    if(facs_pa) {
        size_t facs=find(s,facs_pa);
        if(facs==s->count || s->tables[facs].kind!=PWL_ACPI_FACS)return PWL_ERR_BAD_IMAGE;
        seen[facs]=1;
    } else if(!(u32(f+112)&(1U<<20)))return PWL_ERR_BAD_IMAGE;
    for(size_t i=0;i<s->count;i++)if(!seen[i])return PWL_ERR_BAD_IMAGE;
    return PWL_OK;
}
pwl_status_t pwl_acpi_snapshot_recheck(const pwl_acpi_source_t *source,const pwl_acpi_snapshot_t *s)
{
    if(!source_valid(source))return PWL_ERR_INVALID_ARGUMENT;
    pwl_status_t status=pwl_acpi_snapshot_validate(s);
    if(status!=PWL_OK)return status;
    unsigned char bytes[256];
    for(size_t i=0;i<s->count;i++)for(size_t offset=0;offset<s->tables[i].bytes;) {
        size_t n=s->tables[i].bytes-offset;if(n>sizeof(bytes))n=sizeof(bytes);
        status=read_bytes(source,s->tables[i].physical_address+offset,bytes,n);
        if(status!=PWL_OK)return status;
        if(!equal(bytes,contents(s,i)+offset,n))return PWL_ERR_BAD_IMAGE;
        offset+=n;
    }
    return PWL_OK;
}
pwl_status_t pwl_acpi_ranges(const pwl_acpi_snapshot_t *s,pwl_x64_alias_range_t *out,
    size_t capacity,size_t *out_count)
{
    if(!out_count || (!out && capacity))return PWL_ERR_INVALID_ARGUMENT;
    pwl_status_t status=pwl_acpi_snapshot_validate(s);if(status!=PWL_OK)return status;
    /* Sweep interval endpoints: pages shared with FACS need write access. */
    uint64_t endpoints[2*PWL_ACPI_MAX_TABLES];size_t count=0;
    for(size_t i=0;i<s->count;i++) {
        endpoints[count++]=s->tables[i].physical_address&~UINT64_C(4095);
        endpoints[count++]=(s->tables[i].physical_address+s->tables[i].bytes+4095)&~UINT64_C(4095);
    }
    for(size_t i=1;i<count;i++) {
        uint64_t x=endpoints[i];size_t j=i;
        while(j && endpoints[j-1]>x) { endpoints[j]=endpoints[j-1];j--; }endpoints[j]=x;
    }
    /* Count first, then emit: shortage never changes caller ranges and no
     * second 6 KiB range array consumes the preparation stack. */
    for(unsigned pass=0;pass<2;pass++) {
        pwl_x64_alias_range_t last={0};size_t used=0;
        for(size_t i=0;i+1<count;i++) {
            uint64_t a=endpoints[i],b=endpoints[i+1];unsigned covered=0,writable=0;
            if(a==b)continue;
            for(size_t j=0;j<s->count;j++) {
                uint64_t start=s->tables[j].physical_address&~UINT64_C(4095);
                uint64_t end=(s->tables[j].physical_address+s->tables[j].bytes+4095)&~UINT64_C(4095);
                if(a>=start && a<end) { covered=1;writable|=s->tables[j].kind==PWL_ACPI_FACS; }
            }
            if(!covered)continue;
            if(used && last.virtual_address+last.bytes==a && last.writable==writable)last.bytes+=b-a;
            else { last=(pwl_x64_alias_range_t){a,a,b-a,writable,0,0};used++; }
            if(pass)out[used-1]=last;
        }
        if(!pass) { *out_count=used;if(used>capacity)return PWL_ERR_BUFFER_TOO_SMALL; }
    }
    return PWL_OK;
}
pwl_status_t pwl_acpi_mappings_validate(const pwl_acpi_snapshot_t *s,
    const pwl_x64_table_page_t *tables,size_t count,uint64_t root)
{
    pwl_status_t status=pwl_acpi_snapshot_validate(s);
    if(status!=PWL_OK)return status;
    for(size_t i=0;i<s->count;i++)for(uint64_t p=s->tables[i].physical_address&~UINT64_C(4095);
        p<s->tables[i].physical_address+s->tables[i].bytes;p+=4096) {
        unsigned writable=0;
        for(size_t j=0;j<s->count;j++)if(s->tables[j].kind==PWL_ACPI_FACS &&
            p<((s->tables[j].physical_address+s->tables[j].bytes+4095)&~UINT64_C(4095)) &&
            p+4096>s->tables[j].physical_address)writable=1;
        pwl_x64_translation_t x;
        if(pwl_x64_translate(tables,count,root,p,&x)!=PWL_OK || x.physical_address!=p ||
           x.writable!=writable || x.executable || x.user || x.pat_index)
            return PWL_ERR_BAD_IMAGE;
    }
    return PWL_OK;
}
