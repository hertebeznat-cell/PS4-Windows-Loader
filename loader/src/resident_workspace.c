#include "pwl_native_workspace.h"
static const pwl_efi_guid_t loaded_guid={{0xa1,0x31,0x1b,0x5b,0x62,0x95,0xd2,0x11,
                                        0x8e,0x3f,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static const pwl_efi_guid_t filesystem_guid={{0x22,0x5b,0x4e,0x96,0x59,0x64,0xd2,0x11,
                                             0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};

static void file_path_node(const uint16_t *path,unsigned char out[520])
{
    size_t length=0;
    while (path[length]) length++;
    size_t node_bytes=4+2*(length+1);
    for (size_t i=0;i<520;i++) out[i]=0;
    out[0]=4;out[1]=4; /* MEDIA_DEVICE_PATH / MEDIA_FILEPATH_DP */
    out[2]=(unsigned char)node_bytes;out[3]=(unsigned char)(node_bytes>>8);
    for (size_t i=0;i<length;i++) {
        out[4+i*2]=(unsigned char)path[i];out[5+i*2]=(unsigned char)(path[i]>>8);
    }
    out[node_bytes]=0x7f;out[node_bytes+1]=0xff;out[node_bytes+2]=4;
}

pwl_status_t pwl_native_boot_prepare(const pwl_ps4_memory_api_t *api,
    const pwl_native_request_t *r,const pwl_resident_image_t *image,
    const uint16_t *path,pwl_native_workspace_t *w)
{
    if (!r || !w || !path || r->boot_image || r->boot_image_bytes)
        return PWL_ERR_INVALID_ARGUMENT;
    pwl_status_t status=pwl_files_validate(r->disk_image,r->disk_bytes);
    if (status!=PWL_OK) return status;
    pwl_file_view_t source;
    uint64_t result=pwl_files_open(r->disk_image,r->disk_bytes,path,&source);
    if (result==PWL_EFI_NOT_FOUND) return PWL_ERR_NOT_FOUND;
    if (result || source.directory || !source.size) return PWL_ERR_BAD_IMAGE;
    pwl_native_request_t request=*r;
    request.boot_image=(const unsigned char *)r->disk_image+(size_t)source.offset;
    request.boot_image_bytes=(size_t)source.size;
    status=pwl_native_workspace_prepare_resident(api,&request,image,w);
    if (status!=PWL_OK) return status;
    pwl_native_data_t *d=w->data.prepare_address;
    uint16_t actual_path[PWL_FILES_PATH];pwl_file_view_t copied;
    result=pwl_files_at(w->media.prepare_address,(size_t)d->media.bytes,source.index,
                        &copied,actual_path);
    if (!result && d->files_enabled && copied.offset==source.offset && copied.size==source.size) {
        file_path_node(actual_path,d->boot_file_path);
        d->boot_file_record=source.index;d->boot_origin_bound=1;
        d->loaded_image.device_handle=d->protocols[1].handle;
        d->loaded_image.file_path=w->data.physical_address+offsetof(pwl_native_data_t,boot_file_path);
        status=pwl_native_resident_environment_validate(w,image);
        if (status==PWL_OK) return status;
    } else status=PWL_ERR_BAD_IMAGE;
    pwl_status_t cleanup=pwl_native_workspace_release(w);
    return cleanup==PWL_OK ? status : cleanup;
}

pwl_status_t pwl_resident_image_validate(const pwl_resident_image_t *b)
{
    if (!b || !b->bytes || b->size<8 || (b->binding_offset&7) ||
        b->binding_offset>b->size-8 || pwl_efi_crc32(b->bytes,b->size)!=b->crc32)
        return PWL_ERR_INVALID_ARGUMENT;
    const unsigned char *bytes=b->bytes;
    for (size_t i=0;i<8;i++) if (bytes[b->binding_offset+i]) return PWL_ERR_INVALID_ARGUMENT;
    for (size_t i=0;i<PWL_EFI_PREPARED_CALLBACKS;i++) {
        if (b->callbacks[i]>=b->size ||
            (b->callbacks[i]>=b->binding_offset && b->callbacks[i]<b->binding_offset+8))
            return PWL_ERR_INVALID_ARGUMENT;
        for (size_t j=0;j<i;j++) if (b->callbacks[i]==b->callbacks[j]) return PWL_ERR_INVALID_ARGUMENT;
    }
    return PWL_OK;
}

pwl_status_t pwl_native_workspace_prepare_resident(const pwl_ps4_memory_api_t *api,
    const pwl_native_request_t *r,const pwl_resident_image_t *blob,pwl_native_workspace_t *w)
{
    if (!r || pwl_resident_image_validate(blob)!=PWL_OK) return PWL_ERR_INVALID_ARGUMENT;
    pwl_native_request_t request=*r;
    request.firmware=blob->bytes;request.firmware_bytes=blob->size;
    pwl_status_t status=pwl_native_workspace_prepare(api,&request,w);
    if (status!=PWL_OK) return status;
    pwl_native_data_t *d=w->data.prepare_address;
    pwl_efi_table_spec_t spec={w->firmware.physical_address,blob->size,
        w->data.physical_address+offsetof(pwl_native_data_t,efi),sizeof(d->efi),{0}};
    for (size_t i=0;i<PWL_EFI_PREPARED_CALLBACKS;i++) spec.callback_offsets[i]=blob->callbacks[i];
    status=pwl_efi_tables_prepare(&spec,&d->efi);
    if (status==PWL_OK) status=pwl_efi_tables_validate(&spec,&d->efi);
    if (status==PWL_OK) {
        unsigned char *binding=(unsigned char *)w->firmware.prepare_address+blob->binding_offset;
        for (unsigned i=0;i<8;i++) binding[i]=(unsigned char)(w->data.physical_address>>(8*i));
        d->tpl=4;
        d->image_mapping.heap_base=w->heap.physical_address;
        d->image_mapping.heap_bytes=w->heap.size;
        d->image_mapping.boot_base=w->boot.physical_address;
        d->image_mapping.boot_bytes=w->boot.size;
        d->image_mapping.permission_callback=w->firmware.physical_address+blob->callbacks[52];
        if (w->boot.size) {
            d->initial_application.mapped=w->boot_image;
            d->initial_application.handle=r->image_handle;
            d->initial_application.initial=1;
            d->loaded_image=(pwl_efi_loaded_image_t){0};
            d->loaded_image.revision=0x1000;
            d->loaded_image.system_table=w->data.physical_address+
                offsetof(pwl_native_data_t,efi)+offsetof(pwl_efi_prepared_tables_t,system);
            d->loaded_image.image_base=w->boot.physical_address;
            d->loaded_image.image_size=w->boot.size;
            d->loaded_image.image_code_type=1;
            d->loaded_image.image_data_type=2;
            /* Memory-buffer image: no invented device or filesystem path. */
            d->protocols[0]=(pwl_resident_protocol_t){r->image_handle,
                w->data.physical_address+offsetof(pwl_native_data_t,loaded_image),loaded_guid};
            d->protocol_next_handle=r->image_handle;
        }
        const unsigned char *media=w->media.prepare_address;
        const unsigned char magic[8]={'P','W','L','F','I','L','E','S'};
        int archive=1;
        for (size_t i=0;i<8;i++) if (media[i]!=magic[i]) archive=0;
        if (archive) {
            status=pwl_files_validate(media,r->disk_bytes);
            if (status!=PWL_OK) goto cleanup;
            d->filesystem[0]=UINT64_C(0x10000);
            d->filesystem[1]=w->firmware.physical_address+blob->callbacks[17];
            d->file_template.revision=UINT64_C(0x10000);
            for (size_t i=0;i<10;i++)
                d->file_template.functions[i]=w->firmware.physical_address+blob->callbacks[18+i];
            uint64_t handle=r->image_handle==1 ? 2 : 1;
            d->protocols[1]=(pwl_resident_protocol_t){handle,
                w->data.physical_address+offsetof(pwl_native_data_t,filesystem),filesystem_guid};
            if (d->protocol_next_handle<handle) d->protocol_next_handle=handle;
            d->files_enabled=1;
        }
        status=pwl_native_resident_environment_validate(w,blob);
        if (status==PWL_OK) return PWL_OK;
    }
cleanup:
    {
        pwl_status_t release=pwl_native_workspace_release(w);
        return release==PWL_OK ? status : release;
    }
}

static int span_owned(const pwl_native_workspace_t *w,const pwl_owned_span_t *s)
{
    if (!s->size || !s->prepare_address || s->physical_address<w->arena.physical_address)
        return 0;
    uint64_t offset=s->physical_address-w->arena.physical_address;
    return offset<=w->arena.size && s->size<=w->arena.size-offset &&
        offset<=UINTPTR_MAX-w->arena.kernel_address &&
        (uintptr_t)s->prepare_address==w->arena.kernel_address+offset;
}

pwl_status_t pwl_native_resident_environment_validate(
    const pwl_native_workspace_t *w,const pwl_resident_image_t *image)
{
    if (!w || pwl_resident_image_validate(image)!=PWL_OK ||
        !w->arena.kernel_address || !w->arena.size ||
        w->arena.used>w->arena.size ||
        w->region_count!=(w->boot.size ? 9U : 8U) ||
        w->boot_image.range_count>PWL_PE_MAX_RANGES ||
        w->mapping_count!=6+w->boot_image.range_count ||
        w->table_count<4 || w->table_count>PWL_NATIVE_MAX_TABLES)
        return PWL_ERR_INVALID_ARGUMENT;
    const pwl_owned_span_t *spans[]={&w->firmware,&w->data,&w->tables_span,
                                    &w->stack,&w->media,&w->heap};
    for (size_t i=0;i<6;i++) {
        const pwl_owned_span_t *s=spans[i];
        if (!span_owned(w,s) || s->size%PWL_PAGE_SIZE ||
            s->physical_address%PWL_PAGE_SIZE ||
            w->mappings[i].base!=s->physical_address ||
            w->mappings[i].size!=s->size ||
            w->mappings[i].writable!=(i!=0 && i!=4) ||
            w->mappings[i].executable!=(i==0)) return PWL_ERR_INVALID_ARGUMENT;
        if (i && s->physical_address<spans[i-1]->physical_address+spans[i-1]->size)
            return PWL_ERR_INVALID_ARGUMENT;
    }
    /* Resident callbacks and the relocated application share one owner/root.
     * The image ranges must cover its full extent without gaps or W+X pages;
     * its entry must belong to executable, read-only memory. */
    if (w->boot.size) {
        const pwl_pe_loaded_t *boot=&w->boot_image;
        if (!span_owned(w,&w->boot) || w->boot.size%PWL_PAGE_SIZE ||
            w->boot.physical_address%PWL_PAGE_SIZE ||
            w->boot.physical_address<w->heap.physical_address+w->heap.size ||
            boot->physical_address!=w->boot.physical_address ||
            boot->image_size!=w->boot.size || !boot->range_count)
            return PWL_ERR_INVALID_ARGUMENT;
        uint64_t covered=0;
        int executable_entry=0;
        for (size_t i=0;i<boot->range_count;i++) {
            const pwl_x64_identity_range_t *range=&boot->ranges[i];
            const pwl_x64_identity_range_t *mapping=&w->mappings[6+i];
            if (range->base!=boot->physical_address+covered || !range->size ||
                range->size%PWL_PAGE_SIZE || range->size>boot->image_size-covered ||
                range->writable>1 || range->executable>1 ||
                (range->writable && range->executable) ||
                mapping->base!=range->base || mapping->size!=range->size ||
                mapping->writable!=range->writable || mapping->executable!=range->executable)
                return PWL_ERR_INVALID_ARGUMENT;
            if (boot->entry_address>=range->base &&
                boot->entry_address-range->base<range->size && range->executable)
                executable_entry=1;
            covered+=range->size;
        }
        if (covered!=boot->image_size || !executable_entry)
            return PWL_ERR_INVALID_ARGUMENT;
        const pwl_phys_region_t *region=&w->regions[8];
        if (region->base!=w->boot.physical_address || region->length!=w->boot.size ||
            region->kind!=PWL_MEMORY_LOADER_CODE)
            return PWL_ERR_INVALID_ARGUMENT;
    } else if (w->boot.prepare_address || w->boot.physical_address ||
               w->boot_image.physical_address || w->boot_image.image_size ||
               w->boot_image.entry_address || w->boot_image.range_count) {
        return PWL_ERR_INVALID_ARGUMENT;
    }
    if (image->size>w->firmware.size || sizeof(pwl_native_data_t)>w->data.size ||
        w->table_count>w->tables_span.size/PWL_PAGE_SIZE ||
        w->stack.physical_address-w->tables_span.physical_address-w->tables_span.size!=PWL_PAGE_SIZE ||
        w->media.physical_address-w->stack.physical_address-w->stack.size!=PWL_PAGE_SIZE)
        return PWL_ERR_INVALID_ARGUMENT;
    for (size_t i=0;i<w->table_count;i++)
        if (w->tables[i].physical_address!=w->tables_span.physical_address+i*PWL_PAGE_SIZE ||
            (uintptr_t)w->tables[i].entries!=(uintptr_t)w->tables_span.prepare_address+i*PWL_PAGE_SIZE)
            return PWL_ERR_INVALID_ARGUMENT;
    if (pwl_x64_identity_mappings_validate(w->mappings,w->mapping_count,w->tables,w->table_count)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    const unsigned char *actual=w->firmware.prepare_address,*original=image->bytes;
    for (size_t i=0;i<image->size;i++) {
        unsigned char expected=original[i];
        if (i>=image->binding_offset && i<image->binding_offset+8)
            expected=(unsigned char)(w->data.physical_address>>(8*(i-image->binding_offset)));
        if (actual[i]!=expected) return PWL_ERR_INVALID_ARGUMENT;
    }
    const pwl_native_data_t *data=w->data.prepare_address;
    if (data->image_mapping.heap_base!=w->heap.physical_address ||
        data->image_mapping.heap_bytes!=w->heap.size ||
        data->image_mapping.boot_base!=w->boot.physical_address ||
        data->image_mapping.boot_bytes!=w->boot.size ||
        data->image_mapping.permission_callback!=w->firmware.physical_address+image->callbacks[52])
        return PWL_ERR_INVALID_ARGUMENT;
    /* The mutable transition table binding has its own plan/ownership audit in
     * pwl_native_image_mapping_bind. It does not certify CPU entry readiness. */
    if (w->boot.size) {
        if (data->initial_application.handle!=data->memory.image_handle ||
            data->initial_application.initial!=1 || data->initial_application.running ||
            data->initial_application.quarantined) return PWL_ERR_INVALID_ARGUMENT;
        const unsigned char *a=(const void *)&data->initial_application.mapped;
        const unsigned char *b=(const void *)&w->boot_image;
        for (size_t i=0;i<sizeof(w->boot_image);i++) if (a[i]!=b[i]) return PWL_ERR_INVALID_ARGUMENT;
    } else if (data->initial_application.handle || data->initial_application.initial)
        return PWL_ERR_INVALID_ARGUMENT;
    if (data->media.physical_address!=w->media.physical_address ||
        !data->media.bytes || data->media.bytes>w->media.size ||
        data->media.block_size!=512 || data->media.bytes%512)
        return PWL_ERR_INVALID_ARGUMENT;
    const unsigned char *media_bytes=w->media.prepare_address;
    const unsigned char file_magic[8]={'P','W','L','F','I','L','E','S'};
    unsigned has_archive=1;
    for (size_t i=0;i<8;i++) if (media_bytes[i]!=file_magic[i]) has_archive=0;
    if (data->files_enabled!=has_archive) return PWL_ERR_INVALID_ARGUMENT;
    if (data->files_enabled) {
        if (data->files_enabled!=1 || pwl_files_validate(w->media.prepare_address,
                (size_t)data->media.bytes)!=PWL_OK ||
            data->filesystem[0]!=UINT64_C(0x10000) ||
            data->filesystem[1]!=w->firmware.physical_address+image->callbacks[17] ||
            data->file_template.revision!=UINT64_C(0x10000) ||
            !data->protocols[1].handle ||
            data->protocols[1].interface_address!=w->data.physical_address+
                offsetof(pwl_native_data_t,filesystem)) return PWL_ERR_INVALID_ARGUMENT;
        for (size_t i=0;i<16;i++)
            if (data->protocols[1].guid.bytes[i]!=filesystem_guid.bytes[i])
                return PWL_ERR_INVALID_ARGUMENT;
        for (size_t i=0;i<10;i++)
            if (data->file_template.functions[i]!=w->firmware.physical_address+image->callbacks[18+i])
                return PWL_ERR_INVALID_ARGUMENT;
    }
    if (w->boot.size) {
        pwl_efi_loaded_image_t expected={0};
        expected.revision=0x1000;
        expected.system_table=w->data.physical_address+offsetof(pwl_native_data_t,efi);
        expected.image_base=w->boot.physical_address;
        expected.image_size=w->boot.size;
        expected.image_code_type=1;expected.image_data_type=2;
        if (data->boot_origin_bound) {
            if (data->boot_origin_bound!=1 || !data->files_enabled)
                return PWL_ERR_INVALID_ARGUMENT;
            uint16_t path[PWL_FILES_PATH];pwl_file_view_t source;
            if (pwl_files_at(w->media.prepare_address,(size_t)data->media.bytes,
                    data->boot_file_record,&source,path)!=PWL_EFI_SUCCESS ||
                source.directory || !source.size) return PWL_ERR_INVALID_ARGUMENT;
            uint64_t image_size;
            if (pwl_pe_efi_size((const unsigned char *)w->media.prepare_address+(size_t)source.offset,
                    (size_t)source.size,&image_size)!=PWL_OK || image_size!=w->boot.size)
                return PWL_ERR_INVALID_ARGUMENT;
            pwl_pe_image_t pe;
            if (pwl_pe_inspect((const unsigned char *)w->media.prepare_address+(size_t)source.offset,
                    (size_t)source.size,&pe)!=PWL_OK ||
                w->boot_image.entry_address!=w->boot.physical_address+pe.entry_rva)
                return PWL_ERR_INVALID_ARGUMENT;
            unsigned char node[520];file_path_node(path,node);
            for (size_t i=0;i<sizeof(node);i++)
                if (node[i]!=data->boot_file_path[i]) return PWL_ERR_INVALID_ARGUMENT;
            expected.device_handle=data->protocols[1].handle;
            expected.file_path=w->data.physical_address+offsetof(pwl_native_data_t,boot_file_path);
        }
        const unsigned char *actual=(const unsigned char *)&data->loaded_image;
        const unsigned char *wanted=(const unsigned char *)&expected;
        for (size_t i=0;i<sizeof(expected);i++)
            if (actual[i]!=wanted[i]) return PWL_ERR_INVALID_ARGUMENT;
        const pwl_resident_protocol_t *protocol=&data->protocols[0];
        if (protocol->handle!=data->memory.image_handle ||
            protocol->interface_address!=w->data.physical_address+
                offsetof(pwl_native_data_t,loaded_image)) return PWL_ERR_INVALID_ARGUMENT;
        for (size_t i=0;i<16;i++)
            if (protocol->guid.bytes[i]!=loaded_guid.bytes[i]) return PWL_ERR_INVALID_ARGUMENT;
    }
    if (!data->boot_origin_bound) {
        if (data->boot_file_record) return PWL_ERR_INVALID_ARGUMENT;
        for (size_t i=0;i<sizeof(data->boot_file_path);i++)
            if (data->boot_file_path[i]) return PWL_ERR_INVALID_ARGUMENT;
    } else if (!w->boot.size) return PWL_ERR_INVALID_ARGUMENT;
    pwl_efi_table_spec_t spec={w->firmware.physical_address,image->size,
        w->data.physical_address+offsetof(pwl_native_data_t,efi),sizeof(data->efi),{0}};
    for (size_t i=0;i<PWL_EFI_PREPARED_CALLBACKS;i++)spec.callback_offsets[i]=image->callbacks[i];
    if (!data->graphics.enabled) return pwl_efi_tables_validate(&spec,&data->efi);
    uint64_t destination=w->data.physical_address+offsetof(pwl_native_data_t,graphics);
    pwl_resident_graphics_t expected;
    if (pwl_graphics_prepare(&w->graphics_spec,&spec,destination,&expected)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    const unsigned char *ga=(const void *)&expected,*gb=(const void *)&data->graphics;
    for (size_t i=0;i<sizeof(expected);i++) if (ga[i]!=gb[i]) return PWL_ERR_INVALID_ARGUMENT;
    static const pwl_efi_guid_t gop={{0xde,0xa9,0x42,0x90,0xdc,0x23,0x38,0x4a,0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
    static const pwl_efi_guid_t text={{0xc2,0x77,0x74,0x38,0xc7,0x69,0xd2,0x11,0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
    uint64_t handle=data->efi.system.console_out_handle;unsigned found=0;
    if (!handle || data->efi.system.console_error_handle!=handle ||
        data->efi.system.console_out!=destination+offsetof(pwl_resident_graphics_t,text) ||
        data->efi.system.console_error!=data->efi.system.console_out) return PWL_ERR_INVALID_ARGUMENT;
    for (size_t i=0;i<PWL_RESIDENT_PROTOCOLS;i++) {
        const pwl_resident_protocol_t *p=&data->protocols[i];
        if (p->handle!=handle) continue;
        unsigned eg=1,et=1;
        for (size_t j=0;j<16;j++) { if (p->guid.bytes[j]!=gop.bytes[j]) eg=0;if (p->guid.bytes[j]!=text.bytes[j]) et=0; }
        if (eg && p->interface_address==destination+offsetof(pwl_resident_graphics_t,gop)) found|=1;
        if (et && p->interface_address==destination+offsetof(pwl_resident_graphics_t,text)) found|=2;
    }
    if (found!=3) return PWL_ERR_INVALID_ARGUMENT;
    pwl_efi_prepared_tables_t canonical=data->efi;
    uint32_t crc=canonical.system.header.crc32;canonical.system.header.crc32=0;
    if (pwl_efi_crc32(&canonical.system,sizeof(canonical.system))!=crc) return PWL_ERR_INVALID_ARGUMENT;
    canonical.system.console_out_handle=canonical.system.console_out=0;
    canonical.system.console_error_handle=canonical.system.console_error=0;
    canonical.system.header.crc32=pwl_efi_crc32(&canonical.system,sizeof(canonical.system));
    return pwl_efi_tables_validate(&spec,&canonical);
}
