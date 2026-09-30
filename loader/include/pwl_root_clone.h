#ifndef PWL_ROOT_CLONE_H
#define PWL_ROOT_CLONE_H
#include "pwl_handoff.h"
#include "pwl_address_call.h"
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
#endif
