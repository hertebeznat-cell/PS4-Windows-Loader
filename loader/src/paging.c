#include "pwl_handoff.h"

#define PWL_X64_PRESENT UINT64_C(1)
#define PWL_X64_WRITE UINT64_C(2)
#define PWL_X64_USER UINT64_C(4)
#define PWL_X64_LARGE UINT64_C(128)
#define PWL_X64_NX (UINT64_C(1) << 63)
#define PWL_X64_ADDR UINT64_C(0x000ffffffffff000)
#define PWL_X64_LOWER_LIMIT (UINT64_C(1) << 47)

static const pwl_x64_table_page_t *table_at(const pwl_x64_table_page_t *tables,
                                           size_t count, uint64_t address)
{
    size_t i;
    for (i = 0; i < count; ++i)
        if (tables[i].physical_address == address)
            return &tables[i];
    return NULL;
}

static int owned_data_page(const pwl_phys_region_t *regions, size_t count,
                           uint64_t address)
{
    size_t i;
    for (i = 0; i < count; ++i)
        if (regions[i].kind == PWL_MEMORY_LOADER_DATA &&
            address >= regions[i].base &&
            address - regions[i].base < regions[i].length)
            return 1;
    return 0;
}

static int walk_identity(const pwl_x64_table_page_t *tables, size_t count,
                         uint64_t root, uint64_t address, int needs_write,
                         int needs_execute)
{
    static const unsigned shifts[] = {39, 30, 21, 12};
    unsigned level;
    uint64_t next = root, leaf = 0;
    for (level = 0; level < 4; ++level) {
        const pwl_x64_table_page_t *page = table_at(tables, count, next);
        uint64_t entry;
        if (page == NULL)
            return 0;
        entry = page->entries[(address >> shifts[level]) & 511U];
        if (!(entry & PWL_X64_PRESENT) || (entry & PWL_X64_USER) ||
            (needs_write && !(entry & PWL_X64_WRITE)) ||
            (needs_execute && (entry & PWL_X64_NX)) ||
            (level != 3 && (entry & PWL_X64_LARGE)))
            return 0;
        if (level == 3)
            leaf = entry;
        next = entry & PWL_X64_ADDR;
    }
    return next == address && (needs_execute || (leaf & PWL_X64_NX));
}

static int walk_range(const pwl_x64_table_page_t *tables, size_t count,
                      uint64_t root, uint64_t base, uint64_t length,
                      int write, int execute)
{
    uint64_t page;
    if (!length || (base & (PWL_PAGE_SIZE - 1)) ||
        (length & (PWL_PAGE_SIZE - 1)) ||
        base >= PWL_X64_LOWER_LIMIT ||
        length > PWL_X64_LOWER_LIMIT - base)
        return 0;
    for (page = base; page < base + length; page += PWL_PAGE_SIZE)
        if (!walk_identity(tables, count, root, page, write, execute))
            return 0;
    return 1;
}

pwl_status_t pwl_x64_handoff_mappings_validate(
    const pwl_phys_region_t *regions, size_t region_count,
    const pwl_handoff_layout_t *layout,
    const pwl_x64_table_page_t *tables, size_t table_count)
{
    size_t i, j;
    if (pwl_handoff_layout_validate(regions, region_count, layout) != PWL_OK ||
        tables == NULL || table_count < 4)
        return PWL_ERR_INVALID_ARGUMENT;
    for (i = 0; i < table_count; ++i) {
        uint64_t pa = tables[i].physical_address;
        if (tables[i].entries == NULL || (pa & (PWL_PAGE_SIZE - 1)) ||
            !owned_data_page(regions, region_count, pa) ||
            (pa >= layout->stack_pa && pa - layout->stack_pa < layout->stack_size))
            return PWL_ERR_INVALID_ARGUMENT;
        for (j = 0; j < i; ++j)
            if (tables[j].physical_address == pa)
                return PWL_ERR_INVALID_ARGUMENT;
    }
    if (table_at(tables, table_count, layout->page_table_root_pa) == NULL ||
        !walk_range(tables, table_count, layout->page_table_root_pa,
                    layout->image_pa, layout->image_size, 0, 1) ||
        !walk_range(tables, table_count, layout->page_table_root_pa,
                    layout->stack_pa, layout->stack_size, 1, 0))
        return PWL_ERR_INVALID_ARGUMENT;
    for (i = 0; i < table_count; ++i)
        if (!walk_range(tables, table_count, layout->page_table_root_pa,
                        tables[i].physical_address, PWL_PAGE_SIZE, 1, 0))
            return PWL_ERR_INVALID_ARGUMENT;
    return PWL_OK;
}
