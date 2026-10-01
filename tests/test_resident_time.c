#define _GNU_SOURCE
#include "resident_test_image.h"
typedef uint64_t (EFI *get_time_fn)(pwl_efi_time_t *,pwl_efi_time_capabilities_t *);
static uint64_t tsc(uint32_t *cpu)
{unsigned a,b,c,d;__asm__ volatile("cpuid;rdtsc":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1):"memory");
 *cpu=b>>24;return (uint64_t)a|((uint64_t)d<<32);}
int main(void)
{
    pwl_time_sample_t samples[5];
    for(size_t i=0;i<5;i++)samples[i]=(pwl_time_sample_t){1709164799500000+i*10000,
        100000000+i*20000000,100000100+i*20000000,3,3};
    pwl_time_seed_t s;assert(pwl_time_calibrate(samples,5,&s)==PWL_OK && s.frequency_hz==2000000000);
    pwl_efi_time_t time;
    assert(pwl_time_at(&s,s.tsc,&time)==PWL_OK && time.year==2024 && time.month==2 &&
        time.day==28 && time.hour==23 && time.minute==59 && time.second==59 && time.nanosecond==540000000);
    assert(pwl_time_at(&s,s.tsc+UINT64_C(2000000000),&time)==PWL_OK && time.day==29 && !time.hour && !time.minute && !time.second);
    pwl_time_seed_t sentinel=s;samples[2].cpu_after=4;
    assert(pwl_time_calibrate(samples,5,&s)==PWL_ERR_BAD_IMAGE && !memcmp(&s,&sentinel,sizeof(s)));
    samples[2].cpu_after=3;samples[2].utc_microseconds--;
    assert(pwl_time_calibrate(samples,5,&s)==PWL_OK);
    samples[2].tsc_after=samples[2].tsc_before+10000000;
    assert(pwl_time_calibrate(samples,5,&s)==PWL_ERR_BAD_IMAGE);
    samples[2].tsc_after=samples[2].tsc_before+100;samples[2].utc_microseconds+=2000;
    sentinel=s;
    assert(pwl_time_calibrate(samples,5,&s)==PWL_ERR_BAD_IMAGE && !memcmp(&s,&sentinel,sizeof(s)));
    s=(pwl_time_seed_t){PWL_UTC_LAST_SECOND,1,1000000,999999999,0};
    assert(pwl_time_at(&s,1,&time)==PWL_OK && time.year==9999 && time.month==12 && time.day==31);
    assert(pwl_time_at(&s,2,&time)==PWL_ERR_INVALID_ARGUMENT);
    resident_test_image_t r=resident_test_open();LOAD_FROM(r.data->efi.runtime.functions[0],get_time_fn,get);
    memset(&time,0xaa,sizeof(time));pwl_efi_time_t original=time;
    assert(get(&time,NULL)==PWL_EFI_UNSUPPORTED && !memcmp(&time,&original,sizeof(time)));
    uint32_t cpu;uint64_t now=tsc(&cpu);
    r.data->time_seed=(pwl_time_seed_t){1709164799,now,2000000000,0,cpu};
    r.data->clock.frequency_hz=2000000000;r.data->clock.ready=1;
    assert(get(&time,NULL)==PWL_EFI_SUCCESS && time.year==2024 && time.month==2 && time.day==28);
    pwl_efi_time_capabilities_t caps={0};assert(get(&time,&caps)==PWL_EFI_UNSUPPORTED);
    r.data->time_seed.cpu=cpu+1;original=time;
    assert(get(&time,NULL)==PWL_EFI_DEVICE_ERROR && !memcmp(&time,&original,sizeof(time)) && !r.data->clock.ready);
    r.data->memory.exited=1;assert(get(&time,NULL)==PWL_EFI_UNSUPPORTED);
    resident_test_close(&r);
    puts("UEFI GetTime: real copied RX ABI/TSC, UTC/leap day/year limit, calibration drift/migration/refusal; boot-only, no RTC accuracy claim");
}
