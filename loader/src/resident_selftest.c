#include "pwl_resident_selftest.h"
#define EFI __attribute__((ms_abi))
typedef uint64_t (EFI *raise_fn)(uint64_t);
typedef void (EFI *restore_fn)(uint64_t);
typedef uint64_t (EFI *alloc_fn)(unsigned,unsigned,uint64_t,uint64_t *);
typedef uint64_t (EFI *free_fn)(uint64_t,uint64_t);
typedef uint64_t (EFI *map_fn)(size_t *,pwl_efi_memory_descriptor_t *,uint64_t *,size_t *,uint32_t *);
typedef uint64_t (EFI *exit_fn)(uint64_t,uint64_t);
typedef uint64_t (EFI *crc_fn)(const void *,size_t,uint32_t *);
typedef void (EFI *copy_fn)(void *,const void *,size_t);
typedef void (EFI *set_fn)(void *,size_t,unsigned char);
#define LOAD(type,name,index) type name; do { \
    void *entry=(unsigned char *)code+b->callbacks[index]; \
    _Static_assert(sizeof(name)==sizeof(entry),"AMD64 pointer ABI"); \
    for(size_t i=0;i<sizeof(name);i++) \
        ((unsigned char *)&name)[i]=((const unsigned char *)&entry)[i]; \
} while(0)
#define BEFORE(n) do { \
    r->last_call=(n); \
    if(checkpoint && checkpoint((n),context))return PWL_ERR_IO; \
} while(0)
#define PASSED(n) (r->passed_mask|=1U<<((n)-1))

