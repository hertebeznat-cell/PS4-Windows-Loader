#include "pwl_platform_call.h"
static int privileged(void)
{uint16_t cs;__asm__ volatile("mov %%cs,%0":"=r"(cs)::"memory");return !(cs&3);}
static int backend(const pwl_platform_backend_t *b)
{return b && b->claim && b->freeze && b->thaw && b->release &&
 b->devices.read16 && b->devices.write16 && b->devices.stop && b->devices.drain && b->devices.resume;}
pwl_status_t pwl_native_platform_cleanup(const pwl_platform_backend_t *b,pwl_platform_call_state_t *s)
{
    if(!privileged())return PWL_ERR_UNSUPPORTED;
    if(!backend(b) || !s)return PWL_ERR_INVALID_ARGUMENT;
    if(s->context_changed || s->transferred)return PWL_ERR_ACCESS_DENIED;
    pwl_status_t r=PWL_OK;
    if(s->devices.count) {r=pwl_devices_restore(&b->devices,&s->devices);if(r!=PWL_OK)goto done;}
    if(s->frozen) {r=b->thaw(b->context);if(r!=PWL_OK)goto done;s->frozen=0;}
    if(s->devices.count) {r=pwl_devices_release(&b->devices,&s->devices);if(r!=PWL_OK)goto done;}
    if(s->claimed) {r=b->release(b->context);if(r==PWL_OK)s->claimed=0;}
done:s->cleanup_status=r;return r;
}
pwl_status_t pwl_native_platform_call(const pwl_platform_backend_t *b,const uint64_t *ids,size_t n,
    pwl_native_call_t *call,const pwl_fw_memory_t *memory,pwl_platform_call_state_t *s)
{
    if(!privileged())return PWL_ERR_UNSUPPORTED;
    if(!backend(b) || !ids || !n || n>PWL_DEVICE_MAX || !call || !memory || !s ||
       memory->exited || s->claimed || s->frozen || s->executed || s->devices.count ||
       s->context_changed || s->transferred)return PWL_ERR_INVALID_ARGUMENT;
    /* Privilege is real, but the complete ownership contract still belongs
     * to these target hooks. Never supply a generic successful default. */
    s->claimed=1;pwl_status_t r=b->claim(b->context);if(r!=PWL_OK)goto cleanup;
    r=pwl_devices_quiesce(&b->devices,ids,n,&s->devices);if(r!=PWL_OK)goto cleanup;
    s->frozen=1;r=b->freeze(b->context);if(r!=PWL_OK)goto cleanup;
    r=pwl_x64_cpu_environment_read(&s->before);if(r!=PWL_OK)goto cleanup;
    /* A frozen environment has interrupts disabled and direction clear. */
    if(s->before.flags&0x600) {r=PWL_ERR_ACCESS_DENIED;goto cleanup;}
    if(call->expected.cr0!=s->before.cpu.cr0 || call->expected.cr3!=s->before.cpu.cr3 ||
       call->expected.cr4!=s->before.cpu.cr4 || call->expected.efer!=s->before.cpu.efer ||
       call->fp_kind!=s->before.fp.kind || call->fp_mask!=s->before.fp.mask ||
       call->fp_bytes!=s->before.fp.bytes) {r=PWL_ERR_ACCESS_DENIED;goto cleanup;}
    s->executed=1;r=(pwl_status_t)pwl_x64_native_call(call);
    pwl_status_t observed=pwl_x64_cpu_environment_read(&s->after);
    if(observed!=PWL_OK || pwl_cpu_environment_equal(&s->before,&s->after)!=PWL_OK) {
        s->context_changed=1;r=observed==PWL_OK?PWL_ERR_BAD_IMAGE:observed;
    }
    if(memory->exited) {s->transferred=1;r=PWL_ERR_ACCESS_DENIED;}
    if(r==PWL_OK)r=pwl_native_call_result_validate(call);
cleanup:
    s->operation_status=r;
    pwl_status_t undo=pwl_native_platform_cleanup(b,s);
    return undo==PWL_OK?r:undo;
}
