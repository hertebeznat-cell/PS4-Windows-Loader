#include "pwl.h"

#include <string.h>

/*
 * Stage 0 portable core.
 *
 * This file intentionally contains no PS4 register writes yet. The hardware
 * entry layer will be added only once the Baikal handoff sequence is validated
 * against public PS4 research/source. Keeping this core portable lets CI catch
 * basic interface and PE parsing regressions immediately.
 */

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *p) {
    return (uint64_t)read_u32_le(p) | ((uint64_t)read_u32_le(p + 4) << 32);
}

pwl_status_t pwl_platform_probe(pwl_platform_info_t *info) {
    if (info == NULL) {
        return PWL_ERR_INVALID_ARGUMENT;
    }

    memset(info, 0, sizeof(*info));
    info->signature = PWL_SIGNATURE;
    info->version_major = PWL_VERSION_MAJOR;
    info->version_minor = PWL_VERSION_MINOR;

    /* PS4/Baikal-specific probing is not implemented in Stage 0. */
    return PWL_ERR_UNSUPPORTED;
}

pwl_status_t pwl_pe_inspect(const void *data, size_t size, pwl_pe_image_t *image) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t pe_offset;
    const uint8_t *coff;
    const uint8_t *optional;
    uint16_t machine;
    uint16_t optional_size;
    uint16_t magic;

    if (data == NULL || image == NULL) {
        return PWL_ERR_INVALID_ARGUMENT;
    }

    memset(image, 0, sizeof(*image));

    if (size < 0x40 || bytes[0] != 'M' || bytes[1] != 'Z') {
        return PWL_ERR_BAD_IMAGE;
    }

    pe_offset = read_u32_le(bytes + 0x3c);
    if ((size_t)pe_offset > size || size - (size_t)pe_offset < 24) {
        return PWL_ERR_BAD_IMAGE;
    }

    coff = bytes + pe_offset;
    if (coff[0] != 'P' || coff[1] != 'E' || coff[2] != 0 || coff[3] != 0) {
        return PWL_ERR_BAD_IMAGE;
    }

    machine = read_u16_le(coff + 4);
    optional_size = read_u16_le(coff + 20);

    /* IMAGE_FILE_MACHINE_AMD64 */
    if (machine != 0x8664U) {
        return PWL_ERR_UNSUPPORTED;
    }

    if (optional_size < 0x70U ||
        (size_t)optional_size > size - (size_t)(coff - bytes) - 24U) {
        return PWL_ERR_BAD_IMAGE;
    }

    optional = coff + 24;
    magic = read_u16_le(optional);

    /* PE32+ */
    if (magic != 0x20bU) {
        return PWL_ERR_UNSUPPORTED;
    }

    image->file_data = data;
    image->file_size = size;
    image->entry_rva = read_u32_le(optional + 0x10);
    image->preferred_base = read_u64_le(optional + 0x18);
    image->image_size = read_u32_le(optional + 0x38);

    if (image->image_size == 0U || image->entry_rva >= image->image_size ||
        read_u32_le(optional + 0x3c) == 0U ||
        read_u32_le(optional + 0x3c) > image->image_size ||
        read_u32_le(optional + 0x3c) > size) {
        memset(image, 0, sizeof(*image));
        return PWL_ERR_BAD_IMAGE;
    }

    return PWL_OK;
}

pwl_status_t pwl_boot_windows(void) {
    /*
     * Future Stage 1 flow:
     *   1. PS4/Baikal hardware handoff
     *   2. enumerate/read EFI volume
     *   3. load bootmgfw.efi as PE32+
     *   4. construct EFI compatibility services/tables
     *   5. transfer control
     */
    return PWL_ERR_UNSUPPORTED;
}
