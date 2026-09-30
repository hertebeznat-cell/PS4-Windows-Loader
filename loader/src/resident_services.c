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
    *entry=(pwl_resident_protocol_t){0};
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
