#include "pwl_firmware.h"

pwl_status_t pwl_fw_media_init(pwl_fw_media_t *media, uint64_t address,
                               uint64_t bytes, uint32_t block_size,
                               uint32_t media_id)
{
    if (media == NULL || address == 0 || address % PWL_PAGE_SIZE ||
        bytes == 0 || bytes > UINT64_MAX - address || bytes > SIZE_MAX ||
        block_size < 512 || block_size > PWL_PAGE_SIZE ||
        (block_size & (block_size - 1)) || bytes % block_size)
        return PWL_ERR_INVALID_ARGUMENT;
    media->physical_address = address;
    media->bytes = bytes;
    media->block_size = block_size;
    media->media_id = media_id;
    return PWL_OK;
}

uint64_t pwl_fw_media_read(const pwl_fw_media_t *media, const void *mapping,
                          uint32_t media_id, uint64_t lba, size_t bytes,
                          void *buffer)
{
    uint64_t offset;
    size_t i;
    const unsigned char *src = mapping;
    unsigned char *dst = buffer;
    if (media == NULL || media->block_size == 0 || mapping == NULL)
        return PWL_EFI_INVALID_PARAMETER;
    if (media_id != media->media_id) return PWL_EFI_MEDIA_CHANGED;
    if (bytes == 0) return PWL_EFI_SUCCESS;
    if (buffer == NULL) return PWL_EFI_INVALID_PARAMETER;
    if (bytes % media->block_size) return PWL_EFI_BAD_BUFFER_SIZE;
    if (lba >= media->bytes / media->block_size)
        return PWL_EFI_INVALID_PARAMETER;
    offset = lba * media->block_size;
    if (bytes > media->bytes - offset) return PWL_EFI_INVALID_PARAMETER;
    /* memmove semantics also make overlapping preparation buffers harmless. */
    src += (size_t)offset;
    if ((uintptr_t)dst > (uintptr_t)src && (uintptr_t)dst - (uintptr_t)src < bytes)
        for (i = bytes; i != 0; --i) dst[i - 1] = src[i - 1];
    else
        for (i = 0; i < bytes; ++i) dst[i] = src[i];
    return PWL_EFI_SUCCESS;
}

uint64_t pwl_fw_media_write(const pwl_fw_media_t *media, uint32_t media_id)
{
    if (media == NULL || media->block_size == 0) return PWL_EFI_INVALID_PARAMETER;
    if (media_id != media->media_id) return PWL_EFI_MEDIA_CHANGED;
    return PWL_EFI_WRITE_PROTECTED;
}
