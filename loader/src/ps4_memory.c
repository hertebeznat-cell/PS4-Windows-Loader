#include "pwl_ps4_memory.h"

/* FreeBSD allocator ABI used by the pinned runtime. Avoid its M_WAITOK retry
 * loop, and preserve the allocation KVA rather than replacing it by DMAP.
 */
#define M_NOWAIT 0x0001
#define M_ZERO 0x0100
#define VM_MEMATTR_WRITE_BACK 0x06
#define IDENTITY_LIMIT (UINT64_C(1) << 47)

pwl_status_t pwl_ps4_arena_acquire(const pwl_ps4_memory_api_t *api,
                                  uint64_t size, pwl_ps4_arena_t *arena)
{
    uint64_t kva, pa, offset;
    if (api == NULL || arena == NULL || arena->kernel_address != 0 ||
        api->kernel_map == NULL || api->kernel_pmap == NULL ||
        api->alloc_contig == NULL || api->free == NULL || api->extract == NULL ||
        size == 0 || size % PWL_PS4_VM_PAGE_SIZE || size > SIZE_MAX)
        return PWL_ERR_INVALID_ARGUMENT;
    kva = api->alloc_contig(api->kernel_map, size, M_NOWAIT | M_ZERO,
                            0, IDENTITY_LIMIT - 1,
                            (unsigned long)PWL_PS4_VM_PAGE_SIZE, 0,
                            VM_MEMATTR_WRITE_BACK);
    if (kva == 0) return PWL_ERR_OUT_OF_RESOURCES;
    if (kva % PWL_PS4_VM_PAGE_SIZE || size > UINT64_MAX - kva)
        goto invalid;
    pa = api->extract(api->kernel_pmap, kva);
    if (pa == 0 || pa % PWL_PS4_VM_PAGE_SIZE || pa >= IDENTITY_LIMIT ||
        size > IDENTITY_LIMIT - pa)
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
    api->free(api->kernel_map, kva, size);
    return PWL_ERR_INVALID_ARGUMENT;
}

pwl_status_t pwl_ps4_arena_take(pwl_ps4_arena_t *arena, uint64_t size,
                               uint64_t alignment, pwl_owned_span_t *span)
{
    uint64_t address, padding, offset;
    if (arena == NULL || span == NULL || arena->kernel_address == 0 ||
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

void pwl_ps4_arena_release(pwl_ps4_arena_t *arena)
{
    if (arena != NULL && arena->kernel_address != 0) {
        arena->api.free(arena->api.kernel_map, arena->kernel_address, arena->size);
        *arena = (pwl_ps4_arena_t){0};
    }
}