pwl_status_t pwl_resident_calls_test(const pwl_resident_image_t *b,void *code,
    pwl_resident_data_t *d,pwl_resident_call_report_t *r,
    int (*checkpoint)(unsigned,void *),void *context)
{
    if(!b || !code || !d || !r || pwl_resident_image_validate(b)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    *r=(pwl_resident_call_report_t){0};
    /* Verify the executable copy, except its deliberate context binding. */
    const unsigned char *copy=code,*source=b->bytes;
    for(size_t i=0;i<b->size;i++)
        if(i<b->binding_offset || i>=b->binding_offset+8)
            if(copy[i]!=source[i])return PWL_ERR_BAD_IMAGE;
    uint64_t binding=0;
    for(unsigned i=0;i<8;i++)binding|=(uint64_t)copy[b->binding_offset+i]<<(8*i);
    if(binding!=(uint64_t)(uintptr_t)d)return PWL_ERR_INVALID_ARGUMENT;
    pwl_phys_region_t region={UINT64_C(0x100000000),65536,PWL_MEMORY_FREE};
    uint64_t cache=8,pa=0,key=0;
    if(pwl_fw_memory_init(&d->memory,&region,&cache,1,0x1234)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    d->tpl=4;
    LOAD(raise_fn,raise_tpl,0);LOAD(restore_fn,restore_tpl,1);
    LOAD(alloc_fn,allocate,2);LOAD(free_fn,release,3);LOAD(map_fn,map,4);
    LOAD(exit_fn,exit_boot,5);LOAD(crc_fn,crc,6);LOAD(copy_fn,copy_mem,7);LOAD(set_fn,set_mem,8);
    BEFORE(1);if(raise_tpl(16)!=4 || d->tpl!=16)return PWL_ERR_BAD_IMAGE;
    PASSED(1);
    BEFORE(2);restore_tpl(4);if(d->tpl!=4)return PWL_ERR_BAD_IMAGE;
    PASSED(2);
    BEFORE(3);
    if(allocate(PWL_ALLOCATE_ANY,2,2,&pa)!=0 || pa!=region.base)return PWL_ERR_BAD_IMAGE;
    PASSED(3);
    /* GetMemoryMap precedes FreePages so the changed key can also be checked. */
    BEFORE(5);
    size_t size=0,ds=0;uint32_t version=0;
    if(map(&size,NULL,&key,&ds,&version)!=PWL_EFI_BUFFER_TOO_SMALL || size!=80 || ds!=40 || version!=1)
        return PWL_ERR_BAD_IMAGE;
    pwl_efi_memory_descriptor_t descriptors[2];size=sizeof(descriptors);
    if(map(&size,descriptors,&key,&ds,&version)!=0 || size!=80 ||
       descriptors[0].type!=2 || descriptors[0].physical_start!=region.base ||
       descriptors[0].number_of_pages!=2 || descriptors[1].type!=7)
        return PWL_ERR_BAD_IMAGE;
    PASSED(5);
    BEFORE(4);if(release(pa,2)!=0 || d->memory.key==key)return PWL_ERR_BAD_IMAGE;
    PASSED(4);
    BEFORE(6);r->exit_status=exit_boot(0x1234,key);
    if(r->exit_status!=PWL_EFI_UNSUPPORTED || d->memory.exited)return PWL_ERR_BAD_IMAGE;
    PASSED(6);
    BEFORE(7);uint32_t result=0;
    if(crc("123456789",9,&result)!=0 || result!=UINT32_C(0xcbf43926))return PWL_ERR_BAD_IMAGE;
    PASSED(7);
    BEFORE(8);char bytes[16]="123456789";
    copy_mem(bytes+2,bytes,7);
    const char expected[]="121234567";
    for(size_t i=0;i<sizeof(expected);i++)if(bytes[i]!=expected[i])return PWL_ERR_BAD_IMAGE;
    copy_mem(bytes,bytes+2,7);
    for(size_t i=0;i<7;i++)if(bytes[i]!=(char)('1'+i))return PWL_ERR_BAD_IMAGE;
    PASSED(8);
    BEFORE(9);set_mem(bytes,sizeof(bytes),0xa5);
    for(size_t i=0;i<sizeof(bytes);i++)if((unsigned char)bytes[i]!=0xa5)return PWL_ERR_BAD_IMAGE;
    PASSED(9);
    return PWL_OK;
}

typedef uint64_t (EFI *install_fn)(uint64_t *,const pwl_efi_guid_t *,unsigned,void *);
typedef uint64_t (EFI *replace_fn)(uint64_t,const pwl_efi_guid_t *,void *,void *);
typedef uint64_t (EFI *remove_fn)(uint64_t,const pwl_efi_guid_t *,void *);
typedef uint64_t (EFI *handle_fn)(uint64_t,const pwl_efi_guid_t *,void **);
typedef uint64_t (EFI *locate_fn)(const pwl_efi_guid_t *,void *,void **);
typedef uint64_t (EFI *handles_fn)(unsigned,const pwl_efi_guid_t *,void *,size_t *,uint64_t *);
typedef uint64_t (EFI *open_protocol_fn)(uint64_t,const pwl_efi_guid_t *,void **,uint64_t,uint64_t,uint32_t);
typedef uint64_t (EFI *close_protocol_fn)(uint64_t,const pwl_efi_guid_t *,uint64_t,uint64_t);
typedef uint64_t (EFI *volume_fn)(void *,void **);
typedef uint64_t (EFI *file_open_fn)(void *,void **,const uint16_t *,uint64_t,uint64_t);
typedef uint64_t (EFI *file_close_fn)(void *);
typedef uint64_t (EFI *file_read_fn)(void *,size_t *,void *);
typedef uint64_t (EFI *file_write_fn)(void *,size_t *,const void *);
typedef uint64_t (EFI *file_set_info_fn)(void *,const pwl_efi_guid_t *,size_t,const void *);
typedef uint64_t (EFI *file_get_pos_fn)(void *,uint64_t *);
typedef uint64_t (EFI *file_set_pos_fn)(void *,uint64_t);
typedef uint64_t (EFI *file_info_fn)(void *,const pwl_efi_guid_t *,size_t *,void *);

typedef uint64_t (EFI *pool_alloc_fn)(unsigned,size_t,void **);
typedef uint64_t (EFI *pool_free_fn)(void *);
static void put_le(unsigned char *p,uint64_t value,unsigned n)
{ for(unsigned i=0;i<n;i++)p[i]=(unsigned char)(value>>(8*i)); }
pwl_status_t pwl_resident_all_calls_test(const pwl_resident_image_t *b,void *code,
    pwl_resident_data_t *d,void *scratch,size_t scratch_bytes,pwl_resident_call_report_t *r)
{
    if (!b || !code || !d || !r || !scratch || scratch_bytes<4096 || (uintptr_t)scratch%4096)
        return PWL_ERR_INVALID_ARGUMENT;
    for(size_t i=0;i<sizeof(*d);i++)((unsigned char *)d)[i]=0;
    pwl_status_t status=pwl_resident_calls_test(b,code,d,r,NULL,NULL);
    if(status!=PWL_OK)return status;
    pwl_phys_region_t region={(uintptr_t)scratch,4096,PWL_MEMORY_FREE};uint64_t cache=8;
    if(pwl_fw_memory_init(&d->memory,&region,&cache,1,0x1234)!=PWL_OK)return PWL_ERR_BAD_IMAGE;
    int (*checkpoint)(unsigned,void *)=NULL;void *context=NULL;
    LOAD(pool_alloc_fn,pool_alloc,9);LOAD(pool_free_fn,pool_free,10);
    void *pool=NULL;
    BEFORE(10);if(pool_alloc(2,32,&pool) || pool!=scratch)return PWL_ERR_BAD_IMAGE;
    ((volatile unsigned char *)pool)[0]=0x5a;
    if(((volatile unsigned char *)pool)[0]!=0x5a)return PWL_ERR_BAD_IMAGE;
    PASSED(10);
    BEFORE(11);if(pool_free(pool))return PWL_ERR_BAD_IMAGE;
    PASSED(11);
    LOAD(install_fn,install,11);LOAD(replace_fn,replace,12);LOAD(remove_fn,remove,13);
    LOAD(handle_fn,handle,14);LOAD(handles_fn,handles,15);LOAD(locate_fn,locate,16);
    LOAD(open_protocol_fn,open_protocol,28);LOAD(close_protocol_fn,close_protocol,29);
    const pwl_efi_guid_t guid={{0x50,0x57,0x4c,1}};uint64_t agent=0;void *out=NULL;
    BEFORE(12);if(install(&agent,&guid,0,scratch) || !agent)return PWL_ERR_BAD_IMAGE;
    PASSED(12);
    BEFORE(13);if(replace(agent,&guid,scratch,(unsigned char *)scratch+8))return PWL_ERR_BAD_IMAGE;
    PASSED(13);
    BEFORE(15);if(handle(agent,&guid,&out) || out!=(unsigned char *)scratch+8)return PWL_ERR_BAD_IMAGE;
    PASSED(15);
    BEFORE(16);size_t size=sizeof(uint64_t);uint64_t found=0;
    if(handles(2,&guid,NULL,&size,&found) || found!=agent || size!=8)return PWL_ERR_BAD_IMAGE;
    PASSED(16);
    BEFORE(17);out=NULL;if(locate(&guid,NULL,&out) || out!=(unsigned char *)scratch+8)return PWL_ERR_BAD_IMAGE;
    PASSED(17);
    BEFORE(29);if(open_protocol(agent,&guid,&out,agent,0,2) || out!=(unsigned char *)scratch+8)return PWL_ERR_BAD_IMAGE;
    PASSED(29);
    BEFORE(30);if(close_protocol(agent,&guid,agent,0))return PWL_ERR_BAD_IMAGE;
    PASSED(30);
    BEFORE(14);if(remove(agent,&guid,(unsigned char *)scratch+8))return PWL_ERR_BAD_IMAGE;
    PASSED(14);
    unsigned char archive[1536]={0};const char magic[]="PWLFILES";
    for(unsigned i=0;i<8;i++)archive[i]=(unsigned char)magic[i];
    put_le(archive+8,1,4);put_le(archive+12,2,4);put_le(archive+16,sizeof(archive),8);
    const char *paths[]={"\\","\\CHECK"};
    for(unsigned i=0;i<2;i++) {
        unsigned char *record=archive+24+i*536;
        for(size_t j=0;paths[i][j];j++)record[j*2]=(unsigned char)paths[i][j];
        put_le(record+512,1096,8);put_le(record+520,i ? 3 : 0,8);put_le(record+528,i ? 0 : 1,4);
    }
    archive[1096]='A';archive[1097]='B';archive[1098]='C';
    if(pwl_files_validate(archive,sizeof(archive))!=PWL_OK)return PWL_ERR_BAD_IMAGE;
    d->media.physical_address=(uintptr_t)archive;d->media.bytes=sizeof(archive);d->files_enabled=1;
    d->file_template.revision=0x10000;
    for(unsigned i=0;i<10;i++)d->file_template.functions[i]=(uintptr_t)code+b->callbacks[18+i];
    LOAD(volume_fn,volume,17);LOAD(file_open_fn,open_file,18);LOAD(file_close_fn,close_file,19);
    LOAD(file_close_fn,delete_file,20);LOAD(file_read_fn,read_file,21);LOAD(file_write_fn,write_file,22);
    LOAD(file_get_pos_fn,get_pos,23);LOAD(file_set_pos_fn,set_pos,24);
    LOAD(file_info_fn,get_info,25);LOAD(file_set_info_fn,set_info,26);LOAD(file_close_fn,flush,27);
    void *root=NULL,*file=NULL;const uint16_t path[]={'C','H','E','C','K',0};
    BEFORE(18);if(volume(d->filesystem,&root) || !root)return PWL_ERR_BAD_IMAGE;
    PASSED(18);
    BEFORE(19);if(open_file(root,&file,path,1,0) || !file)return PWL_ERR_BAD_IMAGE;
    PASSED(19);
    char bytes[96]={0};size=3;
    BEFORE(22);if(read_file(file,&size,bytes) || size!=3 || bytes[0]!='A' || bytes[2]!='C')return PWL_ERR_BAD_IMAGE;
    PASSED(22);
    BEFORE(24);uint64_t position=0;if(get_pos(file,&position) || position!=3)return PWL_ERR_BAD_IMAGE;
    PASSED(24);
    BEFORE(25);if(set_pos(file,0))return PWL_ERR_BAD_IMAGE;
    PASSED(25);
    const pwl_efi_guid_t info={{0x92,0x6e,0x57,0x09,0x3f,0x6d,0xd2,0x11,0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
    BEFORE(26);size=sizeof(bytes);if(get_info(file,&info,&size,bytes) || size!=92)return PWL_ERR_BAD_IMAGE;
    PASSED(26);
    BEFORE(23);size=1;if(write_file(file,&size,bytes)!=PWL_EFI_WRITE_PROTECTED)return PWL_ERR_BAD_IMAGE;
    PASSED(23);
    BEFORE(27);if(set_info(file,&info,size,bytes)!=PWL_EFI_WRITE_PROTECTED)return PWL_ERR_BAD_IMAGE;
    PASSED(27);
    BEFORE(28);if(flush(file))return PWL_ERR_BAD_IMAGE;
    PASSED(28);
    BEFORE(21);if(delete_file(file)!=2)return PWL_ERR_BAD_IMAGE;
    PASSED(21);
    BEFORE(20);if(close_file(root))return PWL_ERR_BAD_IMAGE;
    PASSED(20);
    d->files_enabled=0;d->media=(pwl_fw_media_t){0};
    r->last_call=30;
    return r->passed_mask==UINT32_C(0x3fffffff) ? PWL_OK : PWL_ERR_BAD_IMAGE;
}
