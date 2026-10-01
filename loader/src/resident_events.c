#include "pwl_resident.h"

#define EFI __attribute__((ms_abi))
extern const uint64_t pwl_resident_binding __attribute__((visibility("hidden")));
static pwl_resident_data_t *state(void)
{ return (pwl_resident_data_t *)(uintptr_t)pwl_resident_binding; }

/* Serialized, boot-only software events. No host clock, interrupts or process
 * services are used. Handles are monotonically assigned opaque values, never
 * dereferenced and never reused after CloseEvent. */
static pwl_resident_event_t *find_event(pwl_resident_data_t *d,uint64_t handle)
{
    if (handle)
        for (size_t i=0;i<PWL_RESIDENT_EVENTS;i++)
            if (d->events[i].handle==handle) return &d->events[i];
    return NULL;
}
static int same_group(const pwl_efi_guid_t *a,const pwl_efi_guid_t *b)
{
    for (size_t i=0;i<16;i++) if (a->bytes[i]!=b->bytes[i]) return 0;
    return 1;
}
static void queue(pwl_resident_data_t *d,const pwl_resident_event_t *e)
{
    unsigned q=e->notify_tpl==16;
    unsigned n=d->event_queue_count[q];
    for (unsigned i=0;i<n;i++) if (d->event_queue[q][i]==e->handle) return;
    /* At most one queued notification per live event, so n cannot exceed 32. */
    if (n<PWL_RESIDENT_EVENTS) {
        d->event_queue[q][n]=e->handle;
        d->event_queue_count[q]=n+1;
    }
}
static uint64_t dequeue(pwl_resident_data_t *d,unsigned q,unsigned index)
{
    uint64_t handle=d->event_queue[q][index];
    unsigned n=--d->event_queue_count[q];
    for (unsigned i=index;i<n;i++) d->event_queue[q][i]=d->event_queue[q][i+1];
    d->event_queue[q][n]=0;
    return handle;
}
void pwl_resident_events_dispatch(pwl_resident_data_t *d)
{
    if (!d || d->memory.exited) return;
    uint64_t floor=d->tpl;
    for (;;) {
        unsigned q;
        if (floor<16 && d->event_queue_count[1]) q=1;
        else if (floor<8 && d->event_queue_count[0]) q=0;
        else break;
        uint64_t handle=dequeue(d,q,0);
        pwl_resident_event_t *e=find_event(d,handle);
        if (!e) continue;
        pwl_efi_event_notify_t notify=e->notify;
        const void *context=e->context;
        if (e->type==PWL_EVT_NOTIFY_SIGNAL) e->signaled=0;
        d->tpl=q ? 16 : 8;
        /* A callback may close/recreate this slot or signal higher-priority
         * events. Do not access e after the call. Nested dispatch only runs
         * priorities above the currently executing callback. */
        notify(handle,(void *)context);
        d->tpl=floor;
        if (d->memory.exited) break;
    }
}
uint64_t EFI pwl_resident_create_event_ex(uint32_t type,uint64_t tpl,
    pwl_efi_event_notify_t notify,const void *context,const pwl_efi_guid_t *group,
    uint64_t *event)
{
    pwl_resident_data_t *d=state();
    if (!d || !event) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    switch (type) {
    case 0: case PWL_EVT_NOTIFY_WAIT: case PWL_EVT_NOTIFY_SIGNAL: break;
    case UINT32_C(0x80000000): case UINT32_C(0x80000100): case UINT32_C(0x80000200):
    case UINT32_C(0x201): case UINT32_C(0x60000202):
        return PWL_EFI_UNSUPPORTED; /* Timer/runtime/automatic platform events. */
    default: return PWL_EFI_INVALID_PARAMETER;
    }
    if (type && (!notify || (tpl!=8 && tpl!=16))) return PWL_EFI_INVALID_PARAMETER;
    if (d->event_next_handle==UINT64_MAX) return PWL_EFI_OUT_OF_RESOURCES;
    for (size_t i=0;i<PWL_RESIDENT_EVENTS;i++) {
        pwl_resident_event_t *e=&d->events[i];
        if (e->handle) continue;
        pwl_efi_guid_t copy={{0}};
        if (group) copy=*group;
        *e=(pwl_resident_event_t){0};
        e->handle=++d->event_next_handle;
        e->type=type;
        e->group=copy;e->grouped=group!=NULL;
        if (type) { e->notify_tpl=tpl;e->notify=notify;e->context=context; }
        *event=e->handle;
        return PWL_EFI_SUCCESS;
    }
    return PWL_EFI_OUT_OF_RESOURCES;
}
uint64_t EFI pwl_resident_create_event(uint32_t type,uint64_t tpl,
    pwl_efi_event_notify_t notify,void *context,uint64_t *event)
{ return pwl_resident_create_event_ex(type,tpl,notify,context,NULL,event); }

