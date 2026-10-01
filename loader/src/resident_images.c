#include "pwl_resident.h"
#define EFI __attribute__((ms_abi))
extern const uint64_t pwl_resident_binding __attribute__((visibility("hidden")));
extern int pwl_image_save(pwl_image_jump_t *) __attribute__((returns_twice,visibility("hidden")));
extern void pwl_image_resume(pwl_image_jump_t *) __attribute__((noreturn,visibility("hidden")));
extern uint64_t EFI pwl_resident_install_protocol(uint64_t *,const pwl_efi_guid_t *,unsigned,void *);
static const pwl_efi_guid_t loaded_guid={{0xa1,0x31,0x1b,0x5b,0x62,0x95,0xd2,0x11,
    0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}};
static const pwl_efi_guid_t path_guid={{0x7e,0x15,0x62,0xbc,0x33,0x3e,0xec,0x4f,
    0x99,0x20,0x2d,0x3b,0x36,0xd7,0x50,0xdf}};
static pwl_resident_data_t *state(void)
{ return (pwl_resident_data_t *)(uintptr_t)pwl_resident_binding; }
static int equal(const pwl_efi_guid_t *a,const pwl_efi_guid_t *b)
{ for (size_t i=0;i<16;i++) if (a->bytes[i]!=b->bytes[i]) return 0;return 1; }
static pwl_resident_application_t *application(pwl_resident_data_t *d,uint64_t handle)
{
    if (handle && d->initial_application.handle==handle) return &d->initial_application;
    for (size_t i=0;i<PWL_RESIDENT_APPLICATIONS;i++) {
        pwl_resident_application_t *a=(void *)(uintptr_t)d->applications[i];
        if (a && a->handle==handle) return a;
    }
    return NULL;
}
static int parent_known(const pwl_resident_data_t *d,uint64_t parent)
{
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++)
        if (d->protocols[i].handle==parent && equal(&d->protocols[i].guid,&loaded_guid)) return 1;
    return 0;
}
static uint64_t map_image(pwl_resident_data_t *d,const pwl_pe_loaded_t *image,unsigned restore)
{
    if (!d->image_mapping.permission_callback) return PWL_EFI_UNSUPPORTED;
    typedef uint64_t (EFI *permission_t)(pwl_resident_data_t *,const pwl_pe_loaded_t *,unsigned);
    permission_t fn=(permission_t)(uintptr_t)d->image_mapping.permission_callback;
    return fn(d,image,restore);
}
/* A bounded, single MEDIA_FILEPATH_DP short form for our sole resident volume.
 * Hardware and multi-instance device paths need the future device model.
 */
