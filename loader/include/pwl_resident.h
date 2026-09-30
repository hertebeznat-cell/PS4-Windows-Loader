#ifndef PWL_RESIDENT_H
#define PWL_RESIDENT_H
#include "pwl_firmware.h"
#include "pwl_efi_tables.h"

typedef struct pwl_resident_data {
    pwl_fw_memory_t memory;
    pwl_fw_media_t media;
    uint64_t tpl;
    pwl_efi_prepared_tables_t efi;
} pwl_resident_data_t;

typedef struct pwl_resident_image {
    const void *bytes;
    size_t size;
    uint64_t binding_offset;
    uint64_t callbacks[PWL_EFI_PREPARED_CALLBACKS];
    uint32_t crc32;
} pwl_resident_image_t;
/* Integrity/extent checks only. Call offsets must come from audited linked
 * symbols, not arbitrary bytes. The binding slot must initially be zero.
 */
pwl_status_t pwl_resident_image_validate(const pwl_resident_image_t *image);
#endif
