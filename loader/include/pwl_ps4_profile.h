#ifndef PWL_PS4_PROFILE_H
#define PWL_PS4_PROFILE_H
#include "pwl_ps4_memory.h"
typedef struct pwl_ps4_observation {
    uint64_t lstar, thread, root, flags;
    uint16_t cs;
} pwl_ps4_observation_t;
typedef struct pwl_ps4_profile {
    uint64_t base, map, pmap, alloc_contig, free, extract;
} pwl_ps4_profile_t;
/* Pure profile inspection over protected reader data. No function pointers,
 * allocation or privileged operations. Production binder MUST independently
 * obtain observations from the CPU; a supplied observation is not a binding.
 * Both observed callback state and this same-build byte profile must match.
 * Output unchanged on failure. Reader output must be stable through a call.
 */
pwl_status_t pwl_ps4_profile_inspect(const pwl_ps4_observation_t *observation,
    pwl_ps4_kernel_read_fn read,void *context,pwl_ps4_profile_t *out);
#endif
