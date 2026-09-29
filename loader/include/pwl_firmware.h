#ifndef PWL_FIRMWARE_H
#define PWL_FIRMWARE_H

#include "pwl_handoff.h"

/* These services have no libc, kernel or process imports. All state is inline
 * and can live in an owned, identity-mapped firmware data span. This is the
 * memory/media substrate, not a complete EFI System Table or bootable firmware.
 * Calls must be serialized; a future EFI/TPL layer must prevent reentrancy.
 */
#define PWL_FW_MAX_DESCRIPTORS 256U
#define PWL_EFI_ERROR(n) ((UINT64_C(1) << 63) | (n))
#define PWL_EFI_SUCCESS UINT64_C(0)
#define PWL_EFI_INVALID_PARAMETER PWL_EFI_ERROR(2)
#define PWL_EFI_UNSUPPORTED PWL_EFI_ERROR(3)
#define PWL_EFI_BAD_BUFFER_SIZE PWL_EFI_ERROR(4)
#define PWL_EFI_BUFFER_TOO_SMALL PWL_EFI_ERROR(5)
#define PWL_EFI_WRITE_PROTECTED PWL_EFI_ERROR(8)
#define PWL_EFI_OUT_OF_RESOURCES PWL_EFI_ERROR(9)
#define PWL_EFI_MEDIA_CHANGED PWL_EFI_ERROR(13)
#define PWL_EFI_NOT_FOUND PWL_EFI_ERROR(14)
#define PWL_EFI_ACCESS_DENIED PWL_EFI_ERROR(15)

enum pwl_fw_allocate_type { PWL_ALLOCATE_ANY, PWL_ALLOCATE_MAX, PWL_ALLOCATE_ADDRESS };

typedef struct pwl_fw_memory_entry {
    pwl_efi_memory_descriptor_t descriptor;
    unsigned allocated; /* Only pages allocated by this manager may be freed. */
} pwl_fw_memory_entry_t;

typedef struct pwl_fw_memory {
    pwl_fw_memory_entry_t entries[PWL_FW_MAX_DESCRIPTORS];
    size_t count;
    uint64_t key;
    uint64_t issued_key;
    uint64_t image_handle;
    unsigned exited;
} pwl_fw_memory_t;

/* Input must be an actual platform inventory with owned free extents only.
 * Unknown holes are never filled. Pre-existing code/data is not freeable.
 * Call on fresh storage, before publishing any pointer to this manager.
 */
pwl_status_t pwl_fw_memory_init(pwl_fw_memory_t *memory,
                                const pwl_phys_region_t *regions,
                                const uint64_t *cacheability, size_t count,
                                uint64_t image_handle);
uint64_t pwl_fw_allocate_pages(pwl_fw_memory_t *memory, unsigned allocation_type,
                               unsigned memory_type, uint64_t pages,
                               uint64_t *address);
uint64_t pwl_fw_free_pages(pwl_fw_memory_t *memory, uint64_t address, uint64_t pages);
uint64_t pwl_fw_get_memory_map(pwl_fw_memory_t *memory, size_t *size,
                              pwl_efi_memory_descriptor_t *map, uint64_t *key,
                              size_t *descriptor_size, uint32_t *version);
/* Memory part of ExitBootServices only: validates a successfully issued map
 * key and retires this manager. Platform events, timers, SystemTable CRC,
 * runtime mappings and device/AP handoff still belong to the future EFI layer.
 */
uint64_t pwl_fw_memory_exit(pwl_fw_memory_t *memory, uint64_t image_handle,
                           uint64_t key);

typedef struct pwl_fw_media {
    uint64_t physical_address;
    uint64_t bytes;
    uint32_t block_size;
    uint32_t media_id;
} pwl_fw_media_t;

/* A preloaded read-only disk image, kept in owned RAM before leaving Orbis.
 * No USB handles survive the boundary. Mapping is explicit to let the same
 * code run against a preparation KVA in tests and the identity PA after entry.
 * The real EFI BlockIo wrapper must bind mapping to the resident media span.
 */
pwl_status_t pwl_fw_media_init(pwl_fw_media_t *media, uint64_t physical_address,
                               uint64_t bytes, uint32_t block_size,
                               uint32_t media_id);
uint64_t pwl_fw_media_read(const pwl_fw_media_t *media, const void *mapping,
                          uint32_t media_id, uint64_t lba, size_t bytes,
                          void *buffer);
uint64_t pwl_fw_media_write(const pwl_fw_media_t *media, uint32_t media_id);

#endif
