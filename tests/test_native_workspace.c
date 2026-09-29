#include "pwl_native_workspace.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    void *allocation;
    uint64_t bytes, physical, bad_page, bad_byte;
    unsigned allocations, frees, extracts, fail;
} kernel;

static pwl_ps4_vm_u64_t allocate(void *map, pwl_ps4_vm_u64_t size, int flags,
                          pwl_ps4_vm_u64_t low, pwl_ps4_vm_u64_t high, unsigned long alignment,
                          unsigned long boundary, char attribute)
{
    assert(map == &kernel && flags == 0x101 && low == 0x100000);
    assert(high == (UINT64_C(1) << 47) && alignment == 16384);
    assert(boundary == 0 && attribute == 6);
    ++kernel.allocations;
    if (kernel.fail) return 0;
    assert(kernel.allocation == NULL);
    kernel.allocation = aligned_alloc(16384, (size_t)size);
    assert(kernel.allocation);
    kernel.bytes = size;
    memset(kernel.allocation, 0, (size_t)size);
    return (uint64_t)(uintptr_t)kernel.allocation;
}

static void release(void *map, pwl_ps4_vm_u64_t kva, pwl_ps4_vm_u64_t bytes)
{
    assert(map == &kernel && kva == (uint64_t)(uintptr_t)kernel.allocation);
    assert(kva != kernel.physical && bytes == kernel.bytes);
    ++kernel.frees;
    free(kernel.allocation);
    kernel.allocation = NULL;
}

static pwl_ps4_vm_u64_t extract(void *pmap, pwl_ps4_vm_u64_t kva)
{
    uint64_t offset = kva - (uint64_t)(uintptr_t)kernel.allocation;
    assert(pmap == &kernel && offset < kernel.bytes);
    ++kernel.extracts;
    if (kernel.bad_page && offset / 4096 == kernel.bad_page / 4096) return 0;
    if (kernel.bad_byte && offset == kernel.bad_byte) return kernel.physical + offset + 4096;
    return kernel.physical + offset;
}

static pwl_ps4_memory_api_t api(void)
{
    return (pwl_ps4_memory_api_t){&kernel, &kernel, allocate, release, extract, 0};
}

static void test_owner(void)
{
    pwl_ps4_memory_api_t a = api();
    pwl_ps4_arena_t arena = {0};
    pwl_owned_span_t span;
    unsigned frees;
    kernel.physical = 0x4000000;
    assert(pwl_ps4_arena_acquire(&a, 16383, &arena) == PWL_ERR_INVALID_ARGUMENT);
    assert(kernel.allocations == 0);
    kernel.fail = 1;
    assert(pwl_ps4_arena_acquire(&a, 16384, &arena) == PWL_ERR_OUT_OF_RESOURCES);
    assert(kernel.allocations == 1 && kernel.frees == 0);
    kernel.fail = 0;
    kernel.bad_page = 4096; /* A bad 4 KiB subpage inside a 16 KiB VM page. */
    assert(pwl_ps4_arena_acquire(&a, 16384, &arena) == PWL_ERR_INVALID_ARGUMENT);
    assert(kernel.frees == 1 && arena.kernel_address == 0);
    kernel.bad_page = 0;
    kernel.bad_byte = 16383; /* Starts translate correctly, last byte does not. */
    assert(pwl_ps4_arena_acquire(&a, 16384, &arena) == PWL_ERR_INVALID_ARGUMENT);
    assert(kernel.allocation == NULL && arena.kernel_address == 0);
    kernel.bad_byte = 0;
    kernel.physical = 0xfc000; /* Allocator violates requested low PA bound. */
    assert(pwl_ps4_arena_acquire(&a, 16384, &arena) == PWL_ERR_INVALID_ARGUMENT);
    assert(kernel.allocation == NULL);
    kernel.physical = 0x4000001; /* Unaligned physical result. */
    assert(pwl_ps4_arena_acquire(&a, 16384, &arena) == PWL_ERR_INVALID_ARGUMENT);
    assert(kernel.allocation == NULL);
    kernel.physical = 0x4000000;
    assert(pwl_ps4_arena_acquire(&a, 32768, &arena) == PWL_OK);
    assert(pwl_ps4_arena_acquire(&a, 32768, &arena) == PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_ps4_arena_take(&arena, 4096, 4096, &span) == PWL_OK);
    assert(span.prepare_address == kernel.allocation && span.physical_address == kernel.physical);
    assert(pwl_ps4_arena_take(&arena, 4096, 16384, &span) == PWL_OK);
    assert(span.physical_address == kernel.physical + 16384);
    assert(span.prepare_address == (unsigned char *)kernel.allocation + 16384);
    assert(pwl_ps4_arena_take(&arena, 16384, 4096, &span) == PWL_ERR_OUT_OF_RESOURCES);
    frees = kernel.frees;
    arena.api.free = NULL;
    assert(pwl_ps4_arena_release(&arena) == PWL_ERR_INVALID_ARGUMENT);
    assert(arena.kernel_address == (uint64_t)(uintptr_t)kernel.allocation);
    assert(arena.size == 32768 && kernel.frees == frees);
    arena.api = a;
    arena.api.firmware = PWL_PS4_FIRMWARE_1352;
    assert(pwl_ps4_arena_release(&arena) == PWL_ERR_UNSUPPORTED);
    assert(kernel.frees == frees && arena.size == 32768);
    arena.api = a;
    ++arena.size;
    assert(pwl_ps4_arena_release(&arena) == PWL_ERR_INVALID_ARGUMENT);
    --arena.size;
    assert(pwl_ps4_arena_release(&arena) == PWL_OK);
    assert(pwl_ps4_arena_release(&arena) == PWL_OK);
    assert(kernel.frees == frees + 1 && arena.kernel_address == 0);
    kernel.physical = (UINT64_C(1) << 47) - 16384;
    assert(pwl_ps4_arena_acquire(&a, 32768, &arena) == PWL_ERR_INVALID_ARGUMENT);
    assert(kernel.allocation == NULL);
    assert(pwl_ps4_arena_acquire(&a, 16384, &arena) == PWL_OK);
    assert(arena.physical_address + arena.size == PWL_PS4_IDENTITY_LIMIT);
    assert(pwl_ps4_arena_release(&arena) == PWL_OK);
    assert(pwl_ps4_arena_release(NULL) == PWL_ERR_INVALID_ARGUMENT);
    a.free = NULL;
    frees = kernel.allocations;
    assert(pwl_ps4_arena_acquire(&a, 16384, &arena) == PWL_ERR_INVALID_ARGUMENT);
    assert(kernel.allocations == frees); /* Cannot acquire without a free path. */
}

