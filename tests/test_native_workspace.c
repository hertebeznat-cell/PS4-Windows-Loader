#include "pwl_native_workspace.h"
#include "pe_fixture.h"
#include "resident_fixture.h"
static pwl_resident_image_t resident_image;

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
                              3 * 1024 * 1024, 16384, 16, 0x1234, NULL, 0};
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
    assert(pwl_x64_identity_mappings_validate(w.mappings, w.mapping_count, w.tables, w.table_count) != PWL_OK);
    *leaf(&w, w.media.physical_address) = saved;
    *leaf(&w, w.stack.physical_address - 4096) = (w.stack.physical_address - 4096) | 3;
    assert(pwl_x64_identity_mappings_validate(w.mappings, w.mapping_count, w.tables, w.table_count) != PWL_OK);
    *leaf(&w, w.stack.physical_address - 4096) = 0;
    assert(pwl_x64_identity_mappings_validate(w.mappings, w.mapping_count, w.tables, w.table_count) == PWL_OK);
    w.tables[0].entries[1] = w.tables[0].entries[0]; /* Reused subtree/alias. */
    assert(pwl_x64_identity_mappings_validate(w.mappings, w.mapping_count, w.tables, w.table_count) != PWL_OK);
    w.tables[0].entries[1] = 0;
    saved = *leaf(&w, w.media.physical_address);
    *leaf(&w, w.media.physical_address) &= ~(UINT64_C(1) << 63);
    assert(pwl_x64_identity_mappings_validate(w.mappings, w.mapping_count, w.tables, w.table_count) != PWL_OK);
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

static void test_boot_image(void)
{
    pwl_native_workspace_t w = {0};
    pwl_ps4_memory_api_t a = api();
    unsigned char firmware[4096] = {0}, disk[512] = {0}, image[PE_FIXTURE_BYTES];
    pwl_native_request_t r = {firmware, sizeof(firmware), disk, sizeof(disk),
        65536, 16384, 16, 0x1234, image, sizeof(image)};
    unsigned frees = kernel.frees, allocations;
    kernel.physical = 0x4000000;
    pe_fixture(image);
    assert(pwl_native_workspace_prepare(&a, &r, &w) == PWL_OK);
    assert(w.region_count == 9 && w.mapping_count == 10);
    assert(w.boot_image.entry_address == w.boot.physical_address + 0x1000);
    assert(pe_get64((unsigned char *)w.boot.prepare_address + 0x2000) ==
        w.boot.physical_address + 0x1010);
    assert((*leaf(&w, w.boot_image.entry_address) & (UINT64_C(1) << 63 | 2)) == 0);
    assert((*leaf(&w, w.boot.physical_address + 0x2000) & (UINT64_C(1) << 63 | 2)) ==
        (UINT64_C(1) << 63 | 2));
    assert((*leaf(&w, w.boot.physical_address) & (UINT64_C(1) << 63)) != 0);
    assert(pwl_native_workspace_release(&w) == PWL_OK);
    assert(kernel.frees == frees + 1 && w.boot_image.entry_address == 0);
    pe16(image + 0x608, 0x3000); /* Relocation fails after kernel allocation. */
    assert(pwl_native_workspace_prepare(&a, &r, &w) == PWL_ERR_UNSUPPORTED);
    assert(kernel.frees == frees + 2 && kernel.allocation == NULL);
    assert(w.boot.prepare_address == NULL && w.mapping_count == 0);
    pe_fixture(image); pe16(image + PE_OPT + 68, 3);
    allocations = kernel.allocations;
    assert(pwl_native_workspace_prepare(&a, &r, &w) == PWL_ERR_UNSUPPORTED);
    assert(kernel.allocations == allocations); /* Invalid layout: no allocation. */
}

