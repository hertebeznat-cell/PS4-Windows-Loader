#define _GNU_SOURCE
#include "resident_test_image.h"
#include <time.h>
#include <unistd.h>
typedef uint64_t (EFI *create_fn)(uint32_t,uint64_t,pwl_efi_event_notify_t,void *,uint64_t *);
typedef uint64_t (EFI *create_ex_fn)(uint32_t,uint64_t,pwl_efi_event_notify_t,const void *,const pwl_efi_guid_t *,uint64_t *);
typedef uint64_t (EFI *event_fn)(uint64_t);
typedef uint64_t (EFI *timer_fn)(uint64_t,uint32_t,uint64_t);
typedef uint64_t (EFI *wait_fn)(size_t,const uint64_t *,size_t *);
typedef uint64_t (EFI *stall_fn)(uint64_t);
typedef uint64_t (EFI *raise_fn)(uint64_t);
typedef void (EFI *restore_fn)(uint64_t);
typedef uint64_t (EFI *watchdog_fn)(size_t,uint64_t,size_t,const uint16_t *);
static uint64_t sample_tsc(void)
{
    unsigned low,high;
    __asm__ volatile("cpuid; rdtsc" : "=a"(low),"=d"(high) : "0"(0U) : "rbx","rcx","memory");
    unsigned a=0,b,c,d;
    __asm__ volatile("cpuid" : "+a"(a),"=b"(b),"=c"(c),"=d"(d) :: "memory");
    return ((uint64_t)high<<32)|low;
}
static uint64_t host_nanoseconds(void)
{
    struct timespec now;assert(clock_gettime(CLOCK_MONOTONIC,&now)==0);
    return (uint64_t)now.tv_sec*1000000000+(uint64_t)now.tv_nsec;
}
static uint64_t calibrate(void)
{
    /* Only the test harness uses the host clock. Firmware calls execute real
     * RDTSC in copied RX code; no artificial time source is linked into it. */
    uint64_t start=host_nanoseconds(),first=sample_tsc(),last,stop;
    do { stop=host_nanoseconds();last=sample_tsc(); } while(stop-start<UINT64_C(10000000));
    assert(last>first && last-first<UINT64_MAX/1000000000);
    uint64_t hz=(last-first)*1000000000/(stop-start);
    assert(hz && hz<=UINT64_C(10000000000));return hz;
}
static pwl_resident_event_t *event_state(pwl_resident_data_t *d,uint64_t handle)
{
    for(size_t i=0;i<PWL_RESIDENT_EVENTS;i++)if(d->events[i].handle==handle)return &d->events[i];
    assert(0);return NULL;
}
typedef struct notify_context {
    pwl_resident_data_t *data;
    event_fn close;
    uint64_t plain;
    unsigned calls,close_self;
} notify_context_t;
static void EFI notify(uint64_t event,void *context)
{
    notify_context_t *c=context;assert(c->data->tpl==8);c->calls++;
    if(c->plain)assert(event_state(c->data,c->plain)->signaled);
    if(c->close_self)assert(c->close(event)==0);
}
int main(void)
{
    alarm(15); /* A broken blocking timer must fail the suite instead of hanging. */
    resident_test_image_t t=resident_test_open();
    LOAD_FROM(t.data->efi.boot.functions[7],create_fn,create);
    LOAD_FROM(t.data->efi.boot.functions[43],create_ex_fn,create_ex);
    LOAD_FROM(t.data->efi.boot.functions[8],timer_fn,set_timer);
    LOAD_FROM(t.data->efi.boot.functions[9],wait_fn,wait);
    LOAD_FROM(t.data->efi.boot.functions[10],event_fn,signal);
    LOAD_FROM(t.data->efi.boot.functions[11],event_fn,close);
    LOAD_FROM(t.data->efi.boot.functions[12],event_fn,check);
    LOAD_FROM(t.data->efi.boot.functions[28],stall_fn,stall);
    LOAD_FROM(t.data->efi.boot.functions[29],watchdog_fn,watchdog);
    LOAD_FROM(t.data->efi.boot.functions[0],raise_fn,raise_tpl);
    LOAD_FROM(t.data->efi.boot.functions[1],restore_fn,restore_tpl);
    uint64_t event,plain;size_t index=99;
    assert(create(0,0,NULL,NULL,&plain)==0);
    assert(create(PWL_EVT_TIMER,0,NULL,NULL,&event)==0);
    assert(set_timer(event,2,10000)==PWL_EFI_UNSUPPORTED && !event_state(t.data,event)->timer_active);
    assert(set_timer(event,0,UINT64_MAX)==0 && stall(0)==0);
    assert(stall(1)==PWL_EFI_UNSUPPORTED);
    assert(set_timer(plain,2,0)==PWL_EFI_INVALID_PARAMETER);
    assert(set_timer(event,3,0)==PWL_EFI_INVALID_PARAMETER);
    assert(watchdog(0,UINT64_MAX,SIZE_MAX,(void *)(uintptr_t)1)==0);
    assert(watchdog(1,0,0,NULL)==PWL_EFI_UNSUPPORTED);
    uint64_t hz=calibrate();t.data->clock.frequency_hz=hz;t.data->clock.ready=1;
    uint64_t start=sample_tsc();assert(stall(1000)==0);
    assert(sample_tsc()-start>=hz/1000);
    assert(stall(UINT64_MAX)==PWL_EFI_INVALID_PARAMETER);
    /* Relative expiry must not be signaled before the programmed deadline. */
    assert(set_timer(event,2,500000)==0); /* 50 ms leaves room for host scheduling. */
    pwl_resident_event_t *e=event_state(t.data,event);
    uint64_t deadline=e->timer_deadline;
    uint64_t status=check(event);
    assert(status==PWL_EFI_NOT_READY || (status==0 && sample_tsc()>=deadline));
    assert(set_timer(event,2,10000)==0);deadline=e->timer_deadline;
    assert(wait(1,&event,&index)==0 && index==0 && sample_tsc()>=deadline && !e->timer_active);
    assert(check(event)==PWL_EFI_NOT_READY);
    assert(set_timer(event,2,10000)==0 && set_timer(event,0,0)==0 && stall(2000)==0);
    assert(check(event)==PWL_EFI_NOT_READY);
    assert(signal(event)==0 && set_timer(event,0,0)==0 && check(event)==0);
    /* Overflow leaves an already armed timer intact. */
    assert(set_timer(event,1,10000000)==0);
    pwl_resident_event_t before=*e;
    assert(set_timer(event,2,UINT64_MAX)==PWL_EFI_INVALID_PARAMETER && !memcmp(e,&before,sizeof(before)));
    assert(set_timer(event,0,0)==0);
    assert(set_timer(event,1,0)==0 && e->timer_period && e->timer_active);
    deadline=e->timer_deadline;
    assert(wait(1,&event,&index)==0 && sample_tsc()>=deadline && e->timer_active);
    assert(wait(1,&event,&index)==0 && e->timer_deadline>deadline);
    assert(set_timer(event,0,0)==0);
    notify_context_t c={.data=t.data,.close=close};uint64_t notify_event;
    assert(create(PWL_EVT_TIMER|PWL_EVT_NOTIFY_SIGNAL,8,notify,&c,&notify_event)==0);
    assert(set_timer(notify_event,2,10000)==0 && raise_tpl(31)==4);
    assert(stall(2000)==0 && !c.calls);restore_tpl(4);
    assert(c.calls==1 && !event_state(t.data,notify_event)->timer_active);
    assert(set_timer(notify_event,1,10000)==0 && raise_tpl(31)==4 && stall(5000)==0);
    /* Cancel future expiry, but retain an already queued notification. */
    assert(check(plain)==PWL_EFI_NOT_READY && c.calls==1);
    unsigned prior=c.calls;
    assert(set_timer(notify_event,0,0)==0);restore_tpl(4);assert(c.calls==prior+1);
    c.close_self=1;
    assert(set_timer(notify_event,2,0)==0 && stall(2000)==0);
    assert(check(plain)==PWL_EFI_NOT_READY && c.calls==prior+2);
    assert(close(notify_event)==PWL_EFI_INVALID_PARAMETER);
    /* A timer signals every group member before delivering a callback. */
    c.close_self=0;c.plain=plain;
    pwl_efi_guid_t group={{0x93}};
    assert(close(plain)==0 && create_ex(0,0,NULL,NULL,&group,&plain)==0);c.plain=plain;
    assert(create_ex(PWL_EVT_TIMER|PWL_EVT_NOTIFY_SIGNAL,8,notify,&c,&group,&notify_event)==0);
    assert(set_timer(notify_event,2,10000)==0 && wait(1,&plain,&index)==0);
    assert(close(notify_event)==0 && close(plain)==0);
    /* A regressing/invalid clock is quarantined, never treated as expiry. */
    assert(set_timer(event,2,10000)==0);
    t.data->clock.last_tsc=UINT64_MAX;
    assert(check(event)==PWL_EFI_DEVICE_ERROR && !t.data->clock.ready && event_state(t.data,event)->timer_active);
    assert(set_timer(event,0,0)==0);
    t.data->clock.ready=1;t.data->clock.last_tsc=UINT64_MAX;
    assert(stall(1)==PWL_EFI_DEVICE_ERROR && !t.data->clock.ready);
    assert(set_timer(event,2,10000)==PWL_EFI_UNSUPPORTED);
    t.data->clock.ready=1;t.data->clock.last_tsc=0;t.data->clock.frequency_hz=UINT64_MAX;
    assert(stall(1)==PWL_EFI_UNSUPPORTED);
    t.data->clock.frequency_hz=hz;
    t.data->memory.exited=1;
    assert(stall(0)==PWL_EFI_ACCESS_DENIED && watchdog(0,0,0,NULL)==PWL_EFI_ACCESS_DENIED);
    assert(set_timer(event,0,0)==PWL_EFI_ACCESS_DENIED);
    t.data->memory.exited=0;assert(close(event)==0);
    resident_test_close(&t);alarm(0);
    puts("resident timers: copied RX real TSC, relative/periodic expiry, cancellation, TPL and clock refusal passed");
}