static uint64_t *leaf(pwl_native_workspace_t *w, uint64_t address)
{
    static const unsigned shifts[] = {39, 30, 21, 12};
    size_t index = 0, j;
    unsigned level;
    for (level = 0; level < 4; ++level) {
        uint64_t *e = &w->tables[index].entries[(address >> shifts[level]) & 511];
        if (!(*e & 1)) return e;
        if (level == 3) return e;
        for (j = 0; j < w->table_count; ++j)
            if (w->tables[j].physical_address == (*e & UINT64_C(0x000ffffffffff000))) break;
        assert(j < w->table_count);
        index = j;
    }
    abort();
}

static void test_pipeline(void)
{
    pwl_native_workspace_t w = {0};
    pwl_ps4_memory_api_t a = api();
    unsigned char firmware[4097], disk[16384], output[512];
    pwl_native_request_t r = {firmware, sizeof(firmware), disk, sizeof(disk),
                              3 * 1024 * 1024, 16384, 16, 0x1234};
    pwl_native_data_t *d;
    pwl_efi_memory_descriptor_t map[PWL_FW_MAX_DESCRIPTORS];
    uint64_t key = 0, address = 0, saved;
    size_t size, ds, i;
    uint32_t version;
    unsigned allocations, frees, extracts;
    memset(firmware, 0xa5, sizeof(firmware));
    for (i = 0; i < sizeof(disk); ++i) disk[i] = (unsigned char)(i * 37);
    kernel.physical = 0x3ff8000; /* Cross a 2 MiB page-table boundary. */
    a.firmware = PWL_PS4_FIRMWARE_1352;
    allocations = kernel.allocations;
    assert(pwl_native_workspace_prepare(&a, &r, &w) == PWL_ERR_UNSUPPORTED);
    assert(w.arena.kernel_address == 0 && kernel.allocations == allocations);
    a = api();
    kernel.fail = 1;
    assert(pwl_native_workspace_prepare(&a, &r, &w) == PWL_ERR_OUT_OF_RESOURCES);
    assert(w.arena.kernel_address == 0 && kernel.allocations == allocations + 1);
    kernel.fail = 0;
    assert(pwl_native_workspace_prepare(&a, &r, &w) == PWL_OK);
    assert(w.table_count > 4);
    assert(w.firmware.prepare_address != (void *)(uintptr_t)w.firmware.physical_address);
    assert(memcmp(w.firmware.prepare_address, firmware, sizeof(firmware)) == 0);
    assert(memcmp(w.media.prepare_address, disk, sizeof(disk)) == 0);
    assert(((unsigned char *)w.firmware.prepare_address)[sizeof(firmware)] == 0);
    assert(*leaf(&w, w.stack.physical_address - 4096) == 0);
    assert(*leaf(&w, w.stack.physical_address + w.stack.size) == 0);
    assert((*leaf(&w, w.firmware.physical_address) & 2) == 0); /* RX, not RWX. */
    assert((*leaf(&w, w.media.physical_address) & (UINT64_C(1) << 63)) != 0);
    saved = *leaf(&w, w.media.physical_address);
    *leaf(&w, w.media.physical_address) |= 2;
    assert(pwl_x64_identity_mappings_validate(w.mappings, 6, w.tables, w.table_count) != PWL_OK);
    *leaf(&w, w.media.physical_address) = saved;
    *leaf(&w, w.stack.physical_address - 4096) = (w.stack.physical_address - 4096) | 3;
    assert(pwl_x64_identity_mappings_validate(w.mappings, 6, w.tables, w.table_count) != PWL_OK);
    *leaf(&w, w.stack.physical_address - 4096) = 0;
    assert(pwl_x64_identity_mappings_validate(w.mappings, 6, w.tables, w.table_count) == PWL_OK);
    w.tables[0].entries[1] = w.tables[0].entries[0]; /* Reused subtree/alias. */
    assert(pwl_x64_identity_mappings_validate(w.mappings, 6, w.tables, w.table_count) != PWL_OK);
    w.tables[0].entries[1] = 0;
    saved = *leaf(&w, w.media.physical_address);
    *leaf(&w, w.media.physical_address) &= ~(UINT64_C(1) << 63);
    assert(pwl_x64_identity_mappings_validate(w.mappings, 6, w.tables, w.table_count) != PWL_OK);
    *leaf(&w, w.media.physical_address) = saved;

    d = w.data.prepare_address;
    allocations = kernel.allocations; frees = kernel.frees; extracts = kernel.extracts;
    memset(disk, 0, sizeof(disk)); /* The source is no longer needed. */
    assert(pwl_fw_media_read(&d->media, w.media.prepare_address, 1, 2,
                              sizeof(output), output) == PWL_EFI_SUCCESS);
    for (i = 0; i < sizeof(output); ++i) assert(output[i] == (unsigned char)((1024 + i) * 37));
    assert(pwl_fw_allocate_pages(&d->memory, PWL_ALLOCATE_ANY, 2, 3, &address) == 0);
    assert(address >= w.heap.physical_address && address + 3 * 4096 <= w.heap.physical_address + w.heap.size);
    assert(pwl_fw_free_pages(&d->memory, w.data.physical_address, 1) == PWL_EFI_NOT_FOUND);
    size = sizeof(map);
    assert(pwl_fw_get_memory_map(&d->memory, &size, map, &key, &ds, &version) == 0);
    assert(pwl_fw_free_pages(&d->memory, address, 3) == 0);
    assert(pwl_fw_memory_exit(&d->memory, r.image_handle, key) == PWL_EFI_INVALID_PARAMETER);
    size = sizeof(map);
    assert(pwl_fw_get_memory_map(&d->memory, &size, map, &key, &ds, &version) == 0);
    assert(pwl_fw_memory_exit(&d->memory, r.image_handle, key) == 0);
    assert(pwl_fw_allocate_pages(&d->memory, PWL_ALLOCATE_ANY, 2, 1, &address) == PWL_EFI_ACCESS_DENIED);
    assert(kernel.allocations == allocations && kernel.frees == frees && kernel.extracts == extracts);
    /* No CPU switch occurred; this is still a preparation-side abort/cleanup. */
    w.arena.api.free = NULL;
    assert(pwl_native_workspace_release(&w) == PWL_ERR_INVALID_ARGUMENT);
    assert(w.table_count > 4 && w.data.prepare_address == d);
    assert(kernel.frees == frees && kernel.allocation != NULL);
    w.arena.api = a;
    assert(pwl_native_workspace_release(&w) == PWL_OK);
    assert(kernel.frees == frees + 1 && kernel.allocation == NULL && w.table_count == 0);

    r.table_pages = 4; /* Cannot cover this crossing + 3 MiB pool. */
    assert(pwl_native_workspace_prepare(&a, &r, &w) == PWL_ERR_BUFFER_TOO_SMALL);
    assert(kernel.allocation == NULL && w.table_count == 0);
    r.heap_bytes = UINT64_MAX;
    allocations = kernel.allocations;
    assert(pwl_native_workspace_prepare(&a, &r, &w) == PWL_ERR_INVALID_ARGUMENT);
    assert(kernel.allocations == allocations);
}

int main(void)
{
    test_owner();
    test_pipeline();
    puts("native workspace: owned PA/KVA, resident services, guarded mappings and rollback passed");
    return 0;
}
