/* Compile-only against the pinned upstream header; never link kernel.c.
 * TESTING suppresses upstream's private libc typedefs, not its kernel ABI.
 */
#include "pwl_ps4_memory.h"
#define TESTING
#undef offsetof
#include "kernel.h"

#define SAME(member, upstream) \
    _Static_assert(__builtin_types_compatible_p( \
        __typeof__(((pwl_ps4_memory_api_t *)0)->member), \
        __typeof__(((struct ksym_t *)0)->upstream)), "ABI mismatch: " #member)
SAME(alloc_contig, kmem_alloc_contig);
SAME(free, kmem_free);
SAME(extract, pmap_extract);
SAME(kernel_pmap, kernel_pmap_store);
_Static_assert(__builtin_types_compatible_p(
    __typeof__(((pwl_ps4_memory_api_t *)0)->kernel_map),
    __typeof__(*(((struct ksym_t *)0)->kernel_map))), "kernel_map must be dereferenced once");
_Static_assert(sizeof(vm_offset_t) == 8 && sizeof(vm_paddr_t) == 8 &&
    sizeof(vm_size_t) == 8 && sizeof(unsigned long) == 8 &&
    sizeof(vm_memattr_t) == 1, "PS4 x86-64 data model required");
_Static_assert(PAGE_SIZE == PWL_PS4_VM_PAGE_SIZE, "VM page size mismatch");
