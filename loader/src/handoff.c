#include "pwl_handoff.h"

static int valid_extent(uint64_t base, uint64_t length)
{
    return length != 0 && (base % PWL_PAGE_SIZE) == 0 &&
           (length % PWL_PAGE_SIZE) == 0 && length <= UINT64_MAX - base;
}

pwl_status_t pwl_memory_map_validate(const pwl_phys_region_t *regions,
                                    size_t count)
{
    size_t i;
    uint64_t end = 0;
    if (regions == NULL || count == 0)
        return PWL_ERR_INVALID_ARGUMENT;
    for (i = 0; i < count; ++i) {
        const pwl_phys_region_t *r = &regions[i];
        if (!valid_extent(r->base, r->length) ||
            r->kind < PWL_MEMORY_RESERVED || r->kind > PWL_MEMORY_MMIO ||
            (i != 0 && r->base < end))
            return PWL_ERR_INVALID_ARGUMENT;
        end = r->base + r->length;
    }
    return PWL_OK;
}

/* A span may cross adjacent regions of the same kind, but never a gap. */
static int covered(const pwl_phys_region_t *regions, size_t count,
                   uint64_t base, uint64_t length, pwl_memory_kind_t kind)
{
    size_t i;
    uint64_t cursor = base, end = base + length;
    for (i = 0; i < count; ++i) {
        const pwl_phys_region_t *r = &regions[i];
        uint64_t region_end = r->base + r->length;
        if (region_end <= cursor)
            continue;
        if (r->base > cursor || r->kind != kind)
            return 0;
        if (region_end >= end)
            return 1;
        cursor = region_end;
    }
    return 0;
}

static int overlaps(uint64_t a, uint64_t an, uint64_t b, uint64_t bn)
{
    return a < b + bn && b < a + an;
}

pwl_status_t pwl_handoff_layout_validate(const pwl_phys_region_t *regions,
                                        size_t count,
                                        const pwl_handoff_layout_t *layout)
{
    uint64_t root;
    if (layout == NULL || pwl_memory_map_validate(regions, count) != PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    root = layout->page_table_root_pa;
    if (!valid_extent(root, PWL_PAGE_SIZE) ||
        !valid_extent(layout->image_pa, layout->image_size) ||
        !valid_extent(layout->stack_pa, layout->stack_size) ||
        layout->entry_offset >= layout->image_size)
        return PWL_ERR_INVALID_ARGUMENT;
    if (overlaps(root, PWL_PAGE_SIZE, layout->stack_pa, layout->stack_size) ||
        overlaps(root, PWL_PAGE_SIZE, layout->image_pa, layout->image_size) ||
        overlaps(layout->image_pa, layout->image_size,
                 layout->stack_pa, layout->stack_size))
        return PWL_ERR_INVALID_ARGUMENT;
    if (!covered(regions, count, root, PWL_PAGE_SIZE, PWL_MEMORY_LOADER_DATA) ||
        !covered(regions, count, layout->image_pa, layout->image_size,
                 PWL_MEMORY_LOADER_CODE) ||
        !covered(regions, count, layout->stack_pa, layout->stack_size,
                 PWL_MEMORY_LOADER_DATA))
        return PWL_ERR_INVALID_ARGUMENT;
    return PWL_OK;
}

_Static_assert(sizeof(pwl_efi_memory_descriptor_t) == 40,
               "UEFI descriptor must be 40 bytes");

static uint32_t efi_type(pwl_memory_kind_t kind)
{
    switch (kind) {
        case PWL_MEMORY_FREE: return 7;         /* EfiConventionalMemory */
        case PWL_MEMORY_LOADER_CODE: return 1;  /* EfiLoaderCode */
        case PWL_MEMORY_LOADER_DATA: return 2;  /* EfiLoaderData */
        case PWL_MEMORY_MMIO: return 11;        /* EfiMemoryMappedIO */
        default: return 0;                      /* EfiReservedMemoryType */
    }
}

static int known_cacheability(uint64_t attributes)
{
    return attributes == 1 || attributes == 2 || attributes == 4 ||
           attributes == 8 || attributes == 16;
}

pwl_status_t pwl_efi_descriptors_from_regions(
    const pwl_phys_region_t *regions, const uint64_t *cacheability,
    size_t count, pwl_efi_memory_descriptor_t *out, size_t capacity,
    size_t *count_out)
{
    size_t i;
    if (count_out == NULL || cacheability == NULL ||
        pwl_memory_map_validate(regions, count) != PWL_OK ||
        count > SIZE_MAX / sizeof(*out))
        return PWL_ERR_INVALID_ARGUMENT;
    for (i = 0; i < count; ++i) {
        if (!known_cacheability(cacheability[i]))
            return PWL_ERR_INVALID_ARGUMENT;
    }
    *count_out = count;
    if (out == NULL || capacity < count)
        return PWL_ERR_BUFFER_TOO_SMALL;
    for (i = 0; i < count; ++i) {
        out[i].type = efi_type(regions[i].kind);
        out[i].padding = 0;
        out[i].physical_start = regions[i].base;
        out[i].virtual_start = 0;
        out[i].number_of_pages = regions[i].length / PWL_PAGE_SIZE;
        out[i].attribute = cacheability[i];
    }
    return PWL_OK;
}
