#include "pwl_files.h"
static uint16_t u16(const unsigned char *p) { return p[0]|((uint16_t)p[1]<<8); }
static uint32_t u32(const unsigned char *p) { return u16(p)|((uint32_t)u16(p+2)<<16); }
static uint64_t u64(const unsigned char *p) { return u32(p)|((uint64_t)u32(p+4)<<32); }
static uint16_t fold(uint16_t c) { return c>='a' && c<='z' ? c-32 : c; }
static int header(const unsigned char *p,size_t bytes,uint32_t *count)
{
    const unsigned char magic[8]={'P','W','L','F','I','L','E','S'};
    if (!p || bytes<PWL_FILES_HEADER) return 0;
    for (size_t i=0;i<8;i++) if (p[i]!=magic[i]) return 0;
    *count=u32(p+12);
    return u32(p+8)==1 && *count && *count<=PWL_FILES_MAX &&
        u64(p+16)==bytes && *count<=(bytes-PWL_FILES_HEADER)/PWL_FILES_RECORD;
}
static const unsigned char *record(const unsigned char *p,uint32_t index)
{ return p+PWL_FILES_HEADER+(size_t)index*PWL_FILES_RECORD; }
static int path_size(const unsigned char *p,size_t *length)
{
    if (u16(p)!='\\') return 0;
    size_t component=1;
    for (size_t i=1;i<PWL_FILES_PATH;i++) {
        uint16_t c=u16(p+i*2);
        if (!c || c=='\\') {
            size_t n=i-component;
            if (!n && i!=1) return 0;
            if ((n==1 && u16(p+component*2)=='.') ||
                (n==2 && u16(p+component*2)=='.' && u16(p+(component+1)*2)=='.')) return 0;
            if (!c) { *length=i;return 1; }
            component=i+1;
        }
        if (c=='/' || c==':' || (c && c<32)) return 0;
    }
    return 0;
}
static int equal(const unsigned char *a,const unsigned char *b,size_t n)
{
    for (size_t i=0;i<n;i++) if (fold(u16(a+i*2))!=fold(u16(b+i*2))) return 0;
    return 1;
}
pwl_status_t pwl_files_validate(const void *archive,size_t bytes)
{
    const unsigned char *p=archive;uint32_t count;
    if (!header(p,bytes,&count)) return PWL_ERR_BAD_IMAGE;
    uint64_t data=PWL_FILES_HEADER+(uint64_t)count*PWL_FILES_RECORD;
    int root=0;
    for (uint32_t i=0;i<count;i++) {
        const unsigned char *r=record(p,i);size_t n;
        uint64_t offset=u64(r+512),size=u64(r+520);uint32_t flags=u32(r+528);
        if (!path_size(r,&n) || flags>1 || u32(r+532) || offset<data ||
            offset>bytes || size>bytes-offset || (flags && size)) return PWL_ERR_BAD_IMAGE;
        if (n==1) { if (!flags) return PWL_ERR_BAD_IMAGE;root=1; }
        for (size_t k=n;k<PWL_FILES_PATH;k++) if (u16(r+k*2)) return PWL_ERR_BAD_IMAGE;
        for (uint32_t j=0;j<i;j++) {
            const unsigned char *other=record(p,j);size_t m=0;
            if (!path_size(other,&m)) return PWL_ERR_BAD_IMAGE;
            if (m==n && equal(r,other,n)) return PWL_ERR_BAD_IMAGE;
            uint64_t start=u64(other+512),length=u64(other+520);
            if (size && length && offset<start+length && start<offset+size) return PWL_ERR_BAD_IMAGE;
        }
        if (n>1) {
            size_t parent=n;
            while (parent>1 && u16(r+(parent-1)*2)!='\\') parent--;
            parent=parent>1 ? parent-1 : 1;
            int found=0;
            for (uint32_t j=0;j<count;j++) {
                const unsigned char *other=record(p,j);size_t m=0;
                if (path_size(other,&m) && m==parent && u32(other+528)==1 && equal(r,other,m)) found=1;
            }
            if (!found) return PWL_ERR_BAD_IMAGE;
        }
    }
    return root ? PWL_OK : PWL_ERR_BAD_IMAGE;
}
uint64_t pwl_files_open(const void *archive,size_t bytes,const uint16_t *path,pwl_file_view_t *file)
{
    const unsigned char *p=archive;uint32_t count;
    if (!path || !file || !header(p,bytes,&count)) return PWL_EFI_INVALID_PARAMETER;
    size_t n;
    for (n=0;n<PWL_FILES_PATH;n++) if (!path[n]) break;
    if (n==PWL_FILES_PATH) return PWL_EFI_INVALID_PARAMETER;
    for (uint32_t i=0;i<count;i++) {
        const unsigned char *r=record(p,i);size_t k;
        for (k=0;k<n;k++) if (fold(path[k])!=fold(u16(r+k*2))) break;
        if (k!=n || u16(r+n*2)) continue;
        uint64_t offset=u64(r+512),size=u64(r+520);
        if (offset>bytes || size>bytes-offset || u32(r+528)>1) return PWL_EFI_INVALID_PARAMETER;
        *file=(pwl_file_view_t){offset,size,u32(r+528),i};
        return PWL_EFI_SUCCESS;
    }
    return PWL_EFI_NOT_FOUND;
}
uint64_t pwl_files_read(const void *archive,size_t bytes,const pwl_file_view_t *file,
                        uint64_t *position,size_t *size,void *buffer)
{
    const unsigned char *p=archive;uint32_t count;
    if (!file || !position || !size || (!buffer && *size) || !header(p,bytes,&count) ||
        file->index>=count || file->directory) return PWL_EFI_INVALID_PARAMETER;
    const unsigned char *r=record(p,file->index);
    if (file->offset!=u64(r+512) || file->size!=u64(r+520) || u32(r+528) ||
        file->offset>bytes || file->size>bytes-file->offset) return PWL_EFI_INVALID_PARAMETER;
    uint64_t remaining=*position<file->size ? file->size-*position : 0;
    size_t n=*size;if (remaining<n) n=(size_t)remaining;
    unsigned char *d=buffer;
    if (n) {
        const unsigned char *s=p+(size_t)(file->offset+*position);
        if ((uintptr_t)d>(uintptr_t)s && (uintptr_t)d-(uintptr_t)s<n)
            for (size_t i=n;i;i--) d[i-1]=s[i-1];
        else for (size_t i=0;i<n;i++) d[i]=s[i];
        *position+=n;
    }
    *size=n;
    return PWL_EFI_SUCCESS;
}
uint64_t pwl_files_at(const void *archive,size_t bytes,uint32_t index,
                     pwl_file_view_t *file,uint16_t path[PWL_FILES_PATH])
{
    const unsigned char *p=archive;uint32_t count;
    if (!file || !path || !header(p,bytes,&count)) return PWL_EFI_INVALID_PARAMETER;
    if (index>=count) return PWL_EFI_NOT_FOUND;
    const unsigned char *r=record(p,index);size_t n;
    uint64_t offset=u64(r+512),size=u64(r+520);
    if (!path_size(r,&n) || offset>bytes || size>bytes-offset || u32(r+528)>1)
        return PWL_EFI_INVALID_PARAMETER;
    for (size_t i=0;i<=n;i++) path[i]=u16(r+i*2);
    *file=(pwl_file_view_t){offset,size,u32(r+528),index};
    return PWL_EFI_SUCCESS;
}
