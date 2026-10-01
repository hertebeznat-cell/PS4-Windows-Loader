#ifndef PWL_PS4_MEMORY_H
#define PWL_PS4_MEMORY_H

#include "pwl_handoff.h"

#define PWL_PS4_VM_PAGE_SIZE UINT64_C(16384)
#define PWL_PS4_FIRMWARE_1352 1352U /* Decimal major * 100 + minor. */
#define PWL_PS4_PHYSICAL_MIN UINT64_C(0x100000)
#define PWL_PS4_IDENTITY_LIMIT (UINT64_C(1) << 47)

/* Match runtime u64 exactly, including on LP64 hosts where uint64_t is long.
 * Same-width but incompatible C function pointer types are not an ABI check.
 */
typedef unsigned long long pwl_ps4_vm_u64_t;

typedef pwl_status_t (*pwl_ps4_kernel_read_fn)(void *,uint64_t,void *,size_t);
typedef struct pwl_ps4_binding_context {
    /* Platform protected reader, code/context and bounce storage remain
     * resident for every allocation/release. Never a plain candidate memcpy. */
    pwl_ps4_kernel_read_fn read;
    void *read_context;
    uint64_t kernel_base, thread, root;
} pwl_ps4_binding_context_t;

/* Preparation-time kernel ABI, from the pinned ps4-kexec-common/kernel.h.
 * Checked binding supports the exact observed 13.52 build in its established
 * preparation callback. Non-null pointers alone do not authorize calls. The
 * platform must retain a sleepable context and pinned reader/code/state;
 * M_NOWAIT does not remove internal VM locks. No user mmap address is treated
 * as a physical allocation, and no kernel patch is applied by this backend.
 */
typedef struct pwl_ps4_memory_api {
    void *kernel_map;
    void *kernel_pmap;
    pwl_ps4_vm_u64_t (*alloc_contig)(void *, pwl_ps4_vm_u64_t, int,
                             pwl_ps4_vm_u64_t, pwl_ps4_vm_u64_t,
                             unsigned long, unsigned long, char);
    void (*free)(void *, pwl_ps4_vm_u64_t, pwl_ps4_vm_u64_t);
    pwl_ps4_vm_u64_t (*extract)(void *, pwl_ps4_vm_u64_t);
    uint32_t firmware; /* 0 is reserved for host fixtures, never a PS4 fallback. */
    pwl_ps4_binding_context_t *binding;
} pwl_ps4_memory_api_t;

/* Clears output and refuses unsupported/unverified firmware WITHOUT reading
 * kernel memory, deriving a base, or resolving/calling candidate functions.
 * This version-only interface intentionally cannot activate the checked path.
 * Do not rebind an API that owns a live arena; the arena retains its own copy.
 */
pwl_status_t pwl_ps4_memory_bind(uint32_t firmware, pwl_ps4_memory_api_t *api);
/* Same-build 13.52 binding in the actual kernel callback. Captures live CPU,
 * LSTAR/thread/root; protected-reads immutable signatures and mutable map,
 * root/counter state before converting symbols to calls. Context starts with
 * only reader fields set; failure preserves it and clears api. The reader's
 * bootstrap, residency and fault recovery are platform contracts. This does
 * not enable a CPU switch, provide inventory or perform any allocation. */
pwl_status_t pwl_ps4_memory_bind_checked(uint32_t firmware,
    pwl_ps4_binding_context_t *context,pwl_ps4_memory_api_t *api);
pwl_status_t pwl_ps4_memory_api_validate(const pwl_ps4_memory_api_t *api);

typedef struct pwl_ps4_arena {
    pwl_ps4_memory_api_t api;
    uint64_t kernel_address; /* Original allocation address, also used to free. */
    uint64_t physical_address;
    uint64_t size;
    uint64_t used;
} pwl_ps4_arena_t;

typedef struct pwl_owned_span {
    void *prepare_address; /* KVA: valid only while preparing under Orbis. */
    uint64_t physical_address; /* Identity address in the future EFI context. */
    uint64_t size;
} pwl_owned_span_t;

/* arena must initially be zero and must not be copied while owning memory.
 * Exactly one M_NOWAIT allocation attempt (not a lock-free callback).
 * Every 4 KiB hardware page is checked using pmap_extract, while the original
 * 16 KiB VM allocation is kept alive. Bad PA results revalidate the binding
 * before freeing a valid original KVA; refusal retains ownership and propagates
 * the cleanup error so the caller keeps reader/callback storage pinned.
 * a malformed returned KVA is retained and never passed to guessed cleanup.
 * Physical backing below 1 MiB is excluded. This acquires an owned RAM extent,
 * not the platform's complete RAM/MMIO map.
 */
pwl_status_t pwl_ps4_arena_acquire(const pwl_ps4_memory_api_t *api,
                                  uint64_t size, pwl_ps4_arena_t *arena);
pwl_status_t pwl_ps4_arena_take(pwl_ps4_arena_t *arena, uint64_t size,
                               uint64_t alignment, pwl_owned_span_t *span);
/* Preparation failure/abort only. Never call after leaving the Orbis context.
 * No automatic release on ExitBootServices: the backing must remain owned.
 */
/* Returns an error without losing the owner if the binding/owner is invalid.
 * kmem_free itself returns void: PWL_OK means it returned, not an independent
 * hardware proof of deallocation. An empty owner is an idempotent success.
 */
pwl_status_t pwl_ps4_arena_release(pwl_ps4_arena_t *arena);

#endif
