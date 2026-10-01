#include "acpi_internal.h"
pwl_status_t pwl_native_acpi_validate(const pwl_native_workspace_t *w)
{
    if(!w || !w->data.prepare_address)return PWL_ERR_INVALID_ARGUMENT;
    const pwl_native_data_t *d=w->data.prepare_address;
    if(!w->acpi.count) {
        if(w->acpi.storage || w->acpi.bytes || d->efi.system.configuration_count ||
           d->efi.system.configuration_tables)return PWL_ERR_BAD_IMAGE;
    } else {
        if(pwl_acpi_snapshot_validate(&w->acpi)!=PWL_OK || !memory_valid(&d->memory) ||
           d->efi.system.configuration_count!=1 ||
           d->efi.system.configuration_tables!=w->data.physical_address+offsetof(pwl_native_data_t,configuration) ||
           d->configuration[0].table!=w->acpi.tables[0].physical_address)return PWL_ERR_BAD_IMAGE;
        pwl_efi_guid_t guid=acpi_guid(&w->acpi);
        for(size_t i=0;i<16;i++)if(d->configuration[0].guid.bytes[i]!=guid.bytes[i])return PWL_ERR_BAD_IMAGE;
        for(size_t i=0;i<w->acpi.count;i++) {
            uint64_t a=w->acpi.tables[i].physical_address&~UINT64_C(4095);
            uint64_t end=(w->acpi.tables[i].physical_address+w->acpi.tables[i].bytes+4095)&~UINT64_C(4095);
            for(size_t j=0;j<d->memory.count && a<end;j++) {
                const pwl_fw_memory_entry_t *entry=&d->memory.entries[j];
                const pwl_efi_memory_descriptor_t *m=&entry->descriptor;
                uint64_t b=m->physical_start+m->number_of_pages*4096;
                if(a<m->physical_start)break;
                if(a<b) {
                    if(m->type!=10 || entry->allocated || (m->attribute&31)!=8 ||
                       m->virtual_start || m->padding)return PWL_ERR_BAD_IMAGE;
                    a=b<end?b:end;
                }
            }
            if(a!=end)return PWL_ERR_BAD_IMAGE;
        }
    }
    const unsigned char *unused=(const void *)&d->configuration[w->acpi.count?1:0];
    size_t bytes=(PWL_RESIDENT_CONFIGURATIONS-(w->acpi.count?1:0))*sizeof(d->configuration[0]);
    for(size_t i=0;i<bytes;i++)if(unused[i])return PWL_ERR_BAD_IMAGE;
    return PWL_OK;
}
