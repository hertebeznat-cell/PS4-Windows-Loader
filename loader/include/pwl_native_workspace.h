#ifndef PWL_NATIVE_WORKSPACE_H
#define PWL_NATIVE_WORKSPACE_H

#include "pwl_ps4_memory.h"
#include "pwl_firmware.h"
#include "pwl_pe_loader.h"
#include "pwl_resident.h"

#define PWL_NATIVE_MAX_TABLES 128U
#define PWL_NATIVE_MAX_REGIONS 9U

typedef struct pwl_native_request {
    /* Resident firmware blob only; copying does NOT relocate PE/ELF code. */
    const void *firmware;
    size_t firmware_bytes;
    const void *disk_image;
    size_t disk_bytes;
    uint64_t heap_bytes;
    uint64_t stack_bytes;
    size_t table_pages;
    uint64_t image_handle;
    /* Optional AMD64 EFI application. Copied and relocated, never entered. */
    const void *boot_image;
    size_t boot_image_bytes;
} pwl_native_request_t;

typedef pwl_resident_data_t pwl_native_data_t;

/* Preparation-side owner. Its pointers must never be handed to EFI.
 * Only the physical addresses identify objects in the new identity context.
 */
typedef struct pwl_native_workspace {
    pwl_ps4_arena_t arena;
    pwl_owned_span_t firmware, data, tables_span, stack, media, heap, boot;
    pwl_pe_loaded_t boot_image;
    pwl_phys_region_t regions[PWL_NATIVE_MAX_REGIONS];
    size_t region_count;
    pwl_x64_identity_range_t mappings[6 + PWL_PE_MAX_RANGES];
    size_t mapping_count;
    pwl_x64_table_page_t tables[PWL_NATIVE_MAX_TABLES];
    size_t table_count;
} pwl_native_workspace_t;

/* Performs allocation -> physical verification -> resident copies -> memory
 * and media initialization -> independent page-table construction. Stack guard
 * pages are owned/reserved but unmapped. All failures unwind the kernel owner.
 * Relocates the optional EFI image to its owned PA and maps code/data with
 * section permissions. The firmware blob still needs its own relocation.
 * Does NOT switch CR3, install an EFI System Table, call code,
 * or provide a complete machine memory map. Data pointers/callback relocation,
 * AP/IRQ/DMA state, PAT/MTRR and recoverable CPU transition are still required.
 */
pwl_status_t pwl_native_workspace_prepare(const pwl_ps4_memory_api_t *api,
                                          const pwl_native_request_t *request,
                                          pwl_native_workspace_t *workspace);
/* On release refusal the owner and spans remain intact for diagnosis/retry. */
pwl_status_t pwl_native_workspace_release(pwl_native_workspace_t *workspace);

/* Copies the audited position-independent service image, binds its state to
 * the data PA and prepares tables. Never executes code or activates tables.
 */
pwl_status_t pwl_native_workspace_prepare_resident(const pwl_ps4_memory_api_t *api,
    const pwl_native_request_t *request,const pwl_resident_image_t *image,
    pwl_native_workspace_t *workspace);

/* Select the EFI application from the validated resident file archive, map it
 * together with services, and bind LoadedImage to its actual file/volume.
 * request.boot_image must be empty. Preparation only: no CPU transition or
 * Microsoft code execution. All source bytes may be discarded after success. */
pwl_status_t pwl_native_boot_prepare(const pwl_ps4_memory_api_t *api,
    const pwl_native_request_t *request,const pwl_resident_image_t *image,
    const uint16_t *boot_path,pwl_native_workspace_t *workspace);

/* Preparation-only audit of the owned resident environment, including the
 * optional relocated EFI application's extent, entry and page permissions.
 * Does not certify
 * CPU transition readiness or provide a platform memory map. */
pwl_status_t pwl_native_resident_environment_validate(
    const pwl_native_workspace_t *workspace, const pwl_resident_image_t *image);

#endif
