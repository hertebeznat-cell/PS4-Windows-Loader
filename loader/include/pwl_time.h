#ifndef PWL_TIME_H
#define PWL_TIME_H
#include "pwl.h"
#define PWL_TIME_SAMPLES 16U
#define PWL_UTC_LAST_SECOND UINT64_C(253402300799)
typedef struct pwl_efi_time {
    uint16_t year;uint8_t month,day,hour,minute,second,pad1;
    uint32_t nanosecond;int16_t timezone;uint8_t daylight,pad2;
} pwl_efi_time_t;
typedef struct pwl_efi_time_capabilities {
    uint32_t resolution,accuracy;uint8_t sets_to_zero,padding[3];
} pwl_efi_time_capabilities_t;
typedef struct pwl_time_sample {
    uint64_t utc_microseconds,tsc_before,tsc_after;
    uint32_t cpu_before,cpu_after;
} pwl_time_sample_t;
typedef struct pwl_time_seed {
    uint64_t seconds,tsc,frequency_hz;
    uint32_t nanoseconds,cpu;
} pwl_time_seed_t;
/* Pure calibration over ACTUAL platform samples. Does not certify invariant
 * TSC or reference-clock provenance; platform adapter must check those.
 * Requires 5..16 same-CPU bounded samples, stable increasing UTC/TSC, narrow
 * sampling brackets and <=1% frequency variation. Output unchanged on error.
 * Estimates an observed boot-time clock; no RTC accuracy/persistence claims. */
pwl_status_t pwl_time_calibrate(const pwl_time_sample_t *,size_t,pwl_time_seed_t *);
pwl_status_t pwl_time_at(const pwl_time_seed_t *,uint64_t,pwl_efi_time_t *);
#endif
