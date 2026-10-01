#include "pwl_time.h"
_Static_assert(sizeof(pwl_efi_time_t)==16,"EFI_TIME layout");
_Static_assert(sizeof(pwl_efi_time_capabilities_t)==12,"EFI time capabilities layout");
static uint64_t midpoint(const pwl_time_sample_t *s)
{return s->tsc_before+(s->tsc_after-s->tsc_before)/2;}
pwl_status_t pwl_time_calibrate(const pwl_time_sample_t *samples,size_t n,pwl_time_seed_t *out)
{
    if(!samples || !out || n<5 || n>PWL_TIME_SAMPLES)return PWL_ERR_INVALID_ARGUMENT;
    uint64_t rates[PWL_TIME_SAMPLES-1];
    for(size_t i=0;i<n;i++) {
        const pwl_time_sample_t *s=&samples[i];
        if(s->cpu_before!=samples[0].cpu_before || s->cpu_after!=s->cpu_before ||
           !s->tsc_before || s->tsc_after<s->tsc_before ||
           s->utc_microseconds/1000000>PWL_UTC_LAST_SECOND)return PWL_ERR_BAD_IMAGE;
        if(i) {
            uint64_t dt=midpoint(s)-midpoint(&samples[i-1]);
            if(midpoint(s)<=midpoint(&samples[i-1]) ||
               s->utc_microseconds<=samples[i-1].utc_microseconds)return PWL_ERR_BAD_IMAGE;
            uint64_t us=s->utc_microseconds-samples[i-1].utc_microseconds;
            if(us<5000 || us>1000000 || dt>UINT64_MAX/1000000 ||
               s->tsc_after-s->tsc_before>dt/200 ||
               samples[i-1].tsc_after-samples[i-1].tsc_before>dt/200)return PWL_ERR_BAD_IMAGE;
            rates[i-1]=dt*1000000/us;
            if(rates[i-1]<1000000 || rates[i-1]>UINT64_C(10000000000))return PWL_ERR_BAD_IMAGE;
        }
    }
    for(size_t i=1;i<n-1;i++) {
        uint64_t value=rates[i];size_t j=i;
        while(j && rates[j-1]>value) {rates[j]=rates[j-1];j--;}
        rates[j]=value;
    }
    uint64_t hz=rates[(n-1)/2];
    for(size_t i=0;i<n-1;i++)if(rates[i]>hz+hz/100 || rates[i]<hz-hz/100)return PWL_ERR_BAD_IMAGE;
    const pwl_time_sample_t *last=&samples[n-1];
    *out=(pwl_time_seed_t){last->utc_microseconds/1000000,midpoint(last),hz,
        (uint32_t)((last->utc_microseconds%1000000)*1000),last->cpu_before};
    return PWL_OK;
}
static int leap(unsigned year)
{return !(year%4) && (year%100 || !(year%400));}
pwl_status_t pwl_time_at(const pwl_time_seed_t *s,uint64_t now,pwl_efi_time_t *out)
{
    if(!s || !out || !s->tsc || now<s->tsc || s->seconds>PWL_UTC_LAST_SECOND ||
       s->nanoseconds>=1000000000 || s->frequency_hz<1000000 ||
       s->frequency_hz>UINT64_C(10000000000))return PWL_ERR_INVALID_ARGUMENT;
    uint64_t delta=now-s->tsc,seconds=delta/s->frequency_hz;
    uint64_t nano=(delta%s->frequency_hz)*1000000000/s->frequency_hz+s->nanoseconds;
    seconds+=nano/1000000000;nano%=1000000000;
    if(seconds>PWL_UTC_LAST_SECOND-s->seconds)return PWL_ERR_INVALID_ARGUMENT;
    seconds+=s->seconds;uint64_t days=seconds/86400;unsigned year=1970;
    while(days>=(uint64_t)(leap(year)?366:365)) {days-=(unsigned)(leap(year)?366:365);year++;}
    static const unsigned char lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned month=0;
    for(;;) {unsigned length=lengths[month]+(unsigned)(month==1 && leap(year));
        if(days<length)break;
        days-=length;month++;}
    seconds%=86400;
    *out=(pwl_efi_time_t){(uint16_t)year,(uint8_t)(month+1),(uint8_t)(days+1),
        (uint8_t)(seconds/3600),(uint8_t)((seconds/60)%60),(uint8_t)(seconds%60),0,
        (uint32_t)nano,0,0,0};
    return PWL_OK;
}
