#include "pwl_graphics.h"
pwl_status_t pwl_graphics_prepare(const pwl_graphics_spec_t *s,
 const pwl_efi_table_spec_t *code,uint64_t pa,pwl_resident_graphics_t *out)
{
    if (!s || !code || !out || !pa || pa&7 || pa>=(UINT64_C(1)<<47) ||
        sizeof(*out)>(UINT64_C(1)<<47)-pa || !code->code_pa || !code->code_bytes ||
        code->code_pa>=(UINT64_C(1)<<47) || code->code_bytes>(UINT64_C(1)<<47)-code->code_pa ||
        !s->framebuffer || s->framebuffer&3 || s->framebuffer>=(UINT64_C(1)<<47) ||
        !s->bytes || s->bytes>(UINT64_C(1)<<47)-s->framebuffer ||
        s->width<640 || s->height<400 || s->width>16384 || s->height>16384 ||
        s->pitch<s->width || s->pitch>65536 || s->pixel_format>1 || s->pat_index>7 ||
        (uint64_t)s->pitch*s->height*4>s->bytes ||
        (s->framebuffer<pa+sizeof(*out) && pa<s->framebuffer+s->bytes) ||
        (s->framebuffer<code->code_pa+code->code_bytes && code->code_pa<s->framebuffer+s->bytes))
        return PWL_ERR_INVALID_ARGUMENT;
    for (size_t i=54;i<66;i++) if (code->callback_offsets[i]>=code->code_bytes)
        return PWL_ERR_INVALID_ARGUMENT;
    pwl_graphics_spec_t spec=*s;pwl_efi_table_spec_t binding=*code;
    *out=(pwl_resident_graphics_t){0};
    for (size_t i=0;i<3;i++) out->gop[i]=binding.code_pa+binding.callback_offsets[54+i];
    out->gop[3]=pa+offsetof(pwl_resident_graphics_t,mode);
    for (size_t i=0;i<9;i++) out->text[i]=binding.code_pa+binding.callback_offsets[57+i];
    out->text[9]=pa+offsetof(pwl_resident_graphics_t,text_mode);
    out->info=(pwl_graphics_info_t){0,spec.width,spec.height,spec.pixel_format,{0},spec.pitch};
    out->mode=(pwl_graphics_mode_t){1,0,pa+offsetof(pwl_resident_graphics_t,info),sizeof(out->info),
        spec.framebuffer,(uint64_t)spec.pitch*spec.height*4};
    out->text_mode=(pwl_text_mode_t){1,0,7,0,0,1,{0}};
    out->enabled=1;
    out->pat_index=spec.pat_index;
    return PWL_OK;
}
