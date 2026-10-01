#include "pwl_efi_entry.h"
#include "acpi_internal.h"
pwl_status_t pwl_native_acpi_publish(pwl_native_workspace_t *w,
    const pwl_resident_image_t *image,const pwl_native_transition_plan_t *plan,
    const pwl_x64_table_page_t *tables,const pwl_acpi_source_t *source,
    const pwl_acpi_snapshot_t *snapshot)
{
    pwl_efi_entry_context_t checked;
    if(pwl_native_efi_entry_prepare(w,image,plan,tables,&checked)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    pwl_native_data_t *d=w->data.prepare_address;
    if(w->acpi.count || d->tpl!=4 || d->current_application || d->memory.key==UINT64_MAX ||
       !memory_valid(&d->memory))return PWL_ERR_ACCESS_DENIED;
    pwl_status_t status=pwl_acpi_snapshot_recheck(source,snapshot);
    if(status!=PWL_OK)return status;
    status=pwl_acpi_mappings_validate(snapshot,tables,plan->table_count,plan->root);
    if(status!=PWL_OK)return status;
    pwl_x64_alias_range_t ranges[2*PWL_ACPI_MAX_TABLES];size_t count;
    status=pwl_acpi_ranges(snapshot,ranges,2*PWL_ACPI_MAX_TABLES,&count);
    if(status!=PWL_OK)return status;
    if(count>PWL_FW_MAX_DESCRIPTORS-d->memory.count)return PWL_ERR_OUT_OF_RESOURCES;
    for(size_t i=0;i<count;i++) {
        uint64_t a=ranges[i].physical_address,end=a+ranges[i].bytes;
        /* Neither the arena, its unadvertised tail, nor active table storage
         * may masquerade as external firmware-owned ACPI memory. */
        if((a<w->arena.physical_address+w->arena.size && w->arena.physical_address<end) ||
           (a<plan->root+plan->table_count*4096 && plan->root<end))return PWL_ERR_BAD_IMAGE;
        for(size_t j=0;j<d->memory.count;j++) {
            const pwl_efi_memory_descriptor_t *m=&d->memory.entries[j].descriptor;
            if(a<m->physical_start+m->number_of_pages*4096 && m->physical_start<end)
                return PWL_ERR_ACCESS_DENIED;
        }
    }
    /* All refusal paths precede these writes. Keeping each mapping range a
     * distinct NVS descriptor is valid even when adjacent pages share type. */
    pwl_acpi_snapshot_t captured=*snapshot;
    for(size_t i=0;i<count;i++) {
        size_t at=0;
        while(at<d->memory.count && d->memory.entries[at].descriptor.physical_start<ranges[i].physical_address)at++;
        for(size_t j=d->memory.count;j>at;j--)d->memory.entries[j]=d->memory.entries[j-1];
        d->memory.entries[at]=(pwl_fw_memory_entry_t){
            {10,0,ranges[i].physical_address,0,ranges[i].bytes/4096,8},0};
        d->memory.count++;
    }
    d->memory.key++;d->memory.issued_key=0;
    w->acpi=captured;
    d->configuration[0]=(pwl_efi_configuration_t){acpi_guid(&captured),captured.tables[0].physical_address};
    d->efi.system.configuration_count=1;
    d->efi.system.configuration_tables=w->data.physical_address+offsetof(pwl_native_data_t,configuration);
    d->efi.system.header.crc32=0;
    d->efi.system.header.crc32=pwl_efi_crc32(&d->efi.system,sizeof(d->efi.system));
    return PWL_OK;
}
