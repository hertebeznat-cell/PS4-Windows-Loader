#include "pwl_handoff.h"

#define BIT(n) (UINT64_C(1) << (n))
#define ROOT_MASK UINT64_C(0x000ffffffffff000)

pwl_status_t pwl_x64_cpu_state_validate(
    const pwl_x64_cpu_state_t *cpu, uint64_t expected_root_pa)
{
    if (cpu == NULL || expected_root_pa == 0 ||
        (expected_root_pa & ~ROOT_MASK) != 0 ||
        (cpu->cr0 & (BIT(0) | BIT(16) | BIT(31))) !=
            (BIT(0) | BIT(16) | BIT(31)) ||
        (cpu->cr4 & BIT(5)) == 0 || /* PAE */
        (cpu->cr4 & BIT(12)) != 0 || /* LA57: four-level walk unsupported */
        (cpu->efer & (BIT(8) | BIT(10) | BIT(11))) !=
            (BIT(8) | BIT(10) | BIT(11)) || /* LME, LMA, NXE */
        (cpu->cr3 & ROOT_MASK) != expected_root_pa ||
        /* PCID cannot be treated as a root-address suffix in this contract. */
        (cpu->cr4 & BIT(17)) != 0 ||
        (cpu->cr3 & ~ROOT_MASK) != 0)
        return PWL_ERR_INVALID_ARGUMENT;
    return PWL_OK;
}
