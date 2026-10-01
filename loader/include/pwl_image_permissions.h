#ifndef PWL_IMAGE_PERMISSIONS_H
#define PWL_IMAGE_PERMISSIONS_H
#include "pwl_resident.h"
/* Owned, identity-accessible, exclusively used four-level table pool. Validates
 * every leaf before editing any. Does not invalidate translations or certify
 * CPU/AP/device state. restore=1 returns the image span to writable NX pages.
 * apply=0 validates the prospective edit; 1 applies it; 2 checks existing
 * leaf permissions exactly without editing. Preloaded boot pages cannot be
 * restored/freed through this helper.
 */
uint64_t pwl_image_permissions(pwl_image_mapping_t *mapping,
    const pwl_pe_loaded_t *image,unsigned restore,unsigned apply);
#endif
