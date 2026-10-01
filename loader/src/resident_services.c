#include "pwl_resident.h"

#if !defined(__x86_64__) || (!defined(__GNUC__) && !defined(__clang__))
#error "Resident EFI requires AMD64 and Microsoft x64 ABI support"
#endif
#define EFI __attribute__((ms_abi))
/* RIP-relative read from an eight-byte read-only slot. Preparation patches the
 * copied slot to the destination data PA; no process pointers survive entry.
 */
extern const uint64_t pwl_resident_binding __attribute__((visibility("hidden")));
static pwl_resident_data_t *state(void)
{
    return (pwl_resident_data_t *)(uintptr_t)pwl_resident_binding;
}

uint64_t EFI pwl_resident_raise_tpl(uint64_t tpl)
{
    pwl_resident_data_t *d=state();
    if (!d) return 0;
    uint64_t old=d->tpl;
    if ((tpl==4 || tpl==8 || tpl==16 || tpl==31) && tpl>=old) d->tpl=tpl;
    return old;
}
void EFI pwl_resident_restore_tpl(uint64_t tpl)
{
    pwl_resident_data_t *d=state();
    if (d && (tpl==4 || tpl==8 || tpl==16 || tpl==31) && tpl<=d->tpl) d->tpl=tpl;
}
uint64_t EFI pwl_resident_allocate_pages(unsigned kind,unsigned type,uint64_t pages,uint64_t *pa)
{
    pwl_resident_data_t *d=state();
    return pwl_fw_allocate_pages(d ? &d->memory : NULL,kind,type,pages,pa);
}
uint64_t EFI pwl_resident_free_pages(uint64_t pa,uint64_t pages)
{
    pwl_resident_data_t *d=state();
    return pwl_fw_free_pages(d ? &d->memory : NULL,pa,pages);
}
uint64_t EFI pwl_resident_allocate_pool(unsigned type,size_t bytes,void **buffer)
{
    if (!buffer) return PWL_EFI_INVALID_PARAMETER;
    pwl_resident_data_t *d=state();
    uint64_t address=0;
    uint64_t status=pwl_fw_allocate_pool(d ? &d->memory : NULL,type,bytes,&address);
    if (status==PWL_EFI_SUCCESS) *buffer=(void *)(uintptr_t)address;
    return status;
}
uint64_t EFI pwl_resident_free_pool(void *buffer)
{
    pwl_resident_data_t *d=state();
    return pwl_fw_free_pool(d ? &d->memory : NULL,(uint64_t)(uintptr_t)buffer);
}
uint64_t EFI pwl_resident_get_memory_map(size_t *size,pwl_efi_memory_descriptor_t *map,
                                       uint64_t *key,size_t *ds,uint32_t *version)
{
    pwl_resident_data_t *d=state();
    return pwl_fw_get_memory_map(d ? &d->memory : NULL,size,map,key,ds,version);
}
uint64_t EFI pwl_resident_exit_boot_services(uint64_t image,uint64_t key)
{
    /* Memory retirement alone is not ExitBootServices. Keep state intact until
     * platform event/device/CPU handoff and table updates are implemented.
     */
    (void)image; (void)key;
    return PWL_EFI_UNSUPPORTED;
}
uint64_t EFI pwl_resident_calculate_crc32(const void *bytes,size_t size,uint32_t *crc)
{
    if (!bytes || !size || !crc) return PWL_EFI_INVALID_PARAMETER;
    *crc=pwl_efi_crc32(bytes,size);
    return PWL_EFI_SUCCESS;
}
void EFI pwl_resident_copy_mem(void *to,const void *from,size_t size)
{
    unsigned char *d=to;
    const unsigned char *s=from;
    if ((uintptr_t)d>(uintptr_t)s && (uintptr_t)d-(uintptr_t)s<size)
        for (size_t i=size;i;i--) d[i-1]=s[i-1];
    else
        for (size_t i=0;i<size;i++) d[i]=s[i];
}
void EFI pwl_resident_set_mem(void *to,size_t size,unsigned char value)
{
    unsigned char *d=to;
    for (size_t i=0;i<size;i++) d[i]=value;
}

/* Serialized protocol registry. Handles are opaque values, never dereferenced.
 * Driver ownership/open tracking and notification events are not implemented;
 * no OpenProtocol entry is published by this registry. */
