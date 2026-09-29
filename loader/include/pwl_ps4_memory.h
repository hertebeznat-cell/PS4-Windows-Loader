#ifndef PWL_PS4_MEMORY_H
#define PWL_PS4_MEMORY_H

#include "pwl_handoff.h"

#define PWL_PS4_VM_PAGE_SIZE UINT64_C(16384)

/* Preparation-time kernel ABI, from the pinned ps4-kexec-common/kernel.h.
 * The caller must resolve/verify these symbols for the running firmware and
 * invoke this interface in a sleepable kernel context. No guessed offsets,
 * user mmap addresses, direct-map masks or kernel patches are used here.
 */
typedef struct pwl_ps4_memory_api {
    void *kernel_map;
    void *kernel_pmap;
    uint64_t (*alloc_contig)(void *, uint64_t, int, uint64_t, uint64_t,
                             unsigned long, unsigned long, char);
    void (*free)(void *, uint64_t, uint64_t);
    uint64_t (*extract)(void *, uint64_t);
} pwl_ps4_memory_api_t;

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

/* arena must initially be zero. Exactly one non-waiting allocation attempt.
 * Every 4 KiB hardware page is checked using pmap_extract, while the original
 * 16 KiB VM allocation is kept alive. Failure frees it using its original KVA.
 * This acquires an owned RAM extent, not the platform's complete RAM/MMIO map.
 */
pwl_status_t pwl_ps4_arena_acquire(const pwl_ps4_memory_api_t *api,
                                  uint64_t size, pwl_ps4_arena_t *arena);
pwl_status_t pwl_ps4_arena_take(pwl_ps4_arena_t *arena, uint64_t size,
                               uint64_t alignment, pwl_owned_span_t *span);
/* Preparation failure/abort only. Never call after leaving the Orbis context.
 * No automatic release on ExitBootServices: the backing must remain owned.
 */
void pwl_ps4_arena_release(pwl_ps4_arena_t *arena);

#endif
