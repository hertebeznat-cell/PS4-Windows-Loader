#ifndef PWL_RESIDENT_H
#define PWL_RESIDENT_H
#include "pwl_firmware.h"
#include "pwl_efi_tables.h"
#include "pwl_files.h"
#include "pwl_pe_loader.h"
#include "pwl_graphics.h"
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
#define PWL_RESIDENT_OPENS 64U
typedef struct pwl_resident_open {
    uint64_t agent;
    uint32_t protocol_index, attributes, count;
} pwl_resident_open_t;
typedef struct pwl_efi_guid { unsigned char bytes[16]; } pwl_efi_guid_t;
#define PWL_RESIDENT_CONFIGURATIONS 32U
typedef struct pwl_efi_configuration {
    pwl_efi_guid_t guid;
    uint64_t table;
} pwl_efi_configuration_t;
typedef struct pwl_efi_open_info {
    uint64_t agent_handle, controller_handle;
    uint32_t attributes, open_count;
} pwl_efi_open_info_t;
_Static_assert(sizeof(pwl_efi_configuration_t)==24,"AMD64 configuration layout");
_Static_assert(sizeof(pwl_efi_open_info_t)==24,"AMD64 open information layout");
#define PWL_RESIDENT_EVENTS 32U
#define PWL_EVT_TIMER UINT32_C(0x80000000)
#define PWL_EVT_NOTIFY_WAIT UINT32_C(0x100)
#define PWL_EVT_NOTIFY_SIGNAL UINT32_C(0x200)
typedef void (__attribute__((ms_abi)) *pwl_efi_event_notify_t)(uint64_t,void *);
typedef struct pwl_resident_event {
    uint64_t handle, notify_tpl;
    uint64_t timer_deadline, timer_period;
    unsigned timer_active;
    pwl_efi_event_notify_t notify;
    const void *context;
    pwl_efi_guid_t group;
    uint32_t type;
    unsigned signaled, grouped;
} pwl_resident_event_t;
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

#define PWL_RESIDENT_VARIABLES 16U
#define PWL_VARIABLE_NAME_UNITS 128U
#define PWL_VARIABLE_MAX_BYTES 1024U /* UTF-16 name (including NUL) + data. */
typedef struct pwl_resident_variable {
    pwl_efi_guid_t guid;
    uint16_t name[PWL_VARIABLE_NAME_UNITS];
    unsigned char data[PWL_VARIABLE_MAX_BYTES];
    size_t name_units, data_size;
    uint32_t attributes;
} pwl_resident_variable_t;
typedef struct pwl_resident_clock {
    uint64_t frequency_hz, last_tsc;
    unsigned ready;
} pwl_resident_clock_t;
#define PWL_RESIDENT_APPLICATIONS 8U
typedef struct pwl_image_mapping {
    uint64_t root, tables_base, tables_bytes, heap_base, heap_bytes;
    uint64_t boot_base, boot_bytes; /* Preloaded application, never freeable by child services. */
    uint64_t permission_callback; /* Audited resident destination; zero disables LoadImage. */
} pwl_image_mapping_t;
typedef struct pwl_image_jump {
    uint64_t gpr[8]; /* RBX, RBP, R12..R15, RSP, RIP. */
    unsigned char xmm[160]; /* Microsoft ABI nonvolatile XMM6..15. */
    uint32_t mxcsr;
    uint16_t fpcw, padding;
} pwl_image_jump_t;
_Static_assert(offsetof(pwl_image_jump_t,xmm)==64 && offsetof(pwl_image_jump_t,mxcsr)==224 &&
    offsetof(pwl_image_jump_t,fpcw)==228 && sizeof(pwl_image_jump_t)==232,"Image jump AMD64 layout");
typedef struct pwl_resident_application {
    pwl_pe_loaded_t mapped;
    pwl_efi_loaded_image_t protocol;
    pwl_image_jump_t jump;
    uint64_t handle, previous, exit_status, exit_data;
    size_t exit_bytes;
    unsigned running, quarantined, initial;
    unsigned char path[520];
} pwl_resident_application_t;
typedef struct pwl_resident_data {
    pwl_fw_memory_t memory;
    pwl_fw_media_t media;
    uint64_t tpl;
    pwl_efi_prepared_tables_t efi;
    pwl_resident_protocol_t protocols[PWL_RESIDENT_PROTOCOLS];
    uint64_t protocol_next_handle;
    pwl_resident_open_t opens[PWL_RESIDENT_OPENS];
    pwl_efi_loaded_image_t loaded_image;
    uint64_t filesystem[2]; /* revision and OpenVolume destination address */
    pwl_efi_file_protocol_t file_template;
    pwl_resident_file_t files[PWL_RESIDENT_FILES];
    unsigned files_enabled;
    unsigned boot_origin_bound;
    uint32_t boot_file_record;
    pwl_resident_variable_t variables[PWL_RESIDENT_VARIABLES];
    pwl_resident_clock_t clock; /* Must be explicitly calibrated/validated by platform preparation. */
    pwl_image_mapping_t image_mapping;
    uint64_t applications[PWL_RESIDENT_APPLICATIONS]; /* Owned pool addresses. */
    uint64_t current_application;
    pwl_resident_application_t initial_application;
    pwl_resident_graphics_t graphics;
    pwl_efi_configuration_t configuration[PWL_RESIDENT_CONFIGURATIONS];
    pwl_resident_event_t events[PWL_RESIDENT_EVENTS];
    uint64_t event_next_handle;
    uint64_t event_queue[2][PWL_RESIDENT_EVENTS]; /* FIFO at TPL_CALLBACK/NOTIFY. */
    unsigned event_queue_count[2];
    unsigned char boot_file_path[520]; /* FilePath node, UTF-16 name, End node. */
} pwl_resident_data_t;

/* Internal SysV helper; public notification callbacks use Microsoft AMD64 ABI. */
uint64_t pwl_resident_clock_now(pwl_resident_data_t *data,uint64_t *ticks);
uint64_t pwl_resident_clock_ticks(const pwl_resident_data_t *data,uint64_t units,
    uint64_t units_per_second,uint64_t *ticks);
void pwl_resident_events_dispatch(pwl_resident_data_t *data);
void pwl_resident_signal_group(pwl_resident_data_t *data,const pwl_efi_guid_t *group);
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