static void signal_one(pwl_resident_data_t *d,pwl_resident_event_t *e)
{
    if (e->signaled) return;
    e->signaled=1;
    if (e->type==PWL_EVT_NOTIFY_SIGNAL) queue(d,e);
}
void pwl_resident_signal_group(pwl_resident_data_t *d,const pwl_efi_guid_t *group)
{
    if (!d || !group || d->memory.exited) return;
    pwl_efi_guid_t copy=*group;
    for (size_t i=0;i<PWL_RESIDENT_EVENTS;i++)
        if (d->events[i].handle && d->events[i].grouped &&
            same_group(&d->events[i].group,&copy)) signal_one(d,&d->events[i]);
    pwl_resident_events_dispatch(d);
}
uint64_t EFI pwl_resident_signal_event(uint64_t handle)
{
    pwl_resident_data_t *d=state();
    if (!d) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    pwl_resident_event_t *e=find_event(d,handle);
    if (!e) return PWL_EFI_INVALID_PARAMETER;
    if (e->grouped) {
        /* Mark ALL group members before delivering the first notification. */
        pwl_resident_signal_group(d,&e->group);
    } else {
        signal_one(d,e);pwl_resident_events_dispatch(d);
    }
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_close_event(uint64_t handle)
{
    pwl_resident_data_t *d=state();
    if (!d) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    pwl_resident_event_t *e=find_event(d,handle);
    if (!e) return PWL_EFI_INVALID_PARAMETER;
    for (unsigned q=0;q<2;q++)
        for (unsigned i=0;i<d->event_queue_count[q];i++)
            if (d->event_queue[q][i]==handle) { (void)dequeue(d,q,i);break; }
    *e=(pwl_resident_event_t){0};
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_check_event(uint64_t handle)
{
    pwl_resident_data_t *d=state();
    if (!d) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    pwl_resident_event_t *e=find_event(d,handle);
    if (!e || e->type==PWL_EVT_NOTIFY_SIGNAL) return PWL_EFI_INVALID_PARAMETER;
    if (!e->signaled && e->type==PWL_EVT_NOTIFY_WAIT) {
        queue(d,e);pwl_resident_events_dispatch(d);
        e=find_event(d,handle); /* Callback may have closed the event. */
        if (!e) return PWL_EFI_INVALID_PARAMETER;
    }
    if (!e->signaled) return PWL_EFI_NOT_READY;
    e->signaled=0;
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_wait_for_event(size_t count,const uint64_t *events,size_t *index)
{
    pwl_resident_data_t *d=state();
    if (!d || !count || !events || !index) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_ACCESS_DENIED;
    if (d->tpl!=4) return PWL_EFI_UNSUPPORTED;
    /* Blocking cooperative polling, in array order. No fake timeout/NOT_READY
     * return. Without a notification producer, an unsignaled event waits
     * indefinitely, as specified. Hardware idle and timer sources are absent. */
    for (;;) {
        for (size_t i=0;i<count;i++) {
            uint64_t status=pwl_resident_check_event(events[i]);
            if (status!=PWL_EFI_NOT_READY) { *index=i;return status; }
        }
        __asm__ volatile("pause" ::: "memory");
    }
}
