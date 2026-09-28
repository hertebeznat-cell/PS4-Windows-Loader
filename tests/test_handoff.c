#include "pwl_handoff.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Synthetic test data only: not a PS4 RAM map. */
    pwl_phys_region_t map[] = {
        {0, 0x100000, PWL_MEMORY_RESERVED},
        {0x100000, 0x1000, PWL_MEMORY_LOADER_DATA},
        {0x101000, 0x1000, PWL_MEMORY_LOADER_DATA},
        {0x102000, 0x2000, PWL_MEMORY_LOADER_DATA},
        {0x104000, 0x3000, PWL_MEMORY_LOADER_CODE},
        {0x107000, 0x1000, PWL_MEMORY_FREE}
    };
    const size_t count = sizeof(map) / sizeof(map[0]);
    const pwl_handoff_layout_t good = {0x100000, 0x104000, 0x3000, 0x200,
                                        0x101000, 0x3000};
    pwl_handoff_layout_t bad;
    assert(pwl_memory_map_validate(NULL, count) == PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_memory_map_validate(map, 0) == PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_handoff_layout_validate(map, count, NULL) == PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_handoff_layout_validate(map, count, &good) == PWL_OK);

    map[2].base += 0x1000; /* Overlap the following region. */
    assert(pwl_memory_map_validate(map, count) == PWL_ERR_INVALID_ARGUMENT);
    map[2].base -= 0x1000;
    map[2].length = 0;
    assert(pwl_memory_map_validate(map, count) == PWL_ERR_INVALID_ARGUMENT);
    map[2].length = 0x1000;
    map[2].base++;
    assert(pwl_memory_map_validate(map, count) == PWL_ERR_INVALID_ARGUMENT);
    map[2].base--;
    map[5].base = UINT64_MAX - 4095;
    assert(pwl_memory_map_validate(map, count) == PWL_ERR_INVALID_ARGUMENT);
    map[5].base = 0x107000;
    map[5].kind = (pwl_memory_kind_t)99;
    assert(pwl_memory_map_validate(map, count) == PWL_ERR_INVALID_ARGUMENT);
    map[5].kind = PWL_MEMORY_FREE;

    bad = good; bad.entry_offset = bad.image_size;
    assert(pwl_handoff_layout_validate(map, count, &bad) == PWL_ERR_INVALID_ARGUMENT);
    bad = good; bad.page_table_root_pa = bad.stack_pa;
    assert(pwl_handoff_layout_validate(map, count, &bad) == PWL_ERR_INVALID_ARGUMENT);
    bad = good; bad.image_pa = 0x107000; bad.image_size = 0x1000;
    assert(pwl_handoff_layout_validate(map, count, &bad) == PWL_ERR_INVALID_ARGUMENT);
    bad = good; bad.stack_pa = UINT64_MAX - 4095;
    assert(pwl_handoff_layout_validate(map, count, &bad) == PWL_ERR_INVALID_ARGUMENT);
    bad = good; bad.page_table_root_pa = 0x108000; /* Unknown memory. */
    assert(pwl_handoff_layout_validate(map, count, &bad) == PWL_ERR_INVALID_ARGUMENT);
    map[2].kind = PWL_MEMORY_MMIO;
    assert(pwl_handoff_layout_validate(map, count, &good) == PWL_ERR_INVALID_ARGUMENT);
    map[2].kind = PWL_MEMORY_LOADER_DATA;
    map[3].length = 0x1000; /* Gap at the end of the stack. */
    assert(pwl_memory_map_validate(map, count) == PWL_OK);
    assert(pwl_handoff_layout_validate(map, count, &good) == PWL_ERR_INVALID_ARGUMENT);
    map[3].length = 0x2000;
    assert(pwl_handoff_layout_validate(map, count, &good) == PWL_OK);
    puts("physical map and handoff placement tests passed");
    return 0;
}
