#include "pwl_firmware.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_memory(void)
{
    const pwl_phys_region_t regions[] = {
        {0x100000, 0x4000, PWL_MEMORY_LOADER_DATA},
        {0x200000, 0x10000, PWL_MEMORY_FREE},
        {0x300000, 0x2000, PWL_MEMORY_MMIO},
        {0x400000, 0x10000, PWL_MEMORY_FREE}
    };
    const uint64_t cache[] = {8, 8, 1, 8};
    pwl_fw_memory_t m = {0}, before;
    pwl_efi_memory_descriptor_t map[PWL_FW_MAX_DESCRIPTORS];
    uint64_t address, key = 99, stale;
    size_t size, ds;
    uint32_t version;
    assert(pwl_fw_memory_init(&m, regions, cache, 4, 42) == PWL_OK);
    size = 0;
    assert(pwl_fw_get_memory_map(&m, &size, NULL, &key, &ds, &version) == PWL_EFI_BUFFER_TOO_SMALL);
    assert(key == 99 && m.issued_key == 0 && size == 4 * sizeof(map[0]));
    assert(pwl_fw_memory_exit(&m, 42, m.key) == PWL_EFI_INVALID_PARAMETER);
    address = 0x204000;
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_ADDRESS, 2, 2, &address) == 0);
    assert(m.count == 6 && address == 0x204000);
    before = m;
    address = 0x300000;
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_ADDRESS, 2, 1, &address) == PWL_EFI_NOT_FOUND);
    address = 0x180000; /* Unknown gap. */
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_ADDRESS, 2, 1, &address) == PWL_EFI_NOT_FOUND);
    assert(memcmp(&m, &before, sizeof(m)) == 0);
    assert(pwl_fw_free_pages(&m, 0x100000, 1) == PWL_EFI_NOT_FOUND);
    assert(pwl_fw_free_pages(&m, 0x203000, 3) == PWL_EFI_NOT_FOUND);
    assert(pwl_fw_free_pages(&m, 0x204000, 1) == 0);
    assert(pwl_fw_free_pages(&m, 0x205000, 1) == 0);
    assert(m.count == 4);
    assert(pwl_fw_free_pages(&m, 0x204000, 1) == PWL_EFI_NOT_FOUND);
    address = 0x4087ff;
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_MAX, 2, 2, &address) == 0);
    assert(address == 0x406000); /* Inclusive max applies to the entire allocation. */
    size = sizeof(map);
    assert(pwl_fw_get_memory_map(&m, &size, map, &key, &ds, &version) == 0);
    stale = key;
    address = UINT64_MAX;
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_MAX, 4, 1, &address) == 0);
    assert(address == 0x40f000);
    assert(pwl_fw_memory_exit(&m, 42, stale) == PWL_EFI_INVALID_PARAMETER);
    size = sizeof(map);
    assert(pwl_fw_get_memory_map(&m, &size, map, &key, &ds, &version) == 0);
    assert(ds == 40 && version == 1);
    assert(pwl_fw_memory_exit(&m, 43, key) == PWL_EFI_INVALID_PARAMETER);
    assert(pwl_fw_memory_exit(&m, 42, key) == 0);
    assert(pwl_fw_free_pages(&m, address, 1) == PWL_EFI_ACCESS_DENIED);
    assert(pwl_fw_get_memory_map(&m, &size, map, &key, &ds, &version) == PWL_EFI_ACCESS_DENIED);
    assert(pwl_fw_memory_exit(&m, 42, key) == PWL_EFI_ACCESS_DENIED);
}

static void test_capacity_and_overflow(void)
{
    pwl_phys_region_t regions[PWL_FW_MAX_DESCRIPTORS];
    uint64_t cache[PWL_FW_MAX_DESCRIPTORS], address;
    pwl_fw_memory_t m = {0}, before;
    size_t i;
    for (i = 0; i < PWL_FW_MAX_DESCRIPTORS; ++i) {
        regions[i] = (pwl_phys_region_t){0x10000 + i * 0x10000, 0x8000, PWL_MEMORY_FREE};
        cache[i] = 8;
    }
    assert(pwl_fw_memory_init(&m, regions, cache, PWL_FW_MAX_DESCRIPTORS, 1) == 0);
    before = m;
    address = 0x12000;
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_ADDRESS, 2, 1, &address) == PWL_EFI_OUT_OF_RESOURCES);
    assert(memcmp(&m, &before, sizeof(m)) == 0);
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_ANY, 2, UINT64_MAX, &address) == PWL_EFI_INVALID_PARAMETER);
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_ANY, 6, 1, &address) == PWL_EFI_UNSUPPORTED);
    address = UINT64_MAX - 4095;
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_ADDRESS, 2, 1, &address) == PWL_EFI_INVALID_PARAMETER);
    assert(pwl_fw_free_pages(&m, address, 2) == PWL_EFI_INVALID_PARAMETER);
    /* Whole descriptor allocation fits even when descriptor storage is full. */
    address = 0x10000;
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_ADDRESS, 2, 8, &address) == 0);
    assert(pwl_fw_free_pages(&m, address + 4096, 1) == PWL_EFI_OUT_OF_RESOURCES);
    assert(pwl_fw_free_pages(&m, address, 8) == 0);
    m.key = UINT64_MAX;
    before = m;
    assert(pwl_fw_allocate_pages(&m, PWL_ALLOCATE_ADDRESS, 2, 8, &address) == PWL_EFI_OUT_OF_RESOURCES);
    assert(memcmp(&m, &before, sizeof(m)) == 0);
}

static void test_media(void)
{
    pwl_fw_media_t media;
    unsigned char disk[4096], buffer[512];
    memset(disk, 0x5a, sizeof(disk));
    assert(pwl_fw_media_init(&media, 0x100000, sizeof(disk), 512, 7) == PWL_OK);
    assert(pwl_fw_media_read(&media, disk, 7, 7, sizeof(buffer), buffer) == 0);
    assert(buffer[0] == 0x5a && buffer[511] == 0x5a);
    assert(pwl_fw_media_read(&media, disk, 7, 8, sizeof(buffer), buffer) == PWL_EFI_INVALID_PARAMETER);
    assert(pwl_fw_media_read(&media, disk, 7, UINT64_MAX, sizeof(buffer), buffer) == PWL_EFI_INVALID_PARAMETER);
    assert(pwl_fw_media_read(&media, disk, 7, 0, 511, buffer) == PWL_EFI_BAD_BUFFER_SIZE);
    assert(pwl_fw_media_read(&media, disk, 8, 0, sizeof(buffer), buffer) == PWL_EFI_MEDIA_CHANGED);
    assert(pwl_fw_media_read(&media, disk, 7, 0, 0, NULL) == 0);
    assert(pwl_fw_media_write(&media, 7) == PWL_EFI_WRITE_PROTECTED);
    assert(pwl_fw_media_write(&media, 8) == PWL_EFI_MEDIA_CHANGED);
    assert(pwl_fw_media_init(&media, UINT64_MAX - 4095, 4096, 512, 1) != PWL_OK);
}

int main(void)
{
    test_memory();
    test_capacity_and_overflow();
    test_media();
    puts("resident firmware: allocation transactions, map key lifetime and read-only media passed");
    return 0;
}
