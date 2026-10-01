#include "pwl_efi_entry.h"
pwl_status_t pwl_native_efi_entry_prepare(const pwl_native_workspace_t *w,
 const pwl_resident_image_t *image,const pwl_native_transition_plan_t *plan,
 const pwl_x64_table_page_t *tables,pwl_efi_entry_context_t *out)
{
    size_t i;
    if(!w || !plan || !tables || !out || !w->boot_image.entry_address ||
       !plan->root || !plan->table_count || plan->table_count>PWL_NATIVE_MAX_TABLES ||
       !plan->range_count || plan->range_count>PWL_TRANSITION_MAX_RANGES ||
       plan->root!=tables[0].physical_address ||
       pwl_native_resident_environment_validate(w,image)!=PWL_OK ||
       pwl_x64_alias_tables_validate(plan->ranges,plan->range_count,tables,plan->table_count)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    /* Retain every resident mapping, not merely the application entry page. */
    for(i=0;i<w->mapping_count;i++) {
        const pwl_x64_identity_range_t *r=&w->mappings[i];uint64_t offset;
        for(offset=0;offset<r->size;offset+=PWL_PAGE_SIZE) {
            pwl_x64_translation_t x;
            if(pwl_x64_translate(tables,plan->table_count,plan->root,r->base+offset,&x)!=PWL_OK ||
               x.physical_address!=r->base+offset || x.writable!=r->writable ||
               x.executable!=r->executable || x.user || x.pat_index)
                return PWL_ERR_BAD_IMAGE;
        }
    }
    const pwl_native_data_t *data=w->data.prepare_address;
    if (w->acpi.count && pwl_acpi_mappings_validate(&w->acpi,tables,plan->table_count,plan->root)!=PWL_OK)
        return PWL_ERR_BAD_IMAGE;
    if (data->image_mapping.root &&
        (data->image_mapping.root!=plan->root || data->image_mapping.tables_base!=plan->root ||
         data->image_mapping.tables_bytes!=plan->table_count*4096)) return PWL_ERR_BAD_IMAGE;
    if (!data->image_mapping.root && (data->image_mapping.tables_base || data->image_mapping.tables_bytes))
        return PWL_ERR_BAD_IMAGE;
    if (data->graphics.enabled) {
        uint64_t base=data->graphics.mode.framebuffer&~UINT64_C(4095);
        uint64_t end=data->graphics.mode.framebuffer+data->graphics.mode.framebuffer_bytes;
        for (uint64_t p=base;p<end;p+=4096) {
            pwl_x64_translation_t x;
            if (pwl_x64_translate(tables,plan->table_count,plan->root,p,&x)!=PWL_OK ||
                x.physical_address!=p || !x.writable || x.executable || x.user ||
                x.pat_index!=data->graphics.pat_index) return PWL_ERR_BAD_IMAGE;
        }
    }
    pwl_efi_entry_context_t result={w->firmware.physical_address+image->callbacks[53],
        ((const pwl_native_data_t *)w->data.prepare_address)->memory.image_handle,
        ((const pwl_native_data_t *)w->data.prepare_address)->loaded_image.system_table,0,0};
    *out=result;return PWL_OK;
}

pwl_status_t pwl_native_image_mapping_bind(const pwl_native_workspace_t *w,
 const pwl_resident_image_t *image,const pwl_native_transition_plan_t *plan,
 const pwl_x64_table_page_t *tables)
{
    pwl_efi_entry_context_t checked;
    if (pwl_native_efi_entry_prepare(w,image,plan,tables,&checked)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    for (size_t i=0;i<plan->table_count;i++) {
        if (tables[i].physical_address!=plan->root+i*4096) return PWL_ERR_INVALID_ARGUMENT;
        pwl_x64_translation_t x;
        if (pwl_x64_translate(tables,plan->table_count,plan->root,tables[i].physical_address,&x)!=PWL_OK ||
            x.physical_address!=tables[i].physical_address || !x.writable || x.executable || x.user || x.pat_index)
            return PWL_ERR_BAD_IMAGE;
    }
    pwl_native_data_t *d=w->data.prepare_address;
    d->image_mapping.root=plan->root;
    d->image_mapping.tables_base=plan->root;
    d->image_mapping.tables_bytes=plan->table_count*4096;
    return PWL_OK;
}

pwl_status_t pwl_native_graphics_publish(pwl_native_workspace_t *w,
 const pwl_resident_image_t *image,const pwl_native_transition_plan_t *plan,
 const pwl_x64_table_page_t *tables,const pwl_graphics_spec_t *spec)
{
    pwl_efi_entry_context_t checked;
    if (!spec || pwl_native_efi_entry_prepare(w,image,plan,tables,&checked)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    pwl_graphics_spec_t input=*spec;spec=&input;
    pwl_native_data_t *d=w->data.prepare_address;
    if (d->graphics.enabled || d->memory.exited || d->tpl!=4 || d->protocol_next_handle==UINT64_MAX)
        return PWL_ERR_ACCESS_DENIED;
    pwl_resident_graphics_t prepared;
    pwl_efi_table_spec_t code={0};code.code_pa=w->firmware.physical_address;code.code_bytes=image->size;
    for (size_t i=0;i<PWL_EFI_PREPARED_CALLBACKS;i++) code.callback_offsets[i]=image->callbacks[i];
    uint64_t destination=w->data.physical_address+offsetof(pwl_native_data_t,graphics);
    if (pwl_graphics_prepare(spec,&code,destination,&prepared)!=PWL_OK) return PWL_ERR_INVALID_ARGUMENT;
    uint64_t base=spec->framebuffer&~UINT64_C(4095),end=spec->framebuffer+prepared.mode.framebuffer_bytes;
    if ((base<w->arena.physical_address+w->arena.size && w->arena.physical_address<end) ||
        (base<plan->root+plan->table_count*4096 && plan->root<end)) return PWL_ERR_INVALID_ARGUMENT;
    for (uint64_t p=base;p<end;p+=4096) {
        pwl_x64_translation_t x;
        if (pwl_x64_translate(tables,plan->table_count,plan->root,p,&x)!=PWL_OK ||
            x.physical_address!=p || !x.writable || x.executable || x.user || x.pat_index!=spec->pat_index)
            return PWL_ERR_BAD_IMAGE;
    }
    size_t slots[2],count=0;
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS && count<2;i++) if (!d->protocols[i].handle) slots[count++]=i;
    if (count<2) return PWL_ERR_OUT_OF_RESOURCES;
    static const pwl_efi_guid_t gop={{0xde,0xa9,0x42,0x90,0xdc,0x23,0x38,0x4a,0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
    static const pwl_efi_guid_t text={{0xc2,0x77,0x74,0x38,0xc7,0x69,0xd2,0x11,0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
    uint64_t handle=++d->protocol_next_handle;
    w->graphics_spec=*spec;
    d->graphics=prepared;
    d->protocols[slots[0]]=(pwl_resident_protocol_t){handle,destination+offsetof(pwl_resident_graphics_t,gop),gop};
    d->protocols[slots[1]]=(pwl_resident_protocol_t){handle,destination+offsetof(pwl_resident_graphics_t,text),text};
    d->efi.system.console_out_handle=d->efi.system.console_error_handle=handle;
    d->efi.system.console_out=d->efi.system.console_error=destination+offsetof(pwl_resident_graphics_t,text);
    d->efi.system.header.crc32=0;
    d->efi.system.header.crc32=pwl_efi_crc32(&d->efi.system,sizeof(d->efi.system));
    return PWL_OK;
}
