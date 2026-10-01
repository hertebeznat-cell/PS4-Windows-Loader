#define _GNU_SOURCE
#include "resident_test_image.h"
#include "pwl_image_permissions.h"
#include "pe_fixture.h"
#include <signal.h>
#include <unistd.h>
typedef uint64_t (EFI *load_t)(unsigned char,uint64_t,const void *,const void *,size_t,uint64_t *);
typedef uint64_t (EFI *start_t)(uint64_t,size_t *,uint16_t **);
typedef uint64_t (EFI *exit_t)(uint64_t,uint64_t,size_t,const uint16_t *);
typedef uint64_t (EFI *unload_t)(uint64_t);
typedef uint64_t (EFI *handle_t)(uint64_t,const pwl_efi_guid_t *,void **);
extern int test_image_abi(uintptr_t start,uint64_t image);
static pwl_resident_data_t *current;
static load_t load_image;static start_t start_image;static exit_t exit_image;static unload_t unload_image;
static unsigned deny_mapping;
static uint64_t EFI host_permissions(pwl_resident_data_t *d,const pwl_pe_loaded_t *im,unsigned mode)
{
    assert(d==current && mode<=2);
    if (deny_mapping==mode+1) return PWL_EFI_DEVICE_ERROR;
    if (mode==2) return PWL_EFI_SUCCESS;
    for (size_t i=0;i<im->range_count;i++) {
        const pwl_x64_identity_range_t *r=&im->ranges[i];
        int flags=PROT_READ;
        if (mode==1 || r->writable) flags|=PROT_WRITE;
        if (mode==0 && r->executable) flags|=PROT_EXEC;
        assert(mprotect((void *)(uintptr_t)r->base,(size_t)r->size,flags)==0);
    }
    return PWL_EFI_SUCCESS;
}
static void return_program(unsigned char file[PE_FIXTURE_BYTES],uint64_t status)
{ pe_fixture(file);file[0x200]=0x48;file[0x201]=0xb8;pe64(file+0x202,status);file[0x20a]=0xc3; }
static uint64_t EFI nested_program(uint64_t handle,pwl_efi_system_table_t *table)
{
    assert(table==&current->efi.system && current->current_application==handle);
    assert(start_image(handle,NULL,NULL)==PWL_EFI_INVALID_PARAMETER);
    assert(unload_image(handle)==PWL_EFI_UNSUPPORTED);
    assert(exit_image(1,PWL_EFI_DEVICE_ERROR,0,NULL)==PWL_EFI_INVALID_PARAMETER);
    unsigned char file[PE_FIXTURE_BYTES];return_program(file,PWL_EFI_NOT_FOUND);
    uint64_t child=0;
    assert(load_image(0,handle,NULL,file,sizeof(file),&child)==0);
    assert(child!=handle && start_image(child,NULL,NULL)==PWL_EFI_NOT_FOUND);
    assert(current->current_application==handle);
    return UINT64_C(0x123456789abcdef0);
}
static void test_page_permissions(void)
{
    void *allocation=mmap(NULL,4*1024*1024,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    uint64_t base=((uintptr_t)allocation+0x1fffff)&~UINT64_C(0x1fffff);
    uint64_t *pages=mmap(NULL,16384,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(allocation!=MAP_FAILED && pages!=MAP_FAILED);
    uint64_t root=(uintptr_t)pages;
    pages[(base>>39)&511]=(root+4096)|3;
    pages[512+((base>>30)&511)]=(root+8192)|3;
    pages[1024+((base>>21)&511)]=(root+12288)|3;
    for (unsigned i=0;i<4;i++) pages[1536+i]=base+i*4096+UINT64_C(0x8000000000000003);
    pwl_image_mapping_t m={0};m.root=root;m.tables_base=root;m.tables_bytes=16384;
    m.heap_base=base;m.heap_bytes=16384;
    pwl_pe_loaded_t im={0};im.physical_address=base;im.image_size=16384;im.entry_address=base+4096;
    im.range_count=3;im.ranges[0]=(pwl_x64_identity_range_t){base,4096,0,0};
    im.ranges[1]=(pwl_x64_identity_range_t){base+4096,4096,0,1};
    im.ranges[2]=(pwl_x64_identity_range_t){base+8192,8192,1,0};
    uint64_t saved[2048];memcpy(saved,pages,sizeof(saved));
    pages[1539]^=4096;
    uint64_t invalid[2048];memcpy(invalid,pages,sizeof(invalid));
    assert(pwl_image_permissions(&m,&im,0,1)==PWL_EFI_DEVICE_ERROR);
    assert(!memcmp(invalid,pages,sizeof(invalid)));memcpy(pages,saved,sizeof(saved));
    pages[1024+((base>>21)&511)]|=UINT64_C(1)<<63;
    assert(pwl_image_permissions(&m,&im,0,1)==PWL_EFI_DEVICE_ERROR);memcpy(pages,saved,sizeof(saved));
    pages[1537]|=256;assert(pwl_image_permissions(&m,&im,0,1)==PWL_EFI_DEVICE_ERROR);memcpy(pages,saved,sizeof(saved));
    pages[1537]|=4;assert(pwl_image_permissions(&m,&im,0,1)==PWL_EFI_DEVICE_ERROR);memcpy(pages,saved,sizeof(saved));
    assert(pwl_image_permissions(&m,&im,0,2)==PWL_EFI_DEVICE_ERROR);
    assert(pwl_image_permissions(&m,&im,0,1)==0);
    assert(pwl_image_permissions(&m,&im,0,2)==0);
    assert(!(pages[1537]&(UINT64_C(1)<<63|2)) && (pages[1538]&2));
    assert(pwl_image_permissions(&m,&im,1,1)==0 && !memcmp(saved,pages,sizeof(saved)));
    m.heap_bytes=4096;assert(pwl_image_permissions(&m,&im,0,1)==PWL_EFI_INVALID_PARAMETER);
    assert(munmap(pages,16384)==0 && munmap(allocation,4*1024*1024)==0);
}
int main(void)
{
    alarm(15);test_page_permissions();
    resident_test_image_t t=resident_test_open();current=t.data;
    LOAD_FROM(current->efi.boot.functions[22],load_t,load);load_image=load;
    LOAD_FROM(current->efi.boot.functions[23],start_t,start);start_image=start;
    LOAD_FROM(current->efi.boot.functions[24],exit_t,exit);exit_image=exit;
    LOAD_FROM(current->efi.boot.functions[25],unload_t,unload);unload_image=unload;
    LOAD_FROM(current->efi.boot.functions[16],handle_t,handle);
    static const pwl_efi_guid_t guid={{0xa1,0x31,0x1b,0x5b,0x62,0x95,0xd2,0x11,0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}};
    current->protocols[0]=(pwl_resident_protocol_t){1,(uintptr_t)&current->loaded_image,guid};
    current->protocol_next_handle=1;
    void *heap=mmap(NULL,1024*1024,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(heap!=MAP_FAILED && (uintptr_t)heap>UINT32_MAX);
    pwl_phys_region_t region={(uintptr_t)heap,1024*1024,PWL_MEMORY_FREE};uint64_t cache=8;
    assert(pwl_fw_memory_init(&current->memory,&region,&cache,1,1)==0);
    unsigned char file[PE_FIXTURE_BYTES];return_program(file,PWL_EFI_NOT_FOUND);uint64_t child=UINT64_MAX;
    assert(load_image(0,1,NULL,file,sizeof(file),&child)==PWL_EFI_UNSUPPORTED && child==UINT64_MAX);
    /* Production adapter refuses CPL3 before privileged reads or page edits. */
    current->image_mapping.permission_callback=(uintptr_t)t.code+t.image.callbacks[52];
    assert(load_image(0,1,NULL,file,sizeof(file),&child)==PWL_EFI_UNSUPPORTED);
    assert(current->memory.count==1 && current->memory.entries[0].descriptor.type==7);
    current->image_mapping.permission_callback=(uintptr_t)host_permissions;
    assert(load_image(0,99,NULL,file,sizeof(file),&child)==PWL_EFI_INVALID_PARAMETER);
    deny_mapping=1;
    assert(load_image(0,1,NULL,file,sizeof(file),&child)==PWL_EFI_DEVICE_ERROR && child==UINT64_MAX);
    assert(current->memory.count==1 && current->memory.entries[0].descriptor.type==7);deny_mapping=0;
    assert(load_image(1,1,NULL,file,sizeof(file),&child)==0 && child!=1);
    pwl_efi_loaded_image_t *info=NULL;
    assert(handle(child,&guid,(void **)&info)==0 && info->parent_handle==1);
    assert(info->image_base>UINT32_MAX && info->image_size==0x4000 && !info->file_path);
    assert(pe_get64((void *)(uintptr_t)(info->image_base+0x2000))==info->image_base+0x1010);
    size_t n=99;uint16_t *data=(void *)(uintptr_t)1;
    assert(start_image(child,&n,&data)==PWL_EFI_NOT_FOUND && n==0 && !data);
    assert(start_image(child,NULL,NULL)==PWL_EFI_INVALID_PARAMETER && current->memory.count==1);
    /* Actual PE entry calls resident Exit and must not execute its tail. */
    pe_fixture(file);
    unsigned char program[]={0x48,0x83,0xec,0x28,0x48,0x8b,0x42,0x60,
        0x48,0x8b,0x80,0xd8,0,0,0,0x48,0xba,0,0,0,0,0,0,0,0,
        0x49,0xc7,0xc0,0x06,0,0,0,0x4c,0x8d,0x0d,0xd9,0x0f,0,0,
        0xff,0xd0,0x48,0xc7,0xc0,0xad,0x0b,0,0,0x48,0x83,0xc4,0x28,0xc3};
    pe64(program+17,PWL_EFI_DEVICE_ERROR);memcpy(file+0x200,program,sizeof(program));
    /* LEA points to RVA 0x2000 (the relocated pointer): use explicit UTF-16 data instead. */
    pe16(file+0x400,'E');pe16(file+0x402,'!');pe16(file+0x404,0);
    pe16(file+0x608,0); /* ABSOLUTE padding; no data fixup. */
    assert(load_image(0,1,NULL,file,sizeof(file),&child)==0);
    assert(start_image(child,&n,&data)==PWL_EFI_DEVICE_ERROR && n==6 && data);
    assert(data[0]=='E' && data[1]=='!' && !data[2]);
    assert(pwl_fw_free_pool(&current->memory,(uintptr_t)data)==0 && current->memory.count==1);
    /* Exit restores the parent's nonvolatile XMM state even if the exiting
     * application has changed it and never returns through its own prologue. */
    const unsigned char clobber[]={0x66,0x0f,0xef,0xf6,0x66,0x45,0x0f,0xef,0xff};
    memmove(file+0x200+sizeof(clobber),file+0x200,sizeof(program));
    memcpy(file+0x200,clobber,sizeof(clobber));file[0x200+sizeof(clobber)+35]=0xd0;
    assert(load_image(0,1,NULL,file,sizeof(file),&child)==0);
    assert(test_image_abi((uintptr_t)start_image,child)==0 && current->memory.count==1);
    /* A native nested entry checks active-image identity and recursive starts. */
    return_program(file,(uintptr_t)nested_program);file[0x20a]=0xff;file[0x20b]=0xe0;
    assert(load_image(0,1,NULL,file,sizeof(file),&child)==0);
    assert(start_image(child,NULL,NULL)==UINT64_C(0x123456789abcdef0));
    assert(!current->current_application && current->memory.count==1);
    /* The preloaded-image wrapper uses the same lifecycle but leaves its PE
     * span reserved for the preparation owner, instead of freeing arena pages. */
    return_program(file,PWL_EFI_NOT_FOUND);assert(load_image(0,1,NULL,file,sizeof(file),&child)==0);
    pwl_resident_application_t *initial=NULL;
    for (size_t i=0;i<PWL_RESIDENT_APPLICATIONS;i++) {
        pwl_resident_application_t *a=(void *)(uintptr_t)current->applications[i];
        if (a && a->handle==child) { initial=a;current->applications[i]=0;break; }
    }
    assert(initial);current->initial_application=*initial;current->initial_application.initial=1;
    current->loaded_image=initial->protocol;
    uint64_t initial_base=initial->mapped.physical_address;
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++)
        if (current->protocols[i].handle==child && !memcmp(&current->protocols[i].guid,&guid,sizeof(guid)))
            current->protocols[i].interface_address=(uintptr_t)&current->loaded_image;
    assert(pwl_fw_free_pool(&current->memory,(uintptr_t)initial)==0);
    typedef uint64_t (EFI *boot_t)(uint64_t,pwl_efi_system_table_t *);
    LOAD_FROM((uintptr_t)t.code+t.image.callbacks[53],boot_t,boot);
    assert(boot(child,NULL)==PWL_EFI_INVALID_PARAMETER);
    assert(boot(child,&current->efi.system)==PWL_EFI_NOT_FOUND && !current->initial_application.handle);
    assert(mprotect((void *)(uintptr_t)initial_base,0x4000,PROT_READ|PROT_WRITE)==0);
    assert(pwl_fw_free_pages(&current->memory,initial_base,4)==0 && current->memory.count==1);
    /* File-path loading copies the source and the actual short-form device
     * path; both can be discarded by the caller before StartImage. */
    unsigned char archive[4096]={0};memcpy(archive,"PWLFILES",8);
    pe32(archive+8,1);pe32(archive+12,2);pe64(archive+16,sizeof(archive));
    const char *names[]={"\\","\\boot.efi"};
    for (size_t i=0;i<2;i++) {
        unsigned char *record=archive+24+i*536;
        for (size_t j=0;names[i][j];j++) record[j*2]=(unsigned char)names[i][j];
        pe64(record+512,2048);pe64(record+520,i ? PE_FIXTURE_BYTES : 0);pe32(record+528,i ? 0 : 1);
    }
    return_program(archive+2048,PWL_EFI_NOT_FOUND);
    current->media.physical_address=(uintptr_t)archive;current->media.bytes=sizeof(archive);current->files_enabled=1;
    current->protocols[1].handle=42;current->protocol_next_handle=42;
    unsigned char path[28]={4,4,24,0};
    for (size_t i=0;names[1][i];i++) path[4+i*2]=(unsigned char)names[1][i];
    path[24]=0x7f;path[25]=0xff;path[26]=4;
    assert(load_image(0,1,path,NULL,0,&child)==0 && child>42);
    assert(handle(child,&guid,(void **)&info)==0 && info->device_handle==42 && info->file_path);
    assert(!memcmp((void *)(uintptr_t)info->file_path,path,sizeof(path)));
    memset(archive,0,sizeof(archive));memset(path,0,sizeof(path));
    assert(start_image(child,NULL,NULL)==PWL_EFI_NOT_FOUND && current->memory.count==1);
    current->files_enabled=0;current->protocols[1]=(pwl_resident_protocol_t){0};
    return_program(file,0);uint64_t loaded[PWL_RESIDENT_APPLICATIONS];
    for (size_t i=0;i<PWL_RESIDENT_APPLICATIONS;i++) assert(load_image(0,1,NULL,file,sizeof(file),&loaded[i])==0);
    child=UINT64_MAX;
    assert(load_image(0,1,NULL,file,sizeof(file),&child)==PWL_EFI_OUT_OF_RESOURCES && child==UINT64_MAX);
    for (size_t i=0;i<PWL_RESIDENT_APPLICATIONS;i++) assert(unload_image(loaded[i])==0);
    assert(current->memory.count==1);
    /* Loaded applications can be discarded through Exit before StartImage. */
    return_program(file,0);assert(load_image(0,1,NULL,file,sizeof(file),&child)==0);
    assert(exit_image(child,PWL_EFI_NOT_FOUND,0,NULL)==0 && current->memory.count==1);
    assert(load_image(0,1,NULL,file,sizeof(file),&child)==0);deny_mapping=2;
    assert(unload_image(child)==PWL_EFI_DEVICE_ERROR);
    assert(start_image(child,NULL,NULL)==PWL_EFI_DEVICE_ERROR);deny_mapping=0;
    /* Quarantined allocations deliberately remain reserved until outer teardown. */
    current->memory.exited=1;
    assert(load_image(0,1,NULL,file,sizeof(file),&child)==PWL_EFI_ACCESS_DENIED);
    assert(start_image(child,NULL,NULL)==PWL_EFI_ACCESS_DENIED);
    assert(munmap(heap,1024*1024)==0);resident_test_close(&t);alarm(0);
    puts("resident images: real PE execution, Exit jump, nested starts, permissions, rollback and quarantine passed");
}
