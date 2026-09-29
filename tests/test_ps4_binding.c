#include "pwl_ps4_memory.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Valid C function pointers, deliberately fatal if the production gate leaks. */
static pwl_ps4_vm_u64_t forbidden_alloc(void *map, pwl_ps4_vm_u64_t size, int flags,
    pwl_ps4_vm_u64_t low, pwl_ps4_vm_u64_t high, unsigned long alignment,
    unsigned long boundary, char attribute)
{
    (void)map; (void)size; (void)flags; (void)low; (void)high;
    (void)alignment; (void)boundary; (void)attribute;
    abort();
}
static void forbidden_free(void *map, pwl_ps4_vm_u64_t kva, pwl_ps4_vm_u64_t size)
{
    (void)map; (void)kva; (void)size;
    abort();
}
static pwl_ps4_vm_u64_t forbidden_extract(void *map, pwl_ps4_vm_u64_t kva)
{
    (void)map; (void)kva;
    abort();
}

int main(void)
{
    const uint32_t versions[] = {1352, 1202, 1350, 0, UINT32_MAX};
    pwl_ps4_memory_api_t api;
    pwl_ps4_arena_t arena = {0};
    size_t i;
    int map;
    for (i = 0; i < sizeof(versions) / sizeof(versions[0]); ++i) {
        api = (pwl_ps4_memory_api_t){&map, &map, forbidden_alloc,
                                    forbidden_free, forbidden_extract, versions[i]};
        assert(pwl_ps4_memory_api_validate(&api) == PWL_ERR_UNSUPPORTED);
        assert(pwl_ps4_arena_acquire(&api, 16384, &arena) == PWL_ERR_UNSUPPORTED);
        assert(arena.kernel_address == 0 && arena.size == 0);
        /* Even a forged owner must not authorize a candidate free address. */
        arena.api = api;
        arena.kernel_address = UINT64_C(0xffff800004000000);
        arena.size = 16384;
        assert(pwl_ps4_arena_release(&arena) == PWL_ERR_UNSUPPORTED);
        assert(arena.kernel_address == UINT64_C(0xffff800004000000));
        assert(arena.size == 16384);
        arena = (pwl_ps4_arena_t){0};
        assert(pwl_ps4_memory_bind(versions[i], &api) == PWL_ERR_UNSUPPORTED);
        assert(api.firmware == versions[i]);
        assert(!api.kernel_map && !api.kernel_pmap);
        assert(!api.alloc_contig && !api.free && !api.extract);
        assert(pwl_ps4_arena_acquire(&api, 16384, &arena) == PWL_ERR_UNSUPPORTED);
    }
    assert(pwl_ps4_memory_bind(1352, NULL) == PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_ps4_memory_api_validate(NULL) == PWL_ERR_INVALID_ARGUMENT);
    puts("PS4 production binding: candidate, unknown and host IDs refused without kernel calls");
    return 0;
}
