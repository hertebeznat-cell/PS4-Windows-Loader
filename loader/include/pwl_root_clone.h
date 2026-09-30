#ifndef PWL_ROOT_CLONE_H
#define PWL_ROOT_CLONE_H
#include "pwl_handoff.h"
#include "pwl_address_call.h"
/* Same-build direct map covers 32 GiB from the supplied PDPT slot. */
pwl_status_t pwl_x64_root_direct_address(uint32_t pml4_index,uint32_t pdpt_index,
    uint64_t physical,uint64_t *address);
pwl_status_t pwl_x64_root_clone_prepare(const pwl_x64_cpu_state_t *cpu,
    const volatile uint64_t *source,uint64_t *destination);
/* Identical-mapping experiment only. Same six report offsets as address_call;
 * checks expected CR3 and all 512 source/clone entries with IRQs masked before
 * switching. -1: root changed; -2: snapshot changed; 0: switch returned.
 * Caller verifies source PA==active root, destination PA/stack ownership and
 * same current mappings. No call is made while using the clone. */
int pwl_x64_root_clone_call(uint64_t root,uint64_t stack_top,
    pwl_address_call_report_t *report,const volatile uint64_t *source,
    const uint64_t *clone,uint64_t expected_root);
typedef struct pwl_root_efi_call {
    uint64_t expected_root;
    int (*callback)(void *);
    void *context;
} pwl_root_efi_call_t;
_Static_assert(offsetof(pwl_root_efi_call_t,callback)==8,"root callback ABI");
_Static_assert(offsetof(pwl_root_efi_call_t,context)==16,"root context ABI");
/* Same snapshot gates as root_clone_call; only a prepared, resident callback.
 * No SDK, allocation, USB or host calls are allowed under the clone. */
int pwl_x64_root_efi_call(uint64_t root,uint64_t stack_top,
    pwl_address_call_report_t *report,const volatile uint64_t *source,
    const uint64_t *clone,const pwl_root_efi_call_t *call);
#endif
