#ifndef PWL_ACPI_INTERNAL_H
#define PWL_ACPI_INTERNAL_H
#include "pwl_native_workspace.h"
static inline pwl_efi_guid_t acpi_guid(const pwl_acpi_snapshot_t *s)
{
    const pwl_efi_guid_t v1={{0x30,0x2d,0x9d,0xeb,0x88,0x2d,0xd3,0x11,0x9a,0x16,0,0x90,0x27,0x3f,0xc1,0x4d}};
    const pwl_efi_guid_t v2={{0x71,0xe8,0x68,0x88,0xf1,0xe4,0xd3,0x11,0xbc,0x22,0,0x80,0xc7,0x3c,0x88,0x81}};
    return s->tables[0].bytes==20?v1:v2;
}
static inline int memory_valid(const pwl_fw_memory_t *m)
{
    if(!m->count || m->count>PWL_FW_MAX_DESCRIPTORS || !m->key || m->exited)return 0;
    uint64_t end=0;
    for(size_t i=0;i<m->count;i++) {
        const pwl_efi_memory_descriptor_t *d=&m->entries[i].descriptor;
        if(d->physical_start%4096 || !d->number_of_pages || d->physical_start<end ||
           d->physical_start>=(UINT64_C(1)<<47) ||
           d->number_of_pages>((UINT64_C(1)<<47)-d->physical_start)/4096)return 0;
        end=d->physical_start+d->number_of_pages*4096;
    }
    return 1;
}
#endif
