#include "pwl_handoff.h"
#include <assert.h>

int main(void)
{
    const pwl_phys_region_t regions[] = {
        {0x100000, 0x6000, PWL_MEMORY_LOADER_DATA},
        {0x106000, 0x3000, PWL_MEMORY_LOADER_CODE},
        {0x109000, 0x2000, PWL_MEMORY_LOADER_DATA}
    };
    const pwl_handoff_layout_t layout = {
        0x100000, 0x106000, 0x3000, 0x200, 0x109000, 0x2000
    };
    uint64_t pages[6][512] = {{0}};
    pwl_x64_table_page_t tables[6];
    size_t used = 0, i;
    for (i = 0; i < 6; ++i) {
        tables[i].physical_address = 0x100000 + i * 0x1000;
        tables[i].entries = pages[i];
    }
    assert(pwl_x64_handoff_tables_build(regions, 3, &layout, tables, 6, &used) == PWL_OK);
    assert(used == 4);
    assert(pwl_x64_handoff_mappings_validate(regions, 3, &layout, tables, used) == PWL_OK);
    assert(pwl_x64_handoff_tables_build(regions, 3, &layout, tables, 3, &used) != PWL_OK);
    tables[4].physical_address = tables[0].physical_address;
    assert(pwl_x64_handoff_tables_build(regions, 3, &layout, tables, 6, &used) != PWL_OK);
    return 0;
}
