#include "pwl_firmware.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    pwl_fw_memory_t memory={0};
    pwl_phys_region_t region={UINT64_C(0x27a400000),256*PWL_PAGE_SIZE,PWL_MEMORY_FREE};
    uint64_t cache=8, addresses[PWL_FW_MAX_POOLS], key, ds;
    size_t size;
    uint32_t version;
    pwl_efi_memory_descriptor_t map[PWL_FW_MAX_DESCRIPTORS];
    assert(pwl_fw_memory_init(&memory,&region,&cache,1,0x1234)==PWL_OK);
    for (size_t i=0;i<PWL_FW_MAX_POOLS;i++) {
        assert(pwl_fw_allocate_pool(&memory,2,1,&addresses[i])==0);
        assert(addresses[i]==region.base+i*PWL_PAGE_SIZE);
    }
    uint64_t unchanged=0x123, old_key=memory.key;
    assert(pwl_fw_allocate_pool(&memory,2,1,&unchanged)==PWL_EFI_OUT_OF_RESOURCES);
    assert(unchanged==0x123 && old_key==memory.key);
    assert(pwl_fw_free_pages(&memory,region.base,128)==PWL_EFI_INVALID_PARAMETER);
    for (size_t i=0;i<PWL_FW_MAX_POOLS;i+=2)
        assert(pwl_fw_free_pool(&memory,addresses[i])==0);
    for (size_t i=1;i<PWL_FW_MAX_POOLS;i+=2)
        assert(pwl_fw_free_pool(&memory,addresses[i])==0);
    assert(memory.count==1 && memory.entries[0].descriptor.type==7);
    assert(pwl_fw_allocate_pool(&memory,2,SIZE_MAX,&unchanged)==PWL_EFI_OUT_OF_RESOURCES);
    assert(unchanged==0x123);
    assert(pwl_fw_allocate_pool(&memory,2,8193,&addresses[0])==0);
    size=sizeof(map);size_t descriptor_size;
    assert(pwl_fw_get_memory_map(&memory,&size,map,&key,&descriptor_size,&version)==0);
    ds=memory.key;
    assert(pwl_fw_allocate_pool(&memory,2,8,&addresses[1])==0 && memory.key!=ds);
    assert(pwl_fw_memory_exit(&memory,0x1234,key)==PWL_EFI_INVALID_PARAMETER);
    size=sizeof(map);
    assert(pwl_fw_get_memory_map(&memory,&size,map,&key,&descriptor_size,&version)==0);
    assert(pwl_fw_memory_exit(&memory,0x1234,key)==0);
    assert(pwl_fw_free_pool(&memory,addresses[0])==PWL_EFI_ACCESS_DENIED);
    assert(pwl_fw_allocate_pool(&memory,2,1,&unchanged)==PWL_EFI_ACCESS_DENIED);
    puts("EFI pool: ownership, exhaustion, coalescing, overflow and map retirement passed");
    return 0;
}