static void test_resident(void)
{
    pwl_native_workspace_t w={0};
    pwl_ps4_memory_api_t a=api();
    unsigned char disk[512]={0};
    pwl_native_request_t r={NULL,0,disk,sizeof(disk),65536,65536,16,0x1234,NULL,0};
    kernel.physical=UINT64_C(0x27a300000);
    assert(pwl_native_workspace_prepare_resident(&a,&r,&resident_image,&w)==PWL_OK);
    pwl_native_data_t *d=w.data.prepare_address;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)==PWL_OK);
    unsigned char *mutated=w.firmware.prepare_address;
    mutated[0]^=1;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    mutated[0]^=1;
    w.mappings[0].writable=1;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    w.mappings[0].writable=0;
    w.stack.prepare_address=(unsigned char *)w.stack.prepare_address+8;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    w.stack.prepare_address=(unsigned char *)w.stack.prepare_address-8;
    uint64_t *guard=leaf(&w,w.stack.physical_address-PWL_PAGE_SIZE);
    assert(guard && !*guard);
    *guard=(w.stack.physical_address-PWL_PAGE_SIZE)|3;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    *guard=0;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)==PWL_OK);
    assert(d->tpl==4);
    assert(d->efi.system.boot_services==w.data.physical_address+
        offsetof(pwl_native_data_t,efi)+offsetof(pwl_efi_prepared_tables_t,boot));
    assert(d->efi.boot.functions[26]==w.firmware.physical_address+resident_image.callbacks[5]);
    assert(pe_get64((unsigned char *)w.firmware.prepare_address+resident_image.binding_offset)==
        w.data.physical_address);
    const unsigned char *actual=w.firmware.prepare_address;
    for(size_t i=0;i<resident_image.size;i++)
        if(i<resident_image.binding_offset || i>=resident_image.binding_offset+8)
            assert(actual[i]==resident_bytes[i]);
    assert(pwl_x64_identity_mappings_validate(w.mappings,w.mapping_count,w.tables,w.table_count)==PWL_OK);
    assert((*leaf(&w,w.firmware.physical_address)&(UINT64_C(1)<<63|2))==0);
    assert((*leaf(&w,w.data.physical_address)&(UINT64_C(1)<<63|2))==(UINT64_C(1)<<63|2));
    assert(pwl_native_workspace_release(&w)==PWL_OK && !kernel.allocation);
    unsigned allocations=kernel.allocations;
    pwl_resident_image_t bad=resident_image;
    bad.crc32^=1;
    assert(pwl_native_workspace_prepare_resident(&a,&r,&bad,&w)!=PWL_OK);
    bad=resident_image;bad.binding_offset=UINT64_MAX;
    assert(pwl_resident_image_validate(&bad)!=PWL_OK);
    bad=resident_image;bad.callbacks[0]=bad.size;
    assert(pwl_resident_image_validate(&bad)!=PWL_OK);
    bad=resident_image;bad.callbacks[0]=bad.binding_offset;
    assert(pwl_resident_image_validate(&bad)!=PWL_OK);
    bad=resident_image;bad.callbacks[0]=bad.callbacks[1];
    assert(pwl_resident_image_validate(&bad)!=PWL_OK);
    assert(kernel.allocations==allocations);
}

static void test_resident_boot_image(void)
{
    pwl_native_workspace_t w={0};
    pwl_ps4_memory_api_t a=api();
    unsigned char disk[512]={0}, image[PE_FIXTURE_BYTES];
    pwl_native_request_t r={NULL,0,disk,sizeof(disk),65536,65536,16,0x1234,
                            image,sizeof(image)};
    kernel.physical=UINT64_C(0x27a300000);
    pe_fixture(image);
    unsigned frees=kernel.frees;
    assert(pwl_native_workspace_prepare_resident(&a,&r,&resident_image,&w)==PWL_OK);
    assert(w.region_count==9 && w.mapping_count==6+w.boot_image.range_count);
    pwl_native_data_t *data=w.data.prepare_address;
    assert(data->loaded_image.image_base==w.boot.physical_address);
    assert(data->loaded_image.image_size==w.boot.size && data->protocols[0].handle==r.image_handle);
    assert(data->protocols[0].interface_address==w.data.physical_address+
           offsetof(pwl_native_data_t,loaded_image));
    data->loaded_image.image_base++;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    data->loaded_image.image_base--;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)==PWL_OK);
    assert(pe_get64((unsigned char *)w.boot.prepare_address+0x2000)==
           w.boot.physical_address+0x1010);
    uint64_t entry=w.boot_image.entry_address;
    w.boot_image.entry_address=w.boot.physical_address; /* Header is NX. */
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    w.boot_image.entry_address=entry;
    w.boot_image.ranges[0].size+=PWL_PAGE_SIZE;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    w.boot_image.ranges[0].size-=PWL_PAGE_SIZE;
    w.mappings[6].writable=1;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    w.mappings[6].writable=0;
    uint64_t saved=*leaf(&w,entry);
    *leaf(&w,entry)|=2; /* Executable application page must not be writable. */
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    *leaf(&w,entry)=saved;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)==PWL_OK);
    assert(pwl_native_workspace_release(&w)==PWL_OK && kernel.frees==frees+1);
    pe16(image+0x608,0x3000);
    assert(pwl_native_workspace_prepare_resident(&a,&r,&resident_image,&w)==PWL_ERR_UNSUPPORTED);
    assert(!kernel.allocation && kernel.frees==frees+2);
}

