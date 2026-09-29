#include "pwl_native_workspace.h"

static int rounded(uint64_t n, uint64_t alignment, uint64_t *result)
{
    if (n == 0 || n > UINT64_MAX - (alignment - 1)) return 0;
    *result = (n + alignment - 1) & ~(alignment - 1);
    return 1;
}

static void copy_bytes(void *to, const void *from, size_t count)
{
    unsigned char *d = to;
    const unsigned char *s = from;
    size_t i;
    for (i = 0; i < count; ++i) d[i] = s[i];
}

pwl_status_t pwl_native_workspace_release(pwl_native_workspace_t *w)
{
    pwl_status_t status;
    if (w == NULL) return PWL_ERR_INVALID_ARGUMENT;
    status = pwl_ps4_arena_release(&w->arena);
    if (status != PWL_OK) return status;
    *w = (pwl_native_workspace_t){0};
    return PWL_OK;
}

pwl_status_t pwl_native_workspace_prepare(const pwl_ps4_memory_api_t *api,
                                          const pwl_native_request_t *r,
                                          pwl_native_workspace_t *w)
{
    uint64_t sizes[6], total = 2 * PWL_PAGE_SIZE, allocation;
    pwl_owned_span_t *spans[6], guards[2];
    pwl_native_data_t *data;
    uint64_t cacheability[PWL_NATIVE_REGION_COUNT];
    size_t i, region = 0;
    pwl_status_t status;
    if (w == NULL || r == NULL || w->arena.kernel_address != 0 ||
        w->arena.physical_address != 0 || w->arena.size != 0 || w->arena.used != 0 ||
        r->firmware == NULL || r->disk_image == NULL || r->image_handle == 0 ||
        r->table_pages < 4 || r->table_pages > PWL_NATIVE_MAX_TABLES ||
        r->disk_bytes % 512 ||
        !rounded(r->firmware_bytes, PWL_PAGE_SIZE, &sizes[0]) ||
        !rounded(sizeof(pwl_native_data_t), PWL_PAGE_SIZE, &sizes[1]) ||
        !rounded(r->stack_bytes, PWL_PAGE_SIZE, &sizes[3]) ||
        !rounded(r->disk_bytes, PWL_PAGE_SIZE, &sizes[4]) ||
        !rounded(r->heap_bytes, PWL_PAGE_SIZE, &sizes[5]))
        return PWL_ERR_INVALID_ARGUMENT;
    sizes[2] = r->table_pages * PWL_PAGE_SIZE;
    for (i = 0; i < 6; ++i) {
        if (sizes[i] > UINT64_MAX - total) return PWL_ERR_INVALID_ARGUMENT;
        total += sizes[i];
    }
    if (!rounded(total, PWL_PS4_VM_PAGE_SIZE, &allocation) || allocation > SIZE_MAX)
        return PWL_ERR_INVALID_ARGUMENT;
    *w = (pwl_native_workspace_t){0};
    status = pwl_ps4_arena_acquire(api, allocation, &w->arena);
    if (status != PWL_OK) return status;
    spans[0] = &w->firmware; spans[1] = &w->data; spans[2] = &w->tables_span;
    spans[3] = &w->stack; spans[4] = &w->media; spans[5] = &w->heap;
    for (i = 0; i < 6; ++i) {
        if (i == 3) {
            status = pwl_ps4_arena_take(&w->arena, PWL_PAGE_SIZE,
                                       PWL_PAGE_SIZE, &guards[0]);
            if (status != PWL_OK) goto failure;
            w->regions[region++] = (pwl_phys_region_t){ guards[0].physical_address,
                                                guards[0].size, PWL_MEMORY_RESERVED };
        }
        status = pwl_ps4_arena_take(&w->arena, sizes[i], PWL_PAGE_SIZE, spans[i]);
        if (status != PWL_OK) goto failure;
        w->regions[region++] = (pwl_phys_region_t){ spans[i]->physical_address,
            spans[i]->size, i == 0 ? PWL_MEMORY_LOADER_CODE :
                           i == 5 ? PWL_MEMORY_FREE : PWL_MEMORY_LOADER_DATA };
        w->mappings[i] = (pwl_x64_identity_range_t){spans[i]->physical_address,
                            spans[i]->size, i != 0 && i != 4, i == 0};
        if (i == 3) {
            status = pwl_ps4_arena_take(&w->arena, PWL_PAGE_SIZE,
                                       PWL_PAGE_SIZE, &guards[1]);
            if (status != PWL_OK) goto failure;
            w->regions[region++] = (pwl_phys_region_t){ guards[1].physical_address,
                                                guards[1].size, PWL_MEMORY_RESERVED };
        }
    }
    /* 16 KiB tail padding stays owned but outside the advertised pool/map. */
    copy_bytes(w->firmware.prepare_address, r->firmware, r->firmware_bytes);
    copy_bytes(w->media.prepare_address, r->disk_image, r->disk_bytes);
    data = w->data.prepare_address;
    for (i = 0; i < PWL_NATIVE_REGION_COUNT; ++i) cacheability[i] = 8; /* WB allocation */
    status = pwl_fw_memory_init(&data->memory, w->regions, cacheability,
                                PWL_NATIVE_REGION_COUNT, r->image_handle);
    if (status != PWL_OK) goto failure;
    /* Firmware lifetime is distinct from the later Microsoft loader image. */
    data->memory.entries[0].descriptor.type = 3; /* EfiBootServicesCode */
    data->memory.entries[1].descriptor.type = 4; /* EfiBootServicesData */
    status = pwl_fw_media_init(&data->media, w->media.physical_address,
                                r->disk_bytes, 512, 1);
    if (status != PWL_OK) goto failure;
    for (i = 0; i < r->table_pages; ++i) {
        w->tables[i].physical_address = w->tables_span.physical_address + i * PWL_PAGE_SIZE;
        w->tables[i].entries = (uint64_t *)((unsigned char *)w->tables_span.prepare_address +
                                                                    i * PWL_PAGE_SIZE);
    }
    status = pwl_x64_identity_tables_build(w->mappings, 6, w->tables,
                                            r->table_pages, &w->table_count);
    if (status == PWL_OK) return PWL_OK;
failure:
    {
        pwl_status_t cleanup = pwl_native_workspace_release(w);
        if (cleanup != PWL_OK) return cleanup;
    }
    return status;
}
