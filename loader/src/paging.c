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

static int map_page(pwl_x64_table_page_t *tables, size_t capacity,
                    size_t *used, uint64_t address, uint64_t flags)
{
    static const unsigned shifts[] = {39, 30, 21, 12};
    size_t index = 0;
    unsigned level;
    for (level = 0; level < 3; ++level) {
        uint64_t *entry = &tables[index].entries[(address >> shifts[level]) & 511U];
        if (!(*entry & PWL_X64_PRESENT)) {
            if (*used == capacity) return 0;
            *entry = tables[*used].physical_address | PWL_X64_PRESENT | PWL_X64_WRITE;
            index = (*used)++;
        } else {
            const pwl_x64_table_page_t *next = table_at(tables, *used, *entry & PWL_X64_ADDR);
            if (next == NULL || (*entry & (PWL_X64_USER | PWL_X64_LARGE | PWL_X64_NX)))
                return 0;
            index = (size_t)(next - tables);
        }
    }
    {
        uint64_t *leaf = &tables[index].entries[(address >> 12) & 511U];
        uint64_t wanted = address | PWL_X64_PRESENT | flags;
        if (*leaf && *leaf != wanted) return 0;
        *leaf = wanted;
    }
    return 1;
}

static int map_span(pwl_x64_table_page_t *tables, size_t capacity,
                    size_t *used, uint64_t base, uint64_t length, uint64_t flags)
{
    uint64_t offset;
    if (base >= PWL_X64_LOWER_LIMIT ||
        length > PWL_X64_LOWER_LIMIT - base) return 0;
    for (offset = 0; offset < length; offset += PWL_PAGE_SIZE)
        if (!map_page(tables, capacity, used, base + offset, flags)) return 0;
    return 1;
}

