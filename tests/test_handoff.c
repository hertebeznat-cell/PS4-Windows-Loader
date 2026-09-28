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
    uint64_t cache[] = {8, 8, 8, 8, 8, 1};
    pwl_efi_memory_descriptor_t descriptors[sizeof(map) / sizeof(map[0])];
    size_t written = 0;
    pwl_handoff_layout_t bad;
    assert(pwl_memory_map_validate(NULL, count) == PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_memory_map_validate(map, 0) == PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_handoff_layout_validate(map, count, NULL) == PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_handoff_layout_validate(map, count, &good) == PWL_OK);
    assert(pwl_efi_descriptors_from_regions(map, cache, count, NULL, 0,
                                             &written) == PWL_ERR_BUFFER_TOO_SMALL);
    assert(written == count);
    assert(pwl_efi_descriptors_from_regions(map, cache, count, descriptors,
                                             count - 1, &written) == PWL_ERR_BUFFER_TOO_SMALL);
    assert(pwl_efi_descriptors_from_regions(map, cache, count, descriptors,
                                             count, &written) == PWL_OK);
    assert(written == count && descriptors[0].type == 0 &&
           descriptors[0].number_of_pages == 256 &&
           descriptors[1].type == 2 && descriptors[4].type == 1 &&
           descriptors[5].type == 7 && descriptors[5].attribute == 1 &&
           descriptors[4].physical_start == 0x104000 &&
           descriptors[4].virtual_start == 0 && descriptors[4].padding == 0);
    cache[5] = 0; /* Cache type is unknown: no fabricated attributes. */
    assert(pwl_efi_descriptors_from_regions(map, cache, count, descriptors,
                                             count, &written) == PWL_ERR_INVALID_ARGUMENT);
    cache[5] = 1;
    map[4].base = 0x103000; /* Invalid map cannot become EFI descriptors. */
    assert(pwl_efi_descriptors_from_regions(map, cache, count, descriptors,
                                             count, &written) == PWL_ERR_INVALID_ARGUMENT);
    map[4].base = 0x104000;

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