static int guid_equal(const pwl_efi_guid_t *a,const pwl_efi_guid_t *b)
{
    for (size_t i=0;i<16;i++) if (a->bytes[i]!=b->bytes[i]) return 0;
    return 1;
}
static int handle_known(const pwl_resident_data_t *d,uint64_t handle)
{
    if (!handle) return 0;
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++)
        if (d->protocols[i].handle==handle) return 1;
    return 0;
}
static pwl_resident_protocol_t *find_protocol(pwl_resident_data_t *d,
    uint64_t handle,const pwl_efi_guid_t *guid)
{
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++)
        if (d->protocols[i].handle==handle && guid_equal(&d->protocols[i].guid,guid))
            return &d->protocols[i];
    return NULL;
}
static void forget_opens(pwl_resident_data_t *d,const pwl_resident_protocol_t *entry)
{
    uint32_t index=(uint32_t)(entry-d->protocols);
    for (size_t i=0;i<PWL_RESIDENT_OPENS;i++)
        if (d->opens[i].count && d->opens[i].protocol_index==index) d->opens[i].count=0;
}
uint64_t EFI pwl_resident_install_protocol(uint64_t *handle,const pwl_efi_guid_t *guid,
    unsigned type,void *interface)
{
    pwl_resident_data_t *d=state();
    if (!d || !handle || !guid || type) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    if (*handle && (!handle_known(d,*handle) || find_protocol(d,*handle,guid)))
        return PWL_EFI_INVALID_PARAMETER;
    if (!*handle && d->protocol_next_handle==UINT64_MAX)
        return PWL_EFI_OUT_OF_RESOURCES;
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++) {
        pwl_resident_protocol_t *entry=&d->protocols[i];
        if (entry->handle) continue;
        /* Snapshot GUID before assigning in case it aliases this entry. */
        pwl_efi_guid_t copy=*guid;
        uint64_t value=*handle;
        if (!value) value=++d->protocol_next_handle;
        *entry=(pwl_resident_protocol_t){value,(uint64_t)(uintptr_t)interface,copy};
        *handle=value;
        return PWL_EFI_SUCCESS;
    }
    return PWL_EFI_OUT_OF_RESOURCES;
}
uint64_t EFI pwl_resident_reinstall_protocol(uint64_t handle,const pwl_efi_guid_t *guid,
    void *old_interface,void *new_interface)
{
    pwl_resident_data_t *d=state();
    if (!d || !guid || !handle_known(d,handle)) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    pwl_resident_protocol_t *entry=find_protocol(d,handle,guid);
    if (!entry || entry->interface_address!=(uint64_t)(uintptr_t)old_interface)
        return PWL_EFI_NOT_FOUND;
    forget_opens(d,entry);
    entry->interface_address=(uint64_t)(uintptr_t)new_interface;
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_uninstall_protocol(uint64_t handle,const pwl_efi_guid_t *guid,
    void *interface)
{
    pwl_resident_data_t *d=state();
    if (!d || !guid || !handle_known(d,handle)) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    pwl_resident_protocol_t *entry=find_protocol(d,handle,guid);
    if (!entry || entry->interface_address!=(uint64_t)(uintptr_t)interface)
        return PWL_EFI_NOT_FOUND;
    forget_opens(d,entry);
    *entry=(pwl_resident_protocol_t){0};
    if (!handle_known(d,handle))
        for (size_t i=0;i<PWL_RESIDENT_OPENS;i++)
            if (d->opens[i].agent==handle) d->opens[i].count=0;
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_handle_protocol(uint64_t handle,const pwl_efi_guid_t *guid,
    void **interface)
{
    pwl_resident_data_t *d=state();
    if (!d || !guid || !interface || !handle_known(d,handle))
        return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    pwl_resident_protocol_t *entry=find_protocol(d,handle,guid);
    if (!entry) return PWL_EFI_UNSUPPORTED;
    *interface=(void *)(uintptr_t)entry->interface_address;
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_locate_protocol(const pwl_efi_guid_t *guid,void *registration,
    void **interface)
{
    pwl_resident_data_t *d=state();
    if (!d || !guid || !interface) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    if (registration) return PWL_EFI_UNSUPPORTED;
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++) {
        pwl_resident_protocol_t *entry=&d->protocols[i];
        if (entry->handle && guid_equal(&entry->guid,guid)) {
            *interface=(void *)(uintptr_t)entry->interface_address;
            return PWL_EFI_SUCCESS;
        }
    }
    return PWL_EFI_NOT_FOUND;
}
uint64_t EFI pwl_resident_locate_handle(unsigned search,const pwl_efi_guid_t *guid,
    void *key,size_t *size,uint64_t *buffer)
{
    pwl_resident_data_t *d=state();
    if (!d || search>2 || (search==2 && !guid) || (search==1 && !key))
        return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    if (search==1) return PWL_EFI_UNSUPPORTED;
    uint64_t matches[PWL_RESIDENT_PROTOCOLS];
    size_t count=0;
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++) {
        pwl_resident_protocol_t *entry=&d->protocols[i];
        if (!entry->handle || (search==2 && !guid_equal(&entry->guid,guid))) continue;
        size_t j;
        for (j=0;j<count;j++) if (matches[j]==entry->handle) break;
        if (j==count) matches[count++]=entry->handle;
    }
    if (!count) return PWL_EFI_NOT_FOUND;
    if (!size) return PWL_EFI_INVALID_PARAMETER;
    size_t needed=count*sizeof(*buffer);
    if (*size<needed) { *size=needed;return PWL_EFI_BUFFER_TOO_SMALL; }
    if (!buffer) return PWL_EFI_INVALID_PARAMETER;
    for (size_t i=0;i<count;i++) buffer[i]=matches[i];
    *size=needed;
    return PWL_EFI_SUCCESS;
}

/* EFI File Protocol revision 1, over the validated immutable file archive. */
static pwl_resident_file_t *file_handle(pwl_resident_data_t *d,void *self)
{
    if (!d || !d->files_enabled || d->memory.exited) return NULL;
    for (size_t i=0;i<PWL_RESIDENT_FILES;i++)
        if (self==&d->files[i].protocol && d->files[i].active) return &d->files[i];
    return NULL;
}
static const void *file_archive(const pwl_resident_data_t *d)
{ return (const void *)(uintptr_t)d->media.physical_address; }
static uint64_t new_file(pwl_resident_data_t *d,const pwl_file_view_t *view,void **out)
{
    if (!out) return PWL_EFI_INVALID_PARAMETER;
    for (size_t i=0;i<PWL_RESIDENT_FILES;i++) {
        pwl_resident_file_t *f=&d->files[i];
        if (f->active) continue;
        *f=(pwl_resident_file_t){d->file_template,*view,0,1};
        *out=&f->protocol;return PWL_EFI_SUCCESS;
    }
    return PWL_EFI_OUT_OF_RESOURCES;
}
uint64_t EFI pwl_resident_open_volume(void *self,void **root)
{
    pwl_resident_data_t *d=state();
    if (!d || !d->files_enabled || self!=d->filesystem || !root)
        return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    const uint16_t path[2]={'\\',0};pwl_file_view_t view;
    uint64_t status=pwl_files_open(file_archive(d),(size_t)d->media.bytes,path,&view);
    return status ? status : new_file(d,&view,root);
}
static uint64_t resolve_file_path(pwl_resident_data_t *d,pwl_resident_file_t *f,
    const uint16_t *name,uint16_t result[PWL_FILES_PATH])
{
    uint16_t combined[PWL_FILES_PATH];size_t n=0;
    if (!name) return PWL_EFI_INVALID_PARAMETER;
    if (name[0]!='\\') {
        pwl_file_view_t ignored;
        uint64_t status=pwl_files_at(file_archive(d),(size_t)d->media.bytes,f->view.index,
                                     &ignored,combined);
        if (status) return status;
        while (combined[n]) n++;
        if (n>1) { if (n+1>=PWL_FILES_PATH) return PWL_EFI_INVALID_PARAMETER;combined[n++]='\\'; }
    }
    for (size_t i=0;;i++) {
        if (i>=PWL_FILES_PATH || n>=PWL_FILES_PATH) return PWL_EFI_INVALID_PARAMETER;
        uint16_t c=name[i];combined[n++]=c;if (!c) break;
    }
    if (combined[0]!='\\') return PWL_EFI_INVALID_PARAMETER;
    size_t written=1,start=1;result[0]='\\';
    while (combined[start]) {
        while (combined[start]=='\\') start++;
        if (!combined[start]) break;
        size_t end=start;
        while (combined[end] && combined[end]!='\\') end++;
        size_t length=end-start;
        if (length==1 && combined[start]=='.') { start=end;continue; }
        if (length==2 && combined[start]=='.' && combined[start+1]=='.') {
            if (written==1) return PWL_EFI_ACCESS_DENIED;
            while (written>1 && result[written-1]!='\\') written--;
            if (written>1) written--;
            start=end;continue;
        }
        if (written>1) result[written++]='\\';
        if (length>=PWL_FILES_PATH-written) return PWL_EFI_INVALID_PARAMETER;
        for (size_t i=start;i<end;i++) result[written++]=combined[i];
        start=end;
    }
    result[written]=0;return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_file_open(void *self,void **out,const uint16_t *name,
    uint64_t mode,uint64_t attributes)
{
    (void)attributes;pwl_resident_data_t *d=state();pwl_resident_file_t *f=file_handle(d,self);
    if (!f || !out || !name) return PWL_EFI_INVALID_PARAMETER;
    if (mode!=1) return (mode==3 || mode==UINT64_C(0x8000000000000003)) ?
                        PWL_EFI_WRITE_PROTECTED : PWL_EFI_INVALID_PARAMETER;
    if (!f->view.directory) return PWL_EFI_UNSUPPORTED;
    uint16_t path[PWL_FILES_PATH];uint64_t status=resolve_file_path(d,f,name,path);
    if (status) return status;
    pwl_file_view_t view;
    status=pwl_files_open(file_archive(d),(size_t)d->media.bytes,path,&view);
    return status ? status : new_file(d,&view,out);
}
uint64_t EFI pwl_resident_file_close(void *self)
{
    pwl_resident_file_t *f=file_handle(state(),self);
    if (!f) return PWL_EFI_INVALID_PARAMETER;
    f->active=0;return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_file_delete(void *self)
{
    uint64_t status=pwl_resident_file_close(self);
    return status ? status : UINT64_C(2); /* EFI_WARN_DELETE_FAILURE; handle closed. */
}
static uint64_t file_info(const pwl_file_view_t *view,const uint16_t *path,
    size_t *size,void *buffer)
{
    size_t length=0,start=0;
    while (path[length]) { if (path[length]=='\\') start=length+1;length++; }
    size_t chars=length-start;
    size_t needed=80+(chars+1)*2;
    if (!size) return PWL_EFI_INVALID_PARAMETER;
    if (*size<needed) { *size=needed;return PWL_EFI_BUFFER_TOO_SMALL; }
    if (!buffer) return PWL_EFI_INVALID_PARAMETER;
    unsigned char *p=buffer;
    for (size_t i=0;i<needed;i++) p[i]=0;
    uint64_t fields[3]={needed,view->size,view->size};
    for (size_t i=0;i<3;i++) for (unsigned k=0;k<8;k++) p[i*8+k]=(unsigned char)(fields[i]>>(8*k));
    p[72]=view->directory ? 0x11 : 1; /* DIRECTORY and READ_ONLY. */
    for (size_t i=0;i<chars;i++) { uint16_t c=path[start+i];p[80+i*2]=(unsigned char)c;p[81+i*2]=(unsigned char)(c>>8); }
    *size=needed;return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_file_read(void *self,size_t *size,void *buffer)
{
    pwl_resident_data_t *d=state();pwl_resident_file_t *f=file_handle(d,self);
    if (!f || !size || (!buffer && *size)) return PWL_EFI_INVALID_PARAMETER;
    if (!f->view.directory)
        return pwl_files_read(file_archive(d),(size_t)d->media.bytes,&f->view,&f->position,size,buffer);
    uint16_t parent[PWL_FILES_PATH],path[PWL_FILES_PATH];pwl_file_view_t ignored,view;
    uint64_t status=pwl_files_at(file_archive(d),(size_t)d->media.bytes,f->view.index,&ignored,parent);
    if (status) return status;
    size_t prefix=0;while (parent[prefix]) prefix++;
    if (prefix==1) prefix=0;
    for (uint64_t i=f->position;i<PWL_FILES_MAX;i++) {
        status=pwl_files_at(file_archive(d),(size_t)d->media.bytes,(uint32_t)i,&view,path);
        if (status==PWL_EFI_NOT_FOUND) break;
        if (status) return status;
        size_t k=0;
        while (k<prefix) {
            uint16_t a=path[k],b=parent[k];
            if (a>='a' && a<='z') a-=32;
            if (b>='a' && b<='z') b-=32;
            if (a!=b) break;
            k++;
        }
        if (k!=prefix || path[k]!='\\' || !path[k+1]) continue;
        k++;while (path[k] && path[k]!='\\') k++;
        if (path[k]) continue;
        status=file_info(&view,path,size,buffer);
        if (!status) f->position=i+1;
        return status;
    }
    *size=0;return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_file_write(void *self,size_t *size,const void *buffer)
{
    (void)buffer;
    return !file_handle(state(),self) || !size ? PWL_EFI_INVALID_PARAMETER : PWL_EFI_WRITE_PROTECTED;
}
uint64_t EFI pwl_resident_file_get_position(void *self,uint64_t *position)
{
    pwl_resident_file_t *f=file_handle(state(),self);
    if (!f || !position) return PWL_EFI_INVALID_PARAMETER;
    if (f->view.directory) return PWL_EFI_UNSUPPORTED;
    *position=f->position;return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_file_set_position(void *self,uint64_t position)
{
    pwl_resident_file_t *f=file_handle(state(),self);
    if (!f) return PWL_EFI_INVALID_PARAMETER;
    if (f->view.directory && position) return PWL_EFI_UNSUPPORTED;
    f->position=position==UINT64_MAX ? f->view.size : position;
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_file_get_info(void *self,const pwl_efi_guid_t *guid,size_t *size,void *buffer)
{
    static const pwl_efi_guid_t file_guid={{0x92,0x6e,0x57,0x09,0x3f,0x6d,0xd2,0x11,
                                          0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
    pwl_resident_data_t *d=state();pwl_resident_file_t *f=file_handle(d,self);
    if (!f || !guid || !size) return PWL_EFI_INVALID_PARAMETER;
    if (!guid_equal(guid,&file_guid)) return PWL_EFI_UNSUPPORTED;
    pwl_file_view_t view;uint16_t path[PWL_FILES_PATH];
    uint64_t status=pwl_files_at(file_archive(d),(size_t)d->media.bytes,f->view.index,&view,path);
    return status ? status : file_info(&view,path,size,buffer);
}
uint64_t EFI pwl_resident_file_set_info(void *self,const pwl_efi_guid_t *guid,size_t size,const void *buffer)
{
    (void)size;
    return !file_handle(state(),self) || !guid || !buffer ? PWL_EFI_INVALID_PARAMETER : PWL_EFI_WRITE_PROTECTED;
}
uint64_t EFI pwl_resident_file_flush(void *self)
{ return file_handle(state(),self) ? PWL_EFI_SUCCESS : PWL_EFI_INVALID_PARAMETER; }

/* Query-only OpenProtocol. Driver/exclusive/controller ownership needs a
 * separate driver model and is explicitly refused, rather than simulated. */
uint64_t EFI pwl_resident_open_protocol(uint64_t handle,const pwl_efi_guid_t *guid,
    void **interface,uint64_t agent,uint64_t controller,uint32_t attributes)
{
    pwl_resident_data_t *d=state();
    if (!d || !guid || !handle_known(d,handle) ||
        (attributes!=4 && !interface) || (agent && !handle_known(d,agent)))
        return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    if (attributes!=1 && attributes!=2 && attributes!=4) {
        if (attributes==8 || attributes==16 || attributes==32 || attributes==48)
            return PWL_EFI_UNSUPPORTED;
        return PWL_EFI_INVALID_PARAMETER;
    }
    if (controller) return PWL_EFI_UNSUPPORTED;
    pwl_resident_protocol_t *entry=find_protocol(d,handle,guid);
    if (!entry) return PWL_EFI_UNSUPPORTED;
    if (attributes==4) return PWL_EFI_SUCCESS; /* TEST: no interface or record. */
    if (agent) {
        uint32_t index=(uint32_t)(entry-d->protocols);
        size_t free_slot=PWL_RESIDENT_OPENS;
        for (size_t i=0;i<PWL_RESIDENT_OPENS;i++) {
            pwl_resident_open_t *open=&d->opens[i];
            if (!open->count) { if (free_slot==PWL_RESIDENT_OPENS) free_slot=i;continue; }
            if (open->protocol_index==index && open->agent==agent && open->attributes==attributes) {
                if (open->count==UINT32_MAX) return PWL_EFI_OUT_OF_RESOURCES;
                open->count++;*interface=(void *)(uintptr_t)entry->interface_address;
                return PWL_EFI_SUCCESS;
            }
        }
        if (free_slot==PWL_RESIDENT_OPENS) return PWL_EFI_OUT_OF_RESOURCES;
        d->opens[free_slot]=(pwl_resident_open_t){agent,index,attributes,1};
    }
    *interface=(void *)(uintptr_t)entry->interface_address;
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_close_protocol(uint64_t handle,const pwl_efi_guid_t *guid,
    uint64_t agent,uint64_t controller)
{
    pwl_resident_data_t *d=state();
    if (!d || !guid || !handle_known(d,handle) || !agent || !handle_known(d,agent))
        return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    if (controller) return PWL_EFI_UNSUPPORTED;
    pwl_resident_protocol_t *entry=find_protocol(d,handle,guid);
    if (!entry) return PWL_EFI_NOT_FOUND;
    uint32_t index=(uint32_t)(entry-d->protocols);int closed=0;
    for (size_t i=0;i<PWL_RESIDENT_OPENS;i++) {
        pwl_resident_open_t *open=&d->opens[i];
        if (open->count && open->protocol_index==index && open->agent==agent) {
            open->count=0;closed=1;
        }
    }
    return closed ? PWL_EFI_SUCCESS : PWL_EFI_NOT_FOUND;
}
