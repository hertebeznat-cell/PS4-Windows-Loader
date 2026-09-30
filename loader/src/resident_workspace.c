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
        return PWL_OK;
    }
    pwl_status_t cleanup=pwl_native_workspace_release(w);
    return cleanup==PWL_OK ? status : cleanup;
}
