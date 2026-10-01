#include "pwl_resident.h"
#define EFI __attribute__((ms_abi))
extern const uint64_t pwl_resident_binding __attribute__((visibility("hidden")));
uint64_t EFI pwl_resident_get_time(pwl_efi_time_t *time,pwl_efi_time_capabilities_t *caps)
{
    pwl_resident_data_t *d=(void *)(uintptr_t)pwl_resident_binding;
    if(!d || !time)return PWL_EFI_INVALID_PARAMETER;
    /* Boot-time observed UTC is not a persistent runtime RTC. The source
     * supplies no hardware accuracy capabilities; do not invent them. */
    if(d->memory.exited || !d->time_seed.frequency_hz || caps)return PWL_EFI_UNSUPPORTED;
    if(d->clock.frequency_hz!=d->time_seed.frequency_hz)return PWL_EFI_DEVICE_ERROR;
    uint64_t now,status=pwl_resident_clock_now(d,&now);if(status)return status;
    pwl_efi_time_t value;
    if(pwl_time_at(&d->time_seed,now,&value)!=PWL_OK)return PWL_EFI_DEVICE_ERROR;
    *time=value;return PWL_EFI_SUCCESS;
}
