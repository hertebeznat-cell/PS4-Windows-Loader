#ifndef PWL_CPU_ENVIRONMENT_H
#define PWL_CPU_ENVIRONMENT_H
#include "pwl_native_call.h"
typedef struct __attribute__((packed)) pwl_x64_descriptor_register {
    uint16_t limit;uint64_t base;
} pwl_x64_descriptor_register_t;
typedef struct pwl_cpu_environment {
    pwl_x64_cpu_state_t cpu;
    pwl_fp_layout_t fp;
    pwl_x64_descriptor_register_t gdt,idt;
    uint64_t flags,fs_base,gs_base,kernel_gs_base,pat,thread,pcb,apic_base;
    uint64_t mtrr_cap,mtrr_default,mtrr_variable[32],mtrr_fixed[11];
    uint16_t cs,ss,ds,es,fs,gs,tr,ldt;
    uint32_t apic_id;
} pwl_cpu_environment_t;
/* Actual CPL0-only capture. CPL3 refused before examining output. Captures
 * registers, GS thread/PCB anchors and FP layout; never dereferences their
 * targets or changes hardware. Trusted current GS/MSR layout and resident
 * output/stack are preparation contracts. A snapshot cannot prove AP parking,
 * FP ownership, exception recovery, device drain or complete mappings. */
pwl_status_t pwl_x64_cpu_environment_read(pwl_cpu_environment_t *);
/* Compare mode/descriptors/bases/PAT/thread/PCB/APIC/FP and IF/DF. Arithmetic
 * flags are not stable across calls. Does not prove the targets are pinned. */
pwl_status_t pwl_cpu_environment_equal(const pwl_cpu_environment_t *,
    const pwl_cpu_environment_t *);
/* Returning entry with before/after live context capture. Actual call and
 * captured caller must agree, including the pinned original thread/CPU. It
 * requires distinct non-overlapping call/before/after records. It
 * detects changed descriptor/GS/PAT state on return; it supplies no recovery
 * for a trap or a callback that breaks the suspended caller's mappings. */
pwl_status_t pwl_native_call_checked(pwl_native_call_t *,pwl_cpu_environment_t *,
    pwl_cpu_environment_t *);
#endif