static uint64_t decode_path(const void *path,unsigned char copy[520],uint16_t name[PWL_FILES_PATH])
{
    if (!path) return PWL_EFI_SUCCESS;
    const unsigned char *p=path;
    if (p[0]!=4 || p[1]!=4) return PWL_EFI_UNSUPPORTED;
    size_t n=(size_t)p[2]+((size_t)p[3]<<8);
    if (n<8 || n>516 || (n&1) || p[n]!=0x7f || p[n+1]!=0xff || p[n+2]!=4 || p[n+3])
        return PWL_EFI_INVALID_PARAMETER;
    size_t units=(n-4)/2;
    if (units>PWL_FILES_PATH || p[4]!='\\' || p[5]) return PWL_EFI_INVALID_PARAMETER;
    for (size_t i=0;i<units;i++) {
        name[i]=(uint16_t)p[4+2*i]|((uint16_t)p[5+2*i]<<8);
        if ((i==units-1)!=(name[i]==0)) return PWL_EFI_INVALID_PARAMETER;
    }
    for (size_t i=0;i<n+4;i++) copy[i]=p[i];
    return PWL_EFI_SUCCESS;
}
static uint64_t pe_status(pwl_status_t s)
{
    if (s==PWL_ERR_UNSUPPORTED) return PWL_EFI_UNSUPPORTED;
    if (s==PWL_ERR_OUT_OF_RESOURCES) return PWL_EFI_OUT_OF_RESOURCES;
    return PWL_EFI_ERROR(1); /* EFI_LOAD_ERROR. */
}
static uint64_t destroy(pwl_resident_data_t *d,pwl_resident_application_t *a)
{
    uint64_t handle=a->handle;
    uint64_t status=PWL_EFI_SUCCESS;
    if (a->initial) {
        /* The preparation arena owns the initial PE span. Keep its reserved
         * pages and RX bytes until the outer owner releases the entire arena. */
        *a=(pwl_resident_application_t){0};
        goto forget;
    }
    status=map_image(d,&a->mapped,1);
    if (status) { a->quarantined=1;return status; }
    /* Remove executable mappings before making pages available again. */
    unsigned char *p=(void *)(uintptr_t)a->mapped.physical_address;
    for (uint64_t i=0;i<a->mapped.image_size;i++) p[i]=0;
    status=pwl_fw_free_pages(&d->memory,a->mapped.physical_address,a->mapped.image_size/4096);
    if (status) { a->quarantined=1;return status; }
    uint64_t pool=(uintptr_t)a;
    status=pwl_fw_free_pool(&d->memory,pool);
    if (status) { a->quarantined=1;return status; }
    for (size_t i=0;i<PWL_RESIDENT_APPLICATIONS;i++)
        if (d->applications[i]==pool) d->applications[i]=0;
forget:
    /* Applications automatically close their query-mode protocol opens and
     * remove protocols installed on their own handle when they finish. */
    for (size_t i=0;i<PWL_RESIDENT_OPENS;i++) {
        pwl_resident_open_t *o=&d->opens[i];
        if (o->count && (o->agent==handle || d->protocols[o->protocol_index].handle==handle)) o->count=0;
    }
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++)
        if (d->protocols[i].handle==handle) d->protocols[i]=(pwl_resident_protocol_t){0};
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_load_image(unsigned char policy,uint64_t parent,
    const void *path,const void *source,size_t bytes,uint64_t *out)
{
    pwl_resident_data_t *d=state();
    if (!d || !out || !parent || policy>1 || (!source && !path)) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    if (!parent_known(d,parent)) return PWL_EFI_INVALID_PARAMETER;
    if (d->tpl!=4 || !d->image_mapping.permission_callback) return PWL_EFI_UNSUPPORTED;
    unsigned char copied[520]={0};uint16_t name[PWL_FILES_PATH]={0};
    uint64_t status=decode_path(path,copied,name);
    if (status) return status;
    if (!source) {
        if (!d->files_enabled) return PWL_EFI_NOT_FOUND;
        pwl_file_view_t file;
        status=pwl_files_open((void *)(uintptr_t)d->media.physical_address,(size_t)d->media.bytes,name,&file);
        if (status) return status;
        if (file.directory || !file.size) return PWL_EFI_ERROR(1);
        source=(void *)(uintptr_t)(d->media.physical_address+file.offset);bytes=(size_t)file.size;
    } else if (!bytes) return PWL_EFI_INVALID_PARAMETER;
    /* For a buffer source BootPolicy is ignored, as specified. */
    size_t slot=0,free_protocols=0;
    while (slot<PWL_RESIDENT_APPLICATIONS && d->applications[slot]) slot++;
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++) if (!d->protocols[i].handle) free_protocols++;
    if (slot==PWL_RESIDENT_APPLICATIONS || free_protocols<2 || d->protocol_next_handle==UINT64_MAX)
        return PWL_EFI_OUT_OF_RESOURCES;
    uint64_t size=0,pa=0,pool=0;
    pwl_status_t parsed=pwl_pe_efi_size(source,bytes,&size);
    if (parsed!=PWL_OK) return pe_status(parsed);
    status=pwl_fw_allocate_pool(&d->memory,4,sizeof(pwl_resident_application_t),&pool);
    if (status) return status;
    pwl_resident_application_t *a=(void *)(uintptr_t)pool;
    *a=(pwl_resident_application_t){0};
    status=pwl_fw_allocate_pages(&d->memory,PWL_ALLOCATE_ANY,1,size/4096,&pa);
    if (status) goto fail_pool;
    /* Source and writable destination must be disjoint. PE loader enforces it. */
    parsed=pwl_pe_load_efi(source,bytes,(void *)(uintptr_t)pa,(size_t)size,pa,&a->mapped);
    if (parsed!=PWL_OK) { status=pe_status(parsed);goto fail_pages; }
    status=map_image(d,&a->mapped,0);
    if (status) goto fail_pages; /* Permission callback is transactional. */
    a->protocol.revision=0x1000;a->protocol.parent_handle=parent;
    a->protocol.system_table=(uintptr_t)&d->efi.system;
    a->protocol.image_base=pa;a->protocol.image_size=size;
    /* The complete allocation is EfiLoaderCode; data pages are writable NX. */
    a->protocol.image_code_type=1;a->protocol.image_data_type=1;
    if (path) {
        for (size_t i=0;i<sizeof(copied);i++) a->path[i]=copied[i];
        a->protocol.file_path=(uintptr_t)a->path;
        if (d->files_enabled) a->protocol.device_handle=d->protocols[1].handle;
    }
    status=pwl_resident_install_protocol(&a->handle,&loaded_guid,0,&a->protocol);
    if (!status) status=pwl_resident_install_protocol(&a->handle,&path_guid,0,path ? a->path : NULL);
    if (status) {
        /* Reservations above make publication failure impossible for a
         * serialized registry. Preserve owner if state was nevertheless changed. */
        a->quarantined=1;d->applications[slot]=pool;return PWL_EFI_DEVICE_ERROR;
    }
    d->applications[slot]=pool;*out=a->handle;
    return PWL_EFI_SUCCESS;
