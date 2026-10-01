#include "pwl_ps4_memory.h"

/* FreeBSD allocator ABI used by the pinned runtime. Avoid its M_WAITOK retry
 * loop, and preserve the allocation KVA rather than replacing it by DMAP.
 */
#define M_NOWAIT 0x0001
#define M_ZERO 0x0100
#define VM_MEMATTR_WRITE_BACK 0x06

pwl_status_t pwl_ps4_arena_acquire(const pwl_ps4_memory_api_t *api,
                                  uint64_t size, pwl_ps4_arena_t *arena)
{
    uint64_t kva, pa, offset;
    pwl_status_t status;
    if (api == NULL || arena == NULL || arena->kernel_address != 0 ||
        arena->physical_address != 0 || arena->size != 0 || arena->used != 0 ||
        size == 0 || size % PWL_PS4_VM_PAGE_SIZE || size > SIZE_MAX)
        return PWL_ERR_INVALID_ARGUMENT;
    status = pwl_ps4_memory_api_validate(api);
    if (status != PWL_OK) return status;
    kva = api->alloc_contig(api->kernel_map, size, M_NOWAIT | M_ZERO,
                            PWL_PS4_PHYSICAL_MIN, PWL_PS4_IDENTITY_LIMIT,
                            (unsigned long)PWL_PS4_VM_PAGE_SIZE, 0,
                            VM_MEMATTR_WRITE_BACK);
    if (kva == 0) return PWL_ERR_OUT_OF_RESOURCES;
    if (kva % PWL_PS4_VM_PAGE_SIZE || size > UINT64_MAX - kva ||
        (api->firmware && kva<UINT64_C(0xffff800000000000))) {
        /* An unexpected KVA is not a validated owner to pass to free. Retain
         * its raw result and ABI for diagnosis rather than guessing cleanup. */
        arena->api=*api;arena->kernel_address=kva;arena->size=size;
        return PWL_ERR_INVALID_ARGUMENT;
    }
    pa = api->extract(api->kernel_pmap, kva);
    if (pa < PWL_PS4_PHYSICAL_MIN || pa % PWL_PS4_VM_PAGE_SIZE ||
        pa >= PWL_PS4_IDENTITY_LIMIT || size > PWL_PS4_IDENTITY_LIMIT - pa)
        goto invalid;
    for (offset = 0; offset < size; offset += PWL_PAGE_SIZE) {
        if (api->extract(api->kernel_pmap, kva + offset) != pa + offset ||
            api->extract(api->kernel_pmap, kva + offset + PWL_PAGE_SIZE - 1) !=
                pa + offset + PWL_PAGE_SIZE - 1)
            goto invalid;
    }
    arena->api = *api;
    arena->kernel_address = kva;
    arena->physical_address = pa;
    arena->size = size;
    arena->used = 0;
    return PWL_OK;
invalid:
    /* Cleanup rechecks the live binding too. If it refuses, preserve the
     * original owner so its callbacks and reader can remain pinned. */
    arena->api=*api;arena->kernel_address=kva;arena->size=size;
    status=pwl_ps4_arena_release(arena);
    return status==PWL_OK?PWL_ERR_INVALID_ARGUMENT:status;
}

pwl_status_t pwl_ps4_arena_take(pwl_ps4_arena_t *arena, uint64_t size,
                               uint64_t alignment, pwl_owned_span_t *span)
{
    uint64_t address, padding, offset;
    if (arena == NULL || span == NULL || arena->kernel_address == 0 ||
        arena->kernel_address%PWL_PS4_VM_PAGE_SIZE ||
        !arena->size || arena->size%PWL_PS4_VM_PAGE_SIZE ||
        arena->size>UINT64_MAX-arena->kernel_address ||
        arena->physical_address<PWL_PS4_PHYSICAL_MIN ||
        arena->physical_address%PWL_PS4_VM_PAGE_SIZE ||
        arena->physical_address>=PWL_PS4_IDENTITY_LIMIT ||
        arena->size>PWL_PS4_IDENTITY_LIMIT-arena->physical_address ||
        size == 0 || size % PWL_PAGE_SIZE || alignment < PWL_PAGE_SIZE ||
        (alignment & (alignment - 1)) || arena->used > arena->size)
        return PWL_ERR_INVALID_ARGUMENT;
    address = arena->physical_address + arena->used;
    padding = (alignment - (address & (alignment - 1))) & (alignment - 1);
    if (padding > arena->size - arena->used ||
        size > arena->size - arena->used - padding)
        return PWL_ERR_OUT_OF_RESOURCES;
    offset = arena->used + padding;
    span->prepare_address = (void *)(uintptr_t)(arena->kernel_address + offset);
    span->physical_address = arena->physical_address + offset;
    span->size = size;
    arena->used = offset + size;
    return PWL_OK;
}

pwl_status_t pwl_ps4_arena_release(pwl_ps4_arena_t *arena)
{
    pwl_status_t status;
    if (arena == NULL) return PWL_ERR_INVALID_ARGUMENT;
    if (arena->kernel_address == 0) {
        return arena->size || arena->used || arena->physical_address ?
            PWL_ERR_INVALID_ARGUMENT : PWL_OK;
    }
    if (arena->kernel_address % PWL_PS4_VM_PAGE_SIZE || arena->size == 0 ||
        arena->size % PWL_PS4_VM_PAGE_SIZE ||
        arena->size > UINT64_MAX - arena->kernel_address || arena->used > arena->size)
        return PWL_ERR_INVALID_ARGUMENT;
    status = pwl_ps4_memory_api_validate(&arena->api);
    if (status != PWL_OK) return status;
    if(arena->api.firmware && arena->kernel_address<UINT64_C(0xffff800000000000))
        return PWL_ERR_INVALID_ARGUMENT;
    arena->api.free(arena->api.kernel_map, arena->kernel_address, arena->size);
    *arena = (pwl_ps4_arena_t){0};
    return PWL_OK;
}
