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
