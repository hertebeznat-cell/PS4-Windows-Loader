#ifndef PWL_PLATFORM_CALL_H
#define PWL_PLATFORM_CALL_H
#include "pwl_cpu_environment.h"
#include "pwl_devices.h"
#include "pwl_firmware.h"
typedef struct pwl_platform_backend {
    void *context;
    /* Actual same-target hooks, not success placeholders. Claim serializes
     * the complete device/CPU inventory; freeze parks other CPUs, takes FP
     * ownership and masks controllers/IRQs. Thaw restores that saved state.
     * All hooks must be idempotent on retry and undo partial failed attempts.
     * They execute in the original preparation context, never under EFI root.
     * Target discovery and hardware hook implementation remain platform work. */
    pwl_status_t (*claim)(void *);
    pwl_status_t (*freeze)(void *);
    pwl_status_t (*thaw)(void *);
    pwl_status_t (*release)(void *);
    pwl_device_ops_t devices;
} pwl_platform_backend_t;
typedef struct pwl_platform_call_state {
    pwl_cpu_environment_t before,after;
    pwl_device_lease_t devices;
    unsigned claimed,frozen,executed,context_changed,transferred;
    pwl_status_t operation_status,cleanup_status;
} pwl_platform_call_state_t;
/* Actual CPL0 entry: claim -> device stop/drain/mask -> CPU freeze -> checked
 * returning call -> device config restore -> CPU thaw -> driver resume ->
 * release claim. Config restore precedes re-enabling platform interrupts.
 * Caller already prepared/audited call/tables and pins backend/state/owners.
 * Fresh zero state; callbacks and state must not alias or modify the call.
 * Missing hooks/CPL3 refuse before hardware effects. After context mismatch or
 * memory-service retirement, do NOT call preparation hooks, free or unpin.
 * Cleanup failure retains its exact remaining lease for explicit retry.
 * No default PS4 hook set is supplied; this is not a hardware readiness bit.
 */
pwl_status_t pwl_native_platform_call(const pwl_platform_backend_t *,const uint64_t *,size_t,
    pwl_native_call_t *,const pwl_fw_memory_t *,pwl_platform_call_state_t *);
pwl_status_t pwl_native_platform_cleanup(const pwl_platform_backend_t *,pwl_platform_call_state_t *);
#endif
