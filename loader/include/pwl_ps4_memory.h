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

/* Preparation-time kernel ABI, from the pinned ps4-kexec-common/kernel.h.
 * No production firmware binding is currently approved. Non-null pointers
 * alone do not authorize calls. A future verified adapter must also establish
 * a sleepable kernel context (M_NOWAIT does not remove internal VM locks).
 * No guessed offsets,
 * user mmap addresses, direct-map masks or kernel patches are used here.
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
} pwl_ps4_memory_api_t;

/* Clears output and refuses unsupported/unverified firmware WITHOUT reading
 * kernel memory, deriving a base, or resolving/calling candidate functions.
 * 13.52 remains unsupported until the evidence in PS4_1352_BINDING.md exists.
 * Do not rebind an API that owns a live arena; the arena retains its own copy.
 */
pwl_status_t pwl_ps4_memory_bind(uint32_t firmware, pwl_ps4_memory_api_t *api);
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
 * 16 KiB VM allocation is kept alive. Failure frees it using its original KVA.
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