pwl_status_t pwl_x64_handoff_tables_build(
    const pwl_phys_region_t *regions, size_t region_count,
    const pwl_handoff_layout_t *layout,
    pwl_x64_table_page_t *tables, size_t table_capacity,
    size_t *table_count_out)
{
    size_t i, j, used = 1;
    if (table_count_out == NULL || tables == NULL || table_capacity < 4 ||
        pwl_handoff_layout_validate(regions, region_count, layout) != PWL_OK ||
        tables[0].physical_address != layout->page_table_root_pa)
        return PWL_ERR_INVALID_ARGUMENT;
    *table_count_out = 0;
    for (i = 0; i < table_capacity; ++i) {
        uint64_t pa = tables[i].physical_address;
        if (tables[i].entries == NULL || (pa & (PWL_PAGE_SIZE - 1)) ||
            !owned_data_page(regions, region_count, pa) ||
            (pa >= layout->stack_pa && pa - layout->stack_pa < layout->stack_size))
            return PWL_ERR_INVALID_ARGUMENT;
        for (j = 0; j < i; ++j)
            if (tables[j].physical_address == pa ||
                tables[j].entries == tables[i].entries)
                return PWL_ERR_INVALID_ARGUMENT;
    }
    for (i = 0; i < table_capacity; ++i)
        for (j = 0; j < 512; ++j) tables[i].entries[j] = 0;
    if (!map_span(tables, table_capacity, &used, layout->image_pa,
                  layout->image_size, PWL_X64_WRITE) ||
        !map_span(tables, table_capacity, &used, layout->stack_pa,
                  layout->stack_size, PWL_X64_WRITE | PWL_X64_NX))
        return PWL_ERR_BUFFER_TOO_SMALL;
    for (i = 0; i < used; ++i)
        if (!map_page(tables, table_capacity, &used, tables[i].physical_address,
                      PWL_X64_WRITE | PWL_X64_NX))
            return PWL_ERR_BUFFER_TOO_SMALL;
    if (pwl_x64_handoff_mappings_validate(regions, region_count, layout,
                                           tables, used) != PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    *table_count_out = used;
    return PWL_OK;
}

static int identity_inputs(const pwl_x64_identity_range_t *ranges, size_t count,
                           const pwl_x64_table_page_t *tables, size_t capacity)
{
    size_t i, j;
    uint64_t previous_end = 0;
    if (!ranges || !count || !tables || capacity < 4 ||
        capacity > PWL_X64_MAX_IDENTITY_TABLES) return 0;
    for (i = 0; i < count; ++i) {
        const pwl_x64_identity_range_t *r = &ranges[i];
        if (!r->base || !r->size || r->base % PWL_PAGE_SIZE ||
            r->size % PWL_PAGE_SIZE || r->base < previous_end ||
            r->base >= PWL_X64_LOWER_LIMIT ||
            r->size > PWL_X64_LOWER_LIMIT - r->base ||
            r->writable > 1 || r->executable > 1) return 0;
        previous_end = r->base + r->size;
    }
    for (i = 0; i < capacity; ++i) {
        uint64_t pa = tables[i].physical_address;
        uintptr_t va = (uintptr_t)tables[i].entries;
        int covered = 0;
        if (!va || va % sizeof(uint64_t) || va > UINTPTR_MAX - PWL_PAGE_SIZE ||
            !pa || pa % PWL_PAGE_SIZE) return 0;
        for (j = 0; j < i; ++j) {
            uintptr_t other = (uintptr_t)tables[j].entries;
            if (pa == tables[j].physical_address ||
                (va < other + PWL_PAGE_SIZE && other < va + PWL_PAGE_SIZE)) return 0;
        }
        for (j = 0; j < count; ++j)
            if (ranges[j].writable && !ranges[j].executable &&
                pa >= ranges[j].base && pa - ranges[j].base < ranges[j].size)
                covered = 1;
        if (!covered) return 0;
    }
    return 1;
}

/* Require exact leaf permissions, no unexpected leaf/branch flag bits, and
 * reject mappings outside the manifest (including user/large/global aliases).
 */
static int identity_tree(const pwl_x64_identity_range_t *ranges, size_t count,
                          const pwl_x64_table_page_t *tables, size_t table_count,
                          size_t page_index, unsigned level, uint64_t prefix,
                          uint64_t *seen, size_t *visited)
{
    static const unsigned shifts[] = {39, 30, 21, 12};
    size_t i, j;
    uint64_t bit = UINT64_C(1) << (page_index % 64);
    if (seen[page_index / 64] & bit) return 0;
    seen[page_index / 64] |= bit;
    ++*visited;
    for (i = 0; i < 512; ++i) {
        uint64_t e = tables[page_index].entries[i];
        uint64_t address = prefix | ((uint64_t)i << shifts[level]);
        if (!e) continue;
        if (level < 3) {
            const pwl_x64_table_page_t *next;
            if ((e & ~PWL_X64_ADDR) != (PWL_X64_PRESENT | PWL_X64_WRITE)) return 0;
            next = table_at(tables, table_count, e & PWL_X64_ADDR);
            if (!next || !identity_tree(ranges, count, tables, table_count,
                         (size_t)(next - tables), level + 1, address, seen, visited)) return 0;
        } else {
            int found = 0;
            for (j = 0; j < count; ++j) {
                const pwl_x64_identity_range_t *r = &ranges[j];
                uint64_t flags = PWL_X64_PRESENT |
                    (r->writable ? PWL_X64_WRITE : 0) | (r->executable ? 0 : PWL_X64_NX);
                if (address >= r->base && address - r->base < r->size) {
                    if (e != (address | flags)) return 0;
                    found = 1;
                    break;
                }
            }
            if (!found) return 0;
        }
    }
    return 1;
}

pwl_status_t pwl_x64_identity_mappings_validate(
    const pwl_x64_identity_range_t *ranges, size_t range_count,
    const pwl_x64_table_page_t *tables, size_t table_count)
{
    size_t i, visited = 0;
    uint64_t seen[PWL_X64_MAX_IDENTITY_TABLES / 64] = {0};
    if (!identity_inputs(ranges, range_count, tables, table_count) ||
        !identity_tree(ranges, range_count, tables, table_count, 0, 0, 0, seen, &visited) ||
        visited != table_count) return PWL_ERR_INVALID_ARGUMENT;
    for (i = 0; i < range_count; ++i)
        if (!walk_range(tables, table_count, tables[0].physical_address,
                        ranges[i].base, ranges[i].size,
                        ranges[i].writable, ranges[i].executable))
            return PWL_ERR_INVALID_ARGUMENT;
    return PWL_OK;
}

pwl_status_t pwl_x64_identity_tables_build(
    const pwl_x64_identity_range_t *ranges, size_t range_count,
    pwl_x64_table_page_t *tables, size_t table_capacity, size_t *table_count_out)
{
    size_t i, j, used = 1;
    if (!table_count_out) return PWL_ERR_INVALID_ARGUMENT;
    *table_count_out = 0;
    if (!identity_inputs(ranges, range_count, tables, table_capacity))
        return PWL_ERR_INVALID_ARGUMENT;
    for (i = 0; i < table_capacity; ++i)
        for (j = 0; j < 512; ++j) tables[i].entries[j] = 0;
    for (i = 0; i < range_count; ++i) {
        uint64_t flags = (ranges[i].writable ? PWL_X64_WRITE : 0) |
                         (ranges[i].executable ? 0 : PWL_X64_NX);
        if (!map_span(tables, table_capacity, &used, ranges[i].base,
                       ranges[i].size, flags)) return PWL_ERR_BUFFER_TOO_SMALL;
    }
    if (pwl_x64_identity_mappings_validate(ranges, range_count, tables, used) != PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    *table_count_out = used;
    return PWL_OK;
}
