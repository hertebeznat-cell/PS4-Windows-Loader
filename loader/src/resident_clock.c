#include "pwl_resident.h"
#define EFI __attribute__((ms_abi))
extern const uint64_t pwl_resident_binding __attribute__((visibility("hidden")));
static pwl_resident_data_t *state(void)
{ return (pwl_resident_data_t *)(uintptr_t)pwl_resident_binding; }
static int clock_valid(const pwl_resident_data_t *d)
{
    return d && d->clock.ready && d->clock.frequency_hz &&
        d->clock.frequency_hz<=UINT64_C(10000000000);
}
uint64_t pwl_resident_clock_now(pwl_resident_data_t *d,uint64_t *ticks)
{
    if (!ticks || !clock_valid(d)) return PWL_EFI_UNSUPPORTED;
    unsigned low,high,before,c;
    /* CPUID brackets the sample on older AMD64 implementations too; no
     * assumption about LFENCE dispatch serialization or writable MSRs. */
    __asm__ volatile("cpuid; rdtsc" : "=a"(low),"=d"(high),"=b"(before),"=c"(c) : "0"(1U) : "memory");
    unsigned a=1,b,e;
    __asm__ volatile("cpuid" : "+a"(a),"=b"(b),"=c"(c),"=d"(e) :: "memory");
    uint64_t now=((uint64_t)high<<32)|low;
    if(d->time_seed.frequency_hz &&
       ((before>>24)!=d->time_seed.cpu || (b>>24)!=d->time_seed.cpu)) {
        d->clock.ready=0;return PWL_EFI_DEVICE_ERROR;
    }
    if (now<d->clock.last_tsc) {
        d->clock.ready=0;return PWL_EFI_DEVICE_ERROR;
    }
    d->clock.last_tsc=now;*ticks=now;
    return PWL_EFI_SUCCESS;
}
uint64_t pwl_resident_clock_ticks(const pwl_resident_data_t *d,uint64_t units,
    uint64_t denominator,uint64_t *ticks)
{
    if (!ticks || !clock_valid(d)) return PWL_EFI_UNSUPPORTED;
    if (denominator!=UINT64_C(1000000) && denominator!=UINT64_C(10000000))
        return PWL_EFI_INVALID_PARAMETER;
    uint64_t hz=d->clock.frequency_hz,seconds=units/denominator;
    uint64_t fraction=((units%denominator)*hz+denominator-1)/denominator;
    if (seconds>UINT64_MAX/hz || seconds*hz>UINT64_MAX-fraction)
        return PWL_EFI_INVALID_PARAMETER;
    uint64_t result=seconds*hz+fraction;
    if (result>INT64_MAX) return PWL_EFI_INVALID_PARAMETER;
    *ticks=result;
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_stall(uint64_t microseconds)
{
    pwl_resident_data_t *d=state();
    if (!d) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    if (!microseconds) return PWL_EFI_SUCCESS;
    uint64_t ticks,start,now;
    uint64_t status=pwl_resident_clock_ticks(d,microseconds,1000000,&ticks);
    if (status) return status;
    status=pwl_resident_clock_now(d,&start);
    if (status) return status;
    do {
        __asm__ volatile("pause" ::: "memory");
        status=pwl_resident_clock_now(d,&now);
        if (status) return status;
    } while (now-start<ticks);
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_set_watchdog_timer(size_t seconds,uint64_t code,size_t bytes,
    const uint16_t *data)
{
    pwl_resident_data_t *d=state();
    (void)code;(void)bytes;(void)data;
    if (!d) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    /* No watchdog is armed by this firmware. A disable request is valid; a
     * positive timeout needs a native reset backend and must not fake success. */
    return seconds ? PWL_EFI_UNSUPPORTED : PWL_EFI_SUCCESS;
}
