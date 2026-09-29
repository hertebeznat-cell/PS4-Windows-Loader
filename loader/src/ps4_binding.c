#include "pwl_ps4_memory.h"

#if defined(PWL_PS4_MEMORY_HOST_TEST) && !__STDC_HOSTED__
#error "Host memory fixtures must never be enabled in a freestanding build"
#endif

pwl_status_t pwl_ps4_memory_bind(uint32_t firmware, pwl_ps4_memory_api_t *api)
{
    if (api == NULL) return PWL_ERR_INVALID_ARGUMENT;
    *api = (pwl_ps4_memory_api_t){0};
    api->firmware = firmware;
    /* Deliberately no approved profiles, address arithmetic or dereferences.
     * The runtime's PS4_13_52/from-12.02 table is not binding evidence.
     * Adding a profile requires same-build verification and context checks;
     * a firmware number or caller-provided "verified" flag is insufficient.
     */
    return PWL_ERR_UNSUPPORTED;
}

pwl_status_t pwl_ps4_memory_api_validate(const pwl_ps4_memory_api_t *api)
{
    if (api == NULL) return PWL_ERR_INVALID_ARGUMENT;
#ifdef PWL_PS4_MEMORY_HOST_TEST
    /* Compiled only into host integration tests, never the distributed core. */
    if (api->firmware == 0) {
        if (api->kernel_map == NULL || api->kernel_pmap == NULL ||
            api->alloc_contig == NULL || api->free == NULL || api->extract == NULL)
            return PWL_ERR_INVALID_ARGUMENT;
        return PWL_OK;
    }
#endif
    return PWL_ERR_UNSUPPORTED;
}