static void test_resident_files(void)
{
    unsigned char archive[4096]={0};
    memcpy(archive,"PWLFILES",8);
    pe32(archive+8,1);pe32(archive+12,1);pe64(archive+16,sizeof(archive));
    archive[24]='\\';pe64(archive+24+512,560);pe32(archive+24+528,1);
    pwl_native_workspace_t w={0};pwl_ps4_memory_api_t a=api();
    pwl_native_request_t r={NULL,0,archive,sizeof(archive),65536,65536,16,0x1234,NULL,0};
    kernel.physical=UINT64_C(0x27a300000);
    assert(pwl_native_workspace_prepare_resident(&a,&r,&resident_image,&w)==PWL_OK);
    pwl_native_data_t *data=w.data.prepare_address;
    assert(data->files_enabled==1 && data->filesystem[0]==0x10000);
    assert(data->filesystem[1]==w.firmware.physical_address+resident_image.callbacks[17]);
    assert(data->protocols[1].interface_address==w.data.physical_address+
           offsetof(pwl_native_data_t,filesystem));
    assert(pwl_native_resident_environment_validate(&w,&resident_image)==PWL_OK);
    data->files_enabled=0;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    data->files_enabled=1;data->media.bytes=UINT64_MAX;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    data->media.bytes=sizeof(archive);data->file_template.functions[0]++;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    assert(pwl_native_workspace_release(&w)==PWL_OK);
    pe32(archive+12,UINT32_MAX);
    assert(pwl_native_workspace_prepare_resident(&a,&r,&resident_image,&w)==PWL_ERR_BAD_IMAGE);
    assert(!kernel.allocation && !w.arena.kernel_address);
}

static void test_boot_archive(void)
{
    unsigned char archive[4096]={0};memcpy(archive,"PWLFILES",8);
    pe32(archive+8,1);pe32(archive+12,3);pe64(archive+16,sizeof(archive));
    const char *names[]={"\\","\\Boot","\\Boot\\bootmgfw.efi"};
    for (size_t i=0;i<3;i++) {
        unsigned char *record=archive+24+i*536;
        for (size_t j=0;names[i][j];j++) record[j*2]=(unsigned char)names[i][j];
        pe64(record+512,2048);pe64(record+520,i==2 ? PE_FIXTURE_BYTES : 0);
        pe32(record+528,i==2 ? 0 : 1);
    }
    pe_fixture(archive+2048);
    const uint16_t path[]={'\\','b','o','o','t','\\','b','o','o','t','m','g','f','w','.','e','f','i',0};
    pwl_ps4_memory_api_t a=api();pwl_native_workspace_t w={0};
    pwl_native_request_t r={NULL,0,archive,sizeof(archive),65536,65536,16,1,NULL,0};
    kernel.physical=UINT64_C(0x27a300000);
    unsigned allocations=kernel.allocations;
    const uint16_t missing[]={'\\','N','O',0};
    assert(pwl_native_boot_prepare(&a,&r,&resident_image,missing,&w)==PWL_ERR_NOT_FOUND);
    assert(kernel.allocations==allocations);
    assert(pwl_native_boot_prepare(&a,&r,&resident_image,path,&w)==PWL_OK);
    pwl_native_data_t *data=w.data.prepare_address;
    assert(data->boot_origin_bound==1 && data->boot_file_record==2);
    assert(data->loaded_image.device_handle==2 && data->protocols[0].handle==1);
    assert(data->loaded_image.file_path==w.data.physical_address+offsetof(pwl_native_data_t,boot_file_path));
    assert(data->boot_file_path[0]==4 && data->boot_file_path[1]==4);
    assert(data->boot_file_path[4]=='\\' && data->boot_file_path[6]=='B');
    assert(pe_get64((unsigned char *)w.boot.prepare_address+0x2000)==w.boot.physical_address+0x1010);
    /* The source buffer is no longer needed; origin references the owned copy. */
    memset(archive,0,sizeof(archive));
    assert(pwl_native_resident_environment_validate(&w,&resident_image)==PWL_OK);
    data->boot_file_path[0]^=1;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    data->boot_file_path[0]^=1;
    data->loaded_image.file_path++;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    data->loaded_image.file_path--;
    data->boot_file_record=0;
    assert(pwl_native_resident_environment_validate(&w,&resident_image)!=PWL_OK);
    data->boot_file_record=2;
    assert(pwl_native_workspace_release(&w)==PWL_OK && !kernel.allocation);
}

int main(void)
{
    resident_image=resident_fixture();
    test_owner();
    test_pipeline();
    test_boot_image();
    test_resident();
    test_resident_boot_image();
    test_resident_files();
    test_boot_archive();
    puts("native workspace: owned PA/KVA, resident services, guarded mappings and rollback passed");
    return 0;
}
