#include "pwl_handoff.h"
#include <assert.h>

#define NX (UINT64_C(1) << 63)
#define GOOD() (pwl_x64_handoff_mappings_validate(regions, 3, &layout, tables, 4) == PWL_OK)

int main(void)
{
    const pwl_phys_region_t regions[] = {
        {0x100000, 0x4000, PWL_MEMORY_LOADER_DATA},
        {0x104000, 0x3000, PWL_MEMORY_LOADER_CODE},
        {0x107000, 0x2000, PWL_MEMORY_LOADER_DATA}
    };
    const pwl_handoff_layout_t layout = {0x100000, 0x104000, 0x3000, 0x200, 0x107000, 0x2000};
    uint64_t entries[4][512] = {{0}};
    pwl_x64_table_page_t tables[] = {
        {0x100000, entries[0]}, {0x101000, entries[1]},
        {0x102000, entries[2]}, {0x103000, entries[3]}
    };
    unsigned i;
    entries[0][0] = 0x101003;
    entries[1][0] = 0x102003;
    entries[2][0] = 0x103003;
    for (i = 0x100; i <= 0x108; ++i)
        entries[3][i] = ((uint64_t)i << 12) | 3 |
            ((i < 0x104 || i >= 0x107) ? NX : 0);
    assert(GOOD());
    entries[3][0x105] += 0x1000;
    assert(!GOOD());
    entries[3][0x105] -= 0x1000;
    entries[3][0x104] |= NX;
    assert(!GOOD());
    entries[3][0x104] &= ~NX;
    entries[3][0x107] &= ~NX;
    assert(!GOOD());
    entries[3][0x107] |= NX;
    entries[2][0] |= 128;
    assert(!GOOD());
    entries[2][0] &= ~UINT64_C(128);
    entries[0][0] |= 4;
    assert(!GOOD());
    entries[0][0] &= ~UINT64_C(4);
    assert(pwl_x64_handoff_mappings_validate(regions, 3, &layout, tables, 3) != PWL_OK);
    tables[3].physical_address = tables[2].physical_address;
    assert(!GOOD());
    tables[3].physical_address = 0x103000;
    tables[3].physical_address = 0x107000;
    assert(!GOOD());
    return 0;
}