fail_pages:
    {
        uint64_t cleanup=pwl_fw_free_pages(&d->memory,pa,size/4096);
        if (cleanup) { a->quarantined=1;d->applications[slot]=pool;return cleanup; }
    }
fail_pool:
    {
        uint64_t cleanup=pwl_fw_free_pool(&d->memory,pool);
        return cleanup ? cleanup : status;
    }
}
uint64_t EFI pwl_resident_unload_image(uint64_t handle)
{
    pwl_resident_data_t *d=state();
    if (!d) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    pwl_resident_application_t *a=application(d,handle);
    if (!a) return PWL_EFI_INVALID_PARAMETER;
    if (a->running || d->tpl!=4) return PWL_EFI_UNSUPPORTED;
    if (a->quarantined) return PWL_EFI_DEVICE_ERROR;
    uint64_t ready=map_image(d,&a->mapped,2);
    if (ready) return ready;
    return destroy(d,a);
}
uint64_t EFI pwl_resident_exit_image(uint64_t handle,uint64_t status,size_t bytes,const uint16_t *data)
{
    pwl_resident_data_t *d=state();
    if (!d) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    pwl_resident_application_t *a=application(d,handle);
    if (!a || a->quarantined) return PWL_EFI_INVALID_PARAMETER;
    if (!a->running) return destroy(d,a);
    if (d->current_application!=handle) return PWL_EFI_INVALID_PARAMETER;
    uint64_t copy=0;
    if (status!=PWL_EFI_SUCCESS && bytes) {
        if (!data || bytes<2) return PWL_EFI_INVALID_PARAMETER;
        if (bytes>65536) return PWL_EFI_OUT_OF_RESOURCES;
        size_t i;
        for (i=0;i<bytes/2;i++) if (!data[i]) break;
        if (i==bytes/2) return PWL_EFI_INVALID_PARAMETER;
        uint64_t result=pwl_fw_allocate_pool(&d->memory,4,bytes,&copy);
        if (result) return result;
        unsigned char *dst=(void *)(uintptr_t)copy;const unsigned char *src=(const void *)data;
        for (i=0;i<bytes;i++) dst[i]=src[i];
    }
    a->exit_status=status;a->exit_data=copy;a->exit_bytes=copy ? bytes : 0;
    d->tpl=4;
    pwl_image_resume(&a->jump);
}
uint64_t EFI pwl_resident_start_image(uint64_t handle,size_t *bytes,uint16_t **data)
{
    pwl_resident_data_t *d=state();
    if (!d) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    pwl_resident_application_t *a=application(d,handle);
    if (!a) return PWL_EFI_INVALID_PARAMETER;
    if (a->running || d->tpl!=4) return PWL_EFI_INVALID_PARAMETER;
    if (a->quarantined) return PWL_EFI_DEVICE_ERROR;
    uint64_t ready=map_image(d,&a->mapped,2);
    if (ready) return ready;
    if (bytes) *bytes=0;
    if (data) *data=NULL;
    a->previous=d->current_application;a->running=1;d->current_application=handle;
    if (!pwl_image_save(&a->jump)) {
        typedef uint64_t (EFI *entry_t)(uint64_t,pwl_efi_system_table_t *);
        entry_t entry=(entry_t)(uintptr_t)a->mapped.entry_address;
        a->exit_status=entry(handle,&d->efi.system);
    }
    /* StartImage is the destination of both normal return and Exit's jump.
     * Failure does not silently claim a clean unload. */
    uint64_t result=a->exit_status,copy=a->exit_data;size_t n=a->exit_bytes;
    a->running=0;d->current_application=a->previous;d->tpl=4;
    uint64_t cleanup=destroy(d,a);
    if (copy && bytes && data) { *bytes=n;*data=(void *)(uintptr_t)copy; }
    else if (copy) {
        uint64_t freed=pwl_fw_free_pool(&d->memory,copy);
        if (freed && !cleanup) cleanup=freed;
    }
    pwl_resident_events_dispatch(d);
    return cleanup ? cleanup : result;
}
uint64_t EFI pwl_resident_boot_entry(uint64_t handle,pwl_efi_system_table_t *table)
{
    pwl_resident_data_t *d=state();
    if (!d || table!=&d->efi.system || !d->initial_application.initial ||
        handle!=d->initial_application.handle) return PWL_EFI_INVALID_PARAMETER;
    return pwl_resident_start_image(handle,NULL,NULL);
}
