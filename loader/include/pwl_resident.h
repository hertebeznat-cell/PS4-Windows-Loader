#ifndef PWL_RESIDENT_H
#define PWL_RESIDENT_H
#include "pwl_firmware.h"
#include "pwl_efi_tables.h"
#include "pwl_files.h"
#define PWL_RESIDENT_FILES 32U
typedef struct pwl_efi_file_protocol {
    uint64_t revision,functions[10];
} pwl_efi_file_protocol_t;
typedef struct pwl_resident_file {
    pwl_efi_file_protocol_t protocol;
    pwl_file_view_t view;
    uint64_t position;
    unsigned active;
} pwl_resident_file_t;
#define PWL_RESIDENT_PROTOCOLS 64U
typedef struct pwl_efi_guid { unsigned char bytes[16]; } pwl_efi_guid_t;
typedef struct pwl_resident_protocol {
    uint64_t handle, interface_address;
    pwl_efi_guid_t guid;
} pwl_resident_protocol_t;
typedef struct pwl_efi_loaded_image {
    uint32_t revision, padding;
    uint64_t parent_handle, system_table, device_handle, file_path, reserved;
    uint32_t load_options_size, padding2;
    uint64_t load_options, image_base, image_size;
    uint32_t image_code_type, image_data_type;
    uint64_t unload;
} pwl_efi_loaded_image_t;
_Static_assert(sizeof(pwl_efi_loaded_image_t)==96,"LoadedImage AMD64 layout");
_Static_assert(offsetof(pwl_efi_loaded_image_t,image_base)==64,"LoadedImage base offset");

typedef struct pwl_resident_data {
    pwl_fw_memory_t memory;
    pwl_fw_media_t media;
    uint64_t tpl;
    pwl_efi_prepared_tables_t efi;
    pwl_resident_protocol_t protocols[PWL_RESIDENT_PROTOCOLS];
    uint64_t protocol_next_handle;
    pwl_efi_loaded_image_t loaded_image;
    uint64_t filesystem[2]; /* revision and OpenVolume destination address */
    pwl_efi_file_protocol_t file_template;
    pwl_resident_file_t files[PWL_RESIDENT_FILES];
    unsigned files_enabled;
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
