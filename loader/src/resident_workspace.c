#include "pwl_native_workspace.h"

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
        status=pwl_native_resident_environment_validate(w,blob);
        if (status==PWL_OK) return PWL_OK;
    }
    pwl_status_t cleanup=pwl_native_workspace_release(w);
    return cleanup==PWL_OK ? status : cleanup;
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
        w->arena.used>w->arena.size || w->boot.size ||
        w->region_count!=8 || w->mapping_count!=6 ||
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
    pwl_efi_table_spec_t spec={w->firmware.physical_address,image->size,
        w->data.physical_address+offsetof(pwl_native_data_t,efi),sizeof(data->efi),{0}};
    for (size_t i=0;i<PWL_EFI_PREPARED_CALLBACKS;i++)spec.callback_offsets[i]=image->callbacks[i];
    return pwl_efi_tables_validate(&spec,&data->efi);
}
