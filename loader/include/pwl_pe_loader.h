#ifndef PWL_PE_LOADER_H
#define PWL_PE_LOADER_H

#include "pwl_handoff.h"

#define PWL_PE_MAX_SECTIONS 96U
#define PWL_PE_MAX_RANGES (2U * PWL_PE_MAX_SECTIONS + 1U)

typedef struct pwl_pe_loaded {
    uint64_t physical_address, image_size, entry_address;
    pwl_x64_identity_range_t ranges[PWL_PE_MAX_RANGES];
    size_t range_count;
} pwl_pe_loaded_t;

/* AMD64 EFI application, page-aligned sections, no W+X or dynamic imports/TLS.
 * Size query validates layout, not the relocation stream. No code is executed.
 */
pwl_status_t pwl_pe_efi_size(const void *file, size_t bytes, uint64_t *size);

/* Maps into caller-owned writable preparation memory, relocating to target PA
 * rather than the preparation pointer. Source, destination and output must be
 * disjoint and exclusively owned for this call. No syscall/allocator/imports.
 * On layout/capacity failure destination is unchanged. On relocation failure
 * copied destination bytes are cleared and output stays zero. Never publish
 * destination or activate its page mappings until PWL_OK.
 * This is not EFI LoadImage/StartImage, signature validation or CPU handoff.
 */
pwl_status_t pwl_pe_load_efi(const void *file, size_t bytes,
                            void *destination, size_t capacity,
                            uint64_t target_pa, pwl_pe_loaded_t *out);

#endif
