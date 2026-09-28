#include "pwl_handoff.h"
#include <assert.h>

int main(void)
{
    /* Real values photographed in the returning PS4 context callback. */
    pwl_x64_cpu_state_t cpu = {
        UINT64_C(0x8005003B), UINT64_C(0x0B28B000),
        UINT64_C(0x406F0), UINT64_C(0xD01)
    };
    assert(pwl_x64_cpu_state_validate(&cpu, 0x0B28B000) == PWL_OK);
    assert(pwl_x64_cpu_state_validate(&cpu, 0x0B28C000) != PWL_OK);
    cpu.cr4 |= UINT64_C(1) << 12; /* Five-level page tables. */
    assert(pwl_x64_cpu_state_validate(&cpu, 0x0B28B000) != PWL_OK);
    cpu.cr4 &= ~(UINT64_C(1) << 12);
    cpu.efer &= ~(UINT64_C(1) << 11); /* No execute-disable support. */
    assert(pwl_x64_cpu_state_validate(&cpu, 0x0B28B000) != PWL_OK);
    cpu.efer |= UINT64_C(1) << 11;
    cpu.cr3 |= 1; /* Unmodelled low CR3 bits. */
    assert(pwl_x64_cpu_state_validate(&cpu, 0x0B28B000) != PWL_OK);
    return 0;
}
