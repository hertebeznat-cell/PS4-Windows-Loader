#ifndef PWL_NATIVE_CALL_H
#define PWL_NATIVE_CALL_H
#include "pwl_efi_entry.h"
#include "pwl_address_call.h"

#define PWL_FP_MAX_BYTES 65536U
#define PWL_FP_FXSAVE 1U
#define PWL_FP_XSAVE 2U
typedef struct pwl_fp_layout {
    uint64_t bytes, mask, kind;
} pwl_fp_layout_t;
/* CPUID/XGETBV only. Reads the full enabled user-state mask and standard save
 * area size. Does not touch floating-point registers. Native entry separately
 * restricts the supported CPU contract to x87/SSE and optional AVX. */
pwl_status_t pwl_x64_fp_layout_read(pwl_fp_layout_t *out);
pwl_status_t pwl_x64_native_cpu_validate(const pwl_x64_cpu_state_t *cpu,
    const pwl_fp_layout_t *fp);

typedef struct pwl_native_call {
    pwl_x64_cpu_state_t expected;
    uint64_t root, stack_top, callback, context;
    uint64_t fp_address, fp_bytes, fp_mask, fp_kind;
    uint64_t old_stack_base, old_stack_bytes;
    pwl_address_call_report_t addresses;
    uint64_t cr0_entered, cr4_entered;
    uint64_t fp_saved, fp_restored, callback_status, returned;
    int64_t status;
    uint64_t reserved;
} pwl_native_call_t;
_Static_assert(sizeof(pwl_native_call_t)==224,"native call ABI");
_Static_assert(offsetof(pwl_native_call_t,fp_address)==64,"FP save address ABI");
_Static_assert(offsetof(pwl_native_call_t,addresses)==112,"address report ABI");
_Static_assert(offsetof(pwl_native_call_t,status)==208,"call result ABI");

typedef struct pwl_native_call_storage {
    /* Owned, page-aligned contiguous RAM, mapped at prepare_address with
     * identical supervisor RW/NX translations under both roots. Contains
     * call record, aligned FP save area and EFI entry arguments. */
    pwl_owned_span_t control;
    uint64_t old_stack_base, old_stack_bytes;
} pwl_native_call_storage_t;

/* Audit both-root code/control/old-stack/new-stack continuity and physical
 * control placement, then construct one call record and resident EFI context.
 * No CPU changes. Outputs unchanged on failure. Exclusive stable tables,
 * complete exception dependencies, AP/DMA and FP ownership remain platform
 * contracts; this function cannot certify them. */
pwl_status_t pwl_native_call_prepare(const pwl_native_workspace_t *workspace,
    const pwl_resident_image_t *image,const pwl_native_transition_plan_t *plan,
    const pwl_x64_table_page_t *tables,const pwl_x64_cpu_state_t *cpu,
    const pwl_x64_table_page_t *before,size_t before_count,
    const pwl_fp_layout_t *fp,const pwl_native_call_storage_t *storage,
    pwl_native_call_t **out);
pwl_status_t pwl_native_call_result_validate(const pwl_native_call_t *call);

/* Controlled CPL0 returning call. CPL3 returns ACCESS_DENIED before reading
 * the argument or executing privileged instructions. A validated owned call
 * is mandatory in CPL0. Live CR0/3/4/EFER/FP layout and old RSP are rechecked
 * with IRQs masked. Saves/restores FP state, clears TS and PGE temporarily,
 * switches CR3/stack, calls adapter, restores state and caller flags.
 * No exception recovery, AP parking, DMA handoff or ExitBootServices.
 * The callback must return in the same CPU mode, preserve XCR0 and descriptor
 * /GS state, leave interrupts masked, and keep all owners/mappings alive. */
int __attribute__((visibility("hidden"))) pwl_x64_native_call(pwl_native_call_t *call);
extern const unsigned char pwl_x64_native_call_end[] __attribute__((visibility("hidden")));
/* Trusted FP primitives used by the call and native host ABI tests. Require
 * TS/EM clear, correct live layout and 64-byte-aligned sufficiently sized RAM.
 * The restore primitive intentionally restores registers across its return. */
void pwl_x64_fp_save(void *buffer,uint64_t mask,uint64_t kind);
void pwl_x64_fp_restore(const void *buffer,uint64_t mask,uint64_t kind);
#endif
