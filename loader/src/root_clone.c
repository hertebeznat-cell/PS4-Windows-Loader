#include "pwl_root_clone.h"
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
