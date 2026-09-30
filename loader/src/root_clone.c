#include "pwl_root_clone.h"
pwl_status_t pwl_x64_root_direct_address(uint32_t pml4_index,uint32_t pdpt_index,
    uint64_t physical,uint64_t *address)
{
    if (address) *address=0;
    if (!address || pml4_index<256 || pml4_index>511 || pdpt_index>480 ||
        !physical || physical%4096 || physical>=(UINT64_C(32)<<30))
        return PWL_ERR_INVALID_ARGUMENT;
    *address=(UINT64_C(0xffff800000000000) |
        ((uint64_t)pml4_index<<39) | ((uint64_t)pdpt_index<<30))+physical;
    return PWL_OK;
}
pwl_status_t pwl_x64_root_clone_prepare(const pwl_x64_cpu_state_t *cpu,
    const volatile uint64_t *source,uint64_t *destination)
{
    uintptr_t s=(uintptr_t)source,d=(uintptr_t)destination;
    if (!cpu || !s || !d || s%8 || d%8 || s>UINTPTR_MAX-4096 ||
        d>UINTPTR_MAX-4096 || (s<d+4096 && d<s+4096) ||
        pwl_x64_cpu_state_validate(cpu,cpu->cr3)!=PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    for (size_t i=0;i<512;i++) destination[i]=source[i];
    for (size_t i=0;i<512;i++)
        if (destination[i]!=source[i]) return PWL_ERR_INVALID_ARGUMENT;
    return PWL_OK;
}
