#include "pwl_ps4_memory.h"
#include "pwl_ps4_profile.h"

#if defined(PWL_PS4_MEMORY_HOST_TEST) && !__STDC_HOSTED__
#error "Host memory fixtures must never be enabled in a freestanding build"
#endif

/* Read CPL first: a host/process call never reads GS or privileged state. */
static pwl_status_t observe(pwl_ps4_observation_t *out)
{
#if defined(__x86_64__)
    uint16_t cs;uint64_t flags,thread,root;uint32_t lo,hi;
    __asm__ volatile("mov %%cs,%0":"=r"(cs)::"memory");
    if(cs&3)return PWL_ERR_UNSUPPORTED;
    __asm__ volatile("pushfq; popq %0":"=r"(flags)::"memory");
    if(!(flags&0x200) || (flags&0x400))return PWL_ERR_ACCESS_DENIED;
    __asm__ volatile("movq %%gs:0,%0":"=r"(thread));
    __asm__ volatile("movq %%cr3,%0":"=r"(root));
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000082));
    *out=(pwl_ps4_observation_t){(uint64_t)lo|((uint64_t)hi<<32),thread,root,flags,cs};
    return PWL_OK;
#else
    (void)out;return PWL_ERR_UNSUPPORTED;
#endif
}
static int same_context(const pwl_ps4_observation_t *a,const pwl_ps4_observation_t *b)
{ return a->lstar==b->lstar && a->thread==b->thread && a->root==b->root && a->cs==b->cs; }

pwl_status_t pwl_ps4_memory_bind_checked(uint32_t firmware,
    pwl_ps4_binding_context_t *context,pwl_ps4_memory_api_t *api)
{
    if(!api)return PWL_ERR_INVALID_ARGUMENT;
    *api=(pwl_ps4_memory_api_t){0};api->firmware=firmware;
    if(firmware!=PWL_PS4_FIRMWARE_1352)return PWL_ERR_UNSUPPORTED;
    pwl_ps4_observation_t before,after;
    pwl_status_t status=observe(&before);if(status!=PWL_OK)return status;
    if(!context || !context->read || context->kernel_base || context->thread || context->root)
        return PWL_ERR_INVALID_ARGUMENT;
    pwl_ps4_profile_t profile;
    status=pwl_ps4_profile_inspect(&before,context->read,context->read_context,&profile);
    if(status!=PWL_OK)return status;
    status=observe(&after);if(status!=PWL_OK)return status;
    if(!same_context(&before,&after))return PWL_ERR_ACCESS_DENIED;
    pwl_ps4_memory_api_t prepared={0};prepared.firmware=firmware;
    prepared.kernel_map=(void *)(uintptr_t)profile.map;
    prepared.kernel_pmap=(void *)(uintptr_t)profile.pmap;
    prepared.alloc_contig=(pwl_ps4_vm_u64_t (*)(void *,pwl_ps4_vm_u64_t,int,
        pwl_ps4_vm_u64_t,pwl_ps4_vm_u64_t,unsigned long,unsigned long,char))(uintptr_t)profile.alloc_contig;
    prepared.free=(void (*)(void *,pwl_ps4_vm_u64_t,pwl_ps4_vm_u64_t))(uintptr_t)profile.free;
    prepared.extract=(pwl_ps4_vm_u64_t (*)(void *,pwl_ps4_vm_u64_t))(uintptr_t)profile.extract;
    prepared.binding=context;
    context->kernel_base=profile.base;context->thread=before.thread;context->root=before.root;
    *api=prepared;return PWL_OK;
}

pwl_status_t pwl_ps4_memory_bind(uint32_t firmware, pwl_ps4_memory_api_t *api)
{
    if (api == NULL) return PWL_ERR_INVALID_ARGUMENT;
    *api = (pwl_ps4_memory_api_t){0};
    api->firmware = firmware;
    /* Firmware alone supplies neither a protected reader nor live context.
     * Use bind_checked inside the established preparation callback. */
    return PWL_ERR_UNSUPPORTED;
}

pwl_status_t pwl_ps4_memory_api_validate(const pwl_ps4_memory_api_t *api)
{
    if (api == NULL) return PWL_ERR_INVALID_ARGUMENT;
#ifdef PWL_PS4_MEMORY_HOST_TEST
    /* Compiled only into host integration tests, never the distributed core. */
    if (api->firmware == 0) {
        if (api->kernel_map == NULL || api->kernel_pmap == NULL ||
            api->alloc_contig == NULL || api->free == NULL || api->extract == NULL)
            return PWL_ERR_INVALID_ARGUMENT;
        return PWL_OK;
    }
#endif
    if(api->firmware!=PWL_PS4_FIRMWARE_1352)return PWL_ERR_UNSUPPORTED;
    pwl_ps4_observation_t before,after;
    pwl_status_t status=observe(&before);if(status!=PWL_OK)return status;
    const pwl_ps4_binding_context_t *context=api->binding;
    if(!context || !context->read || context->kernel_base!=before.lstar-0x1c0 ||
       context->thread!=before.thread || context->root!=before.root)return PWL_ERR_ACCESS_DENIED;
    pwl_ps4_profile_t profile;
    status=pwl_ps4_profile_inspect(&before,context->read,context->read_context,&profile);
    if(status!=PWL_OK)return status;
    if((uintptr_t)api->kernel_map!=profile.map || (uintptr_t)api->kernel_pmap!=profile.pmap ||
       (uintptr_t)api->alloc_contig!=profile.alloc_contig || (uintptr_t)api->free!=profile.free ||
       (uintptr_t)api->extract!=profile.extract)return PWL_ERR_BAD_IMAGE;
    status=observe(&after);if(status!=PWL_OK)return status;
    return same_context(&before,&after)?PWL_OK:PWL_ERR_ACCESS_DENIED;
}
